#pragma once

#include <types>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// Which queued job a free worker takes first
enum class JobPriority : u8 {
    High,
    Normal,
    Low,
    Count
};

struct JobStats {
    u32 workers = 0;
    u32 queued[static_cast<u32>(JobPriority::Count)] = {};
    u32 running = 0;
    u64 completed = 0;
    u32 mainTasks = 0;
};

// Worker threads that run queued jobs. Jobs must not throw.
class JobSystem {
public:
    // One worker per hardware thread, leaving one for the main thread
    static u32 defaultWorkerCount();

    // With no workers every job runs at once on the thread that queues it
    explicit JobSystem(u32 workerCount = defaultWorkerCount());
    // Runs everything still queued, then stops the workers
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    u32 workerCount() const { return static_cast<u32>(m_workers.size()); }

    void run(std::function<void()> job, JobPriority priority = JobPriority::Normal);

    // Calls body(begin, end) over 0 to count in pieces of grain; the calling thread works too, and it returns when all are done
    void parallelFor(u32 count, u32 grain, const std::function<void(u32 begin, u32 end)>& body, JobPriority priority = JobPriority::Normal);

    // Queues a task for the main thread from any thread; runMainTasks runs them (call it once a frame) and returns how many
    void runOnMain(std::function<void()> task);
    u32 runMainTasks();

    // Blocks until nothing is queued or running; main tasks don't count
    void waitUntilIdle();

    JobStats stats() const;

    // The index of the worker thread calling this, or -1 on any other thread
    static i32 currentWorker();

private:
    friend class JobGroup;

    void workerLoop(u32 index);
    // Runs one queued job on the calling thread; false when there was none
    bool runOne();
    bool takeJob(std::function<void()>& job);
    void finishJob();

    std::vector<std::thread> m_workers;

    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    std::deque<std::function<void()>> m_queues[static_cast<u32>(JobPriority::Count)];
    u32 m_running = 0;
    u64 m_completed = 0;
    bool m_stopping = false;

    mutable std::mutex m_mainMutex;
    std::vector<std::function<void()>> m_mainTasks;
};

// Jobs that are waited for together. The group must outlive its jobs; its destructor waits.
class JobGroup {
public:
    explicit JobGroup(JobSystem& jobs) : m_jobs(jobs) {}
    ~JobGroup() { wait(); }

    JobGroup(const JobGroup&) = delete;
    JobGroup& operator=(const JobGroup&) = delete;

    void run(std::function<void()> job, JobPriority priority = JobPriority::Normal);

    // Returns when every job run through the group has finished; a worker that waits runs other jobs meanwhile
    void wait();

private:
    JobSystem& m_jobs;
    std::mutex m_mutex;
    std::condition_variable m_done;
    u32 m_pending = 0;
};
