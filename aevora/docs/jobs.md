# Jobs

Worker threads that run queued pieces of work, so anything heavy (generating terrain, building meshes, loading) can be spread across the machine's cores and kept off the main thread.

Files: [job_system.hpp](../engine/jobs/job_system.hpp), [job_system.cpp](../engine/jobs/job_system.cpp)

## The pieces

| Piece | Job |
|---|---|
| `JobSystem` | Owns the worker threads and the queues. The editor has one, `ctx.jobs`. |
| `JobGroup` | A set of jobs that are waited for together. |
| `JobPriority` | `High`, `Normal` (the default), or `Low`: which queued job a free worker takes first. |
| `JobStats` | A snapshot for debugging: workers, jobs queued at each priority, jobs running, jobs completed, tasks waiting for the main thread. |

A job is a `std::function<void()>`. Jobs must not throw.

## JobSystem

| Function | Description |
|---|---|
| `JobSystem(workerCount)` | Starts the workers. The default, `defaultWorkerCount()`, is one per hardware thread, less one for the main thread. With 0 workers every job runs at once on the thread that queues it, which is the way to run everything on one thread when hunting a bug. |
| `~JobSystem()` | Runs everything still queued, then stops the workers. |
| `run(job, priority)` | Queues a job and returns at once. |
| `parallelFor(count, grain, body, priority)` | Calls `body(begin, end)` over 0 to `count` in pieces of `grain` items, on the workers and the calling thread, and returns when every piece is done. |
| `runOnMain(task)` | Queues a task for the main thread; any thread can call it. |
| `runMainTasks()` | Runs the tasks queued so far and returns how many. The main thread calls it once a frame (the editor does, in `Editor::update`). |
| `waitUntilIdle()` | Blocks until nothing is queued or running. Tasks waiting for the main thread don't count. |
| `stats()` | The current `JobStats`. |
| `workerCount()` | How many workers there are. |
| `currentWorker()` | Static: the index of the worker thread calling it (0 to `workerCount() - 1`), or -1 on any other thread. For per-worker scratch space. |

**Order:** a free worker takes the oldest job of the highest priority that has any waiting. Jobs of one priority start in the order they were queued, but with more than one worker they finish in any order, so a job's result must never depend on which other jobs ran first.

**Handing results to the main thread:** a job that finishes something only the main thread may touch (a GPU buffer, the scene) ends by calling `runOnMain` with the last step:

```cpp
jobs.run([&jobs, chunk] {
    Mesh mesh = buildMesh(chunk);                          // on a worker
    jobs.runOnMain([mesh = std::move(mesh), chunk] {       // next frame, on the main thread
        upload(chunk, mesh);
    });
});
```

## parallelFor

```cpp
jobs.parallelFor(cells.size(), 256, [&](u32 begin, u32 end) {
    for (u32 i = begin; i < end; ++i) cells[i] = generate(i);
});
```

- The calling thread takes pieces too, so it's safe inside a job: a worker that calls it never just sits waiting.
- It doesn't wait for the workers to become free. If they're all busy, the caller does every piece itself and returns; the helpers it queued find nothing left when they finally start.
- `grain` is how many items one claim covers. Use a large one for cheap items, so claiming isn't most of the work, and 1 for items that are heavy or uneven. A `grain` of 0 counts as 1.
- With one piece, or no workers, `body(0, count)` just runs on the caller.

## JobGroup

```cpp
JobGroup group(jobs);
for (Chunk& chunk : chunks) group.run([&chunk] { generate(chunk); });
group.wait();
```

- `run(job, priority)` queues a job through the group.
- `wait()` returns when every job run through the group has finished. The group can then be used again.
- The destructor waits, so a group never goes away while its jobs still refer to it. The group must outlive them.
- Waiting on the main thread blocks. Waiting **on a worker** runs other queued jobs meanwhile, so workers that are all waiting on each other's jobs can't lock up.

## How it works

One mutex guards three queues, one per priority. Workers sleep on a condition variable and wake when a job is queued. Counts of running and completed jobs are kept under the same mutex, which is what `stats` and `waitUntilIdle` read.

That single lock is the simplest thing that's correct. If it ever shows up when profiling (many tiny jobs), each worker can get its own queue with stealing behind the same interface; `parallelFor` with a sensible `grain` is the first thing to try before that.

## In the editor

`ctx.jobs` is made with the default worker count. The `jobs` console command prints `stats()`:

```
23 workers, 0 running
Queued: 0 high, 0 normal, 0 low
Completed: 0
Waiting for the main thread: 0
```

Nothing in the editor queues jobs yet; the planet generator will be the first user.

## Tests

[job_tests.cpp](../tests/job_tests.cpp); what it covers is in [testing.md](testing.md).
