#include "engine/jobs/job_system.hpp"

#include <algorithm>
#include <atomic>
#include <memory>

namespace {
    thread_local i32 workerIndex = -1;
}

u32 JobSystem::defaultWorkerCount() {
    return std::max(1u, std::thread::hardware_concurrency()) - 1;
}

i32 JobSystem::currentWorker() {
    return workerIndex;
}

JobSystem::JobSystem(u32 workerCount) {
    m_workers.reserve(workerCount);
    for (u32 i = 0; i < workerCount; ++i) m_workers.emplace_back([this, i] { workerLoop(i); });
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
    }
    m_wake.notify_all();
    for (std::thread& worker : m_workers) worker.join();
}

void JobSystem::run(std::function<void()> job, JobPriority priority) {
    if (m_workers.empty()) {
        job();
        std::lock_guard lock(m_mutex);
        ++m_completed;
        return;
    }

    {
        std::lock_guard lock(m_mutex);
        m_queues[static_cast<u32>(priority)].push_back(std::move(job));
    }
    m_wake.notify_one();
}

// Takes the oldest job of the highest priority waiting; the caller holds the lock
bool JobSystem::takeJob(std::function<void()>& job) {
    for (std::deque<std::function<void()>>& queue : m_queues) {
        if (queue.empty()) continue;
        job = std::move(queue.front());
        queue.pop_front();
        ++m_running;
        return true;
    }
    return false;
}

void JobSystem::finishJob() {
    std::lock_guard lock(m_mutex);
    --m_running;
    ++m_completed;

    bool queued = false;
    for (const std::deque<std::function<void()>>& queue : m_queues) queued = queued || !queue.empty();
    if (!queued && m_running == 0) m_idle.notify_all();
}

void JobSystem::workerLoop(u32 index) {
    workerIndex = static_cast<i32>(index);

    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(m_mutex);
            // Stopping still runs what's queued; a worker leaves only when there's nothing left
            m_wake.wait(lock, [&] { return takeJob(job) || m_stopping; });
            if (!job) return;
        }
        job();
        finishJob();
    }
}

bool JobSystem::runOne() {
    std::function<void()> job;
    {
        std::lock_guard lock(m_mutex);
        if (!takeJob(job)) return false;
    }
    job();
    finishJob();
    return true;
}

void JobSystem::parallelFor(u32 count, u32 grain, const std::function<void(u32 begin, u32 end)>& body, JobPriority priority) {
    if (count == 0) return;
    grain = std::max(grain, 1u);
    const u32 pieces = (count + grain - 1) / grain;
    if (pieces == 1 || m_workers.empty()) {
        body(0, count);
        return;
    }

    // Shared with the helpers, which may start after this call has returned and must then find nothing to do
    struct Shared {
        std::atomic<u32> next { 0 };
        std::mutex mutex;
        std::condition_variable done;
        u32 working = 0;
    };
    const std::shared_ptr<Shared> shared = std::make_shared<Shared>();

    // body is only called for a piece claimed while this call is still waiting, so the reference stays good
    const auto work = [shared, pieces, grain, count, &body] {
        {
            std::lock_guard lock(shared->mutex);
            ++shared->working;
        }
        for (u32 piece = shared->next.fetch_add(1); piece < pieces; piece = shared->next.fetch_add(1)) {
            const u32 begin = piece * grain;
            body(begin, std::min(count, begin + grain));
        }
        std::lock_guard lock(shared->mutex);
        if (--shared->working == 0) shared->done.notify_all();
    };

    const u32 helpers = std::min(workerCount(), pieces - 1);
    for (u32 i = 0; i < helpers; ++i) run(work, priority);
    work();

    // Every piece is claimed by now; wait for the helpers still inside one
    std::unique_lock lock(shared->mutex);
    shared->done.wait(lock, [&] { return shared->working == 0; });
}

void JobSystem::runOnMain(std::function<void()> task) {
    std::lock_guard lock(m_mainMutex);
    m_mainTasks.push_back(std::move(task));
}

u32 JobSystem::runMainTasks() {
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard lock(m_mainMutex);
        tasks.swap(m_mainTasks);
    }
    for (std::function<void()>& task : tasks) task();
    return static_cast<u32>(tasks.size());
}

void JobSystem::waitUntilIdle() {
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [&] {
        for (const std::deque<std::function<void()>>& queue : m_queues) {
            if (!queue.empty()) return false;
        }
        return m_running == 0;
    });
}

JobStats JobSystem::stats() const {
    JobStats result;
    result.workers = workerCount();
    {
        std::lock_guard lock(m_mutex);
        for (u32 i = 0; i < static_cast<u32>(JobPriority::Count); ++i) result.queued[i] = static_cast<u32>(m_queues[i].size());
        result.running = m_running;
        result.completed = m_completed;
    }
    std::lock_guard lock(m_mainMutex);
    result.mainTasks = static_cast<u32>(m_mainTasks.size());
    return result;
}

void JobGroup::run(std::function<void()> job, JobPriority priority) {
    {
        std::lock_guard lock(m_mutex);
        ++m_pending;
    }
    m_jobs.run([this, job = std::move(job)] {
        job();
        // Notified under the lock: once it's released the waiter may destroy the group
        std::lock_guard lock(m_mutex);
        if (--m_pending == 0) m_done.notify_all();
    }, priority);
}

void JobGroup::wait() {
    // A worker that only blocked could leave every worker waiting on jobs none of them is running
    if (JobSystem::currentWorker() >= 0) {
        for (;;) {
            {
                std::lock_guard lock(m_mutex);
                if (m_pending == 0) return;
            }
            if (!m_jobs.runOne()) std::this_thread::yield();
        }
    }

    std::unique_lock lock(m_mutex);
    m_done.wait(lock, [&] { return m_pending == 0; });
}
