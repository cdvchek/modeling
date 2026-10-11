#include "test.hpp"
#include "engine/jobs/job_system.hpp"

#include <atomic>
#include <string>

namespace {
    // Holds the one worker of a single-worker system inside a job until released
    struct Gate {
        std::atomic<bool> entered { false };
        std::atomic<bool> open { false };

        void block(JobSystem& jobs) {
            jobs.run([this] {
                entered = true;
                while (!open) std::this_thread::yield();
            });
            while (!entered) std::this_thread::yield();
        }
    };
}

TEST_CASE(jobs_each_run_once) {
    JobSystem jobs(4);
    CHECK(jobs.workerCount() == 4);

    std::atomic<u32> sum { 0 };
    for (u32 i = 1; i <= 1000; ++i) jobs.run([&sum, i] { sum += i; });
    jobs.waitUntilIdle();
    CHECK(sum == 500500);

    const JobStats stats = jobs.stats();
    CHECK(stats.workers == 4);
    CHECK(stats.completed == 1000);
    CHECK(stats.running == 0);
    CHECK(stats.queued[0] == 0 && stats.queued[1] == 0 && stats.queued[2] == 0);
}

TEST_CASE(jobs_know_which_worker_they_are_on) {
    JobSystem jobs(3);
    CHECK(JobSystem::currentWorker() == -1);

    std::atomic<u32> inRange { 0 };
    for (u32 i = 0; i < 200; ++i) {
        jobs.run([&] {
            const i32 worker = JobSystem::currentWorker();
            if (worker >= 0 && worker < 3) ++inRange;
        });
    }
    jobs.waitUntilIdle();
    CHECK(inRange == 200);
}

TEST_CASE(jobs_run_by_priority_then_in_order) {
    JobSystem jobs(1);
    Gate gate;
    gate.block(jobs);

    // Queued while the worker is busy, out of order
    std::string order;
    jobs.run([&] { order += "low "; }, JobPriority::Low);
    jobs.run([&] { order += "normal1 "; });
    jobs.run([&] { order += "high "; }, JobPriority::High);
    jobs.run([&] { order += "normal2 "; }, JobPriority::Normal);

    const JobStats waiting = jobs.stats();
    CHECK(waiting.running == 1);
    CHECK(waiting.queued[static_cast<u32>(JobPriority::High)] == 1);
    CHECK(waiting.queued[static_cast<u32>(JobPriority::Normal)] == 2);
    CHECK(waiting.queued[static_cast<u32>(JobPriority::Low)] == 1);

    gate.open = true;
    jobs.waitUntilIdle();
    CHECK(order == "high normal1 normal2 low ");
}

TEST_CASE(job_group_waits_for_its_own_jobs) {
    JobSystem jobs(4);
    JobGroup group(jobs);

    std::atomic<u32> count { 0 };
    for (u32 i = 0; i < 300; ++i) group.run([&count] { ++count; });
    group.wait();
    CHECK(count == 300);

    // A group can be used again, and an empty wait returns at once
    group.wait();
    for (u32 i = 0; i < 50; ++i) group.run([&count] { ++count; }, JobPriority::High);
    group.wait();
    CHECK(count == 350);
}

TEST_CASE(job_group_waited_on_from_a_worker_does_not_hang) {
    // Every worker waits on jobs that only workers can run
    JobSystem jobs(2);
    std::atomic<u32> inner { 0 };

    JobGroup outer(jobs);
    for (u32 i = 0; i < 8; ++i) {
        outer.run([&] {
            JobGroup group(jobs);
            for (u32 j = 0; j < 20; ++j) group.run([&inner] { ++inner; });
            group.wait();
        });
    }
    outer.wait();
    CHECK(inner == 160);
}

TEST_CASE(parallel_for_visits_every_index_once) {
    JobSystem jobs(4);

    for (u32 count : { 0u, 1u, 7u, 64u, 1000u, 1001u }) {
        for (u32 grain : { 0u, 1u, 3u, 64u, 5000u }) {
            std::vector<std::atomic<u32>> visits(count);
            jobs.parallelFor(count, grain, [&](u32 begin, u32 end) {
                for (u32 i = begin; i < end; ++i) ++visits[i];
            });

            bool once = true;
            for (const std::atomic<u32>& visit : visits) once = once && visit == 1;
            CHECK(once);
        }
    }
}

TEST_CASE(parallel_for_returns_while_the_workers_are_busy) {
    // The one worker never gets to help; the caller does every piece and doesn't wait for it
    JobSystem jobs(1);
    Gate gate;
    gate.block(jobs);

    u32 sum = 0;
    jobs.parallelFor(100, 10, [&](u32 begin, u32 end) {
        for (u32 i = begin; i < end; ++i) sum += i;
    });
    CHECK(sum == 4950);

    // The helper that was queued finds nothing left to do once the worker is free
    gate.open = true;
    jobs.waitUntilIdle();
    CHECK(sum == 4950);
}

TEST_CASE(parallel_for_nests_inside_jobs) {
    JobSystem jobs(3);
    std::atomic<u64> total { 0 };

    jobs.parallelFor(16, 1, [&](u32 begin, u32 end) {
        for (u32 outer = begin; outer < end; ++outer) {
            jobs.parallelFor(100, 7, [&](u32 innerBegin, u32 innerEnd) {
                for (u32 i = innerBegin; i < innerEnd; ++i) total += i;
            });
        }
    });
    CHECK(total == 16u * 4950u);
}

TEST_CASE(main_tasks_wait_for_the_main_thread) {
    JobSystem jobs(2);
    u32 ran = 0;

    for (u32 i = 0; i < 10; ++i) jobs.run([&] { jobs.runOnMain([&ran] { ++ran; }); });
    jobs.waitUntilIdle();

    // Queued by the workers, but nothing runs until the main thread asks
    CHECK(ran == 0);
    CHECK(jobs.stats().mainTasks == 10);
    CHECK(jobs.runMainTasks() == 10);
    CHECK(ran == 10);
    CHECK(jobs.runMainTasks() == 0);
}

TEST_CASE(jobs_without_workers_run_where_they_are_queued) {
    JobSystem jobs(0);
    CHECK(jobs.workerCount() == 0);

    u32 ran = 0;
    jobs.run([&] { ++ran; });
    CHECK(ran == 1);

    JobGroup group(jobs);
    group.run([&] { ++ran; });
    group.wait();
    CHECK(ran == 2);

    u32 sum = 0;
    jobs.parallelFor(10, 3, [&](u32 begin, u32 end) { for (u32 i = begin; i < end; ++i) sum += i; });
    CHECK(sum == 45);
    jobs.waitUntilIdle();
    CHECK(jobs.stats().completed == 2);
}

TEST_CASE(jobs_still_queued_run_before_the_system_stops) {
    std::atomic<u32> ran { 0 };
    {
        JobSystem jobs(1);
        Gate gate;
        gate.block(jobs);
        for (u32 i = 0; i < 100; ++i) jobs.run([&ran] { ++ran; });
        gate.open = true;
    }
    CHECK(ran == 100);
}
