# Testing

Aevora's tests cover its engine library. They run as `build/aevora_tests.exe`, which links `aevora_engine`, so the editor itself isn't covered. The runner, how to run it, and how to write a test are on the [shared testing page](../../shared/docs/testing.md).

Files: `aevora/tests/`

## Coverage

| File | Covers |
|---|---|
| `aevora/tests/project_tests.cpp` | Aevora project files: the text round-trips with quotes and backslashes in the name; comments, blank lines, Windows line ends, and unknown keys are skipped; refusals with their messages (not a project, a newer version, no name, a name outside quotes or with a stray quote, a line with no `=`) leave the project untouched; `create` makes the folder, its `scenes`, `data`, and `assets` folders, and a file named after it, leaves no temporary file, keeps what was already in the folder, and refuses a folder that already holds a project; `save` and `load` follow the file, and a missing file says so |
| `aevora/tests/job_tests.cpp` | The job system: 1,000 jobs each run once and the stats count them; jobs see a worker index in range and other threads see -1; with the one worker held busy, queued jobs run high, then normal in the order queued, then low; a group waits for its own jobs, can be used again, and returns at once when empty; workers that all wait on groups of their own run each other's jobs instead of locking up; `parallelFor` visits every index exactly once for counts and grains around the edges (0, 1, odd sizes, a grain larger than the count), returns without the workers when they're busy, and nests inside itself; tasks for the main thread wait until it asks and run once; with no workers everything runs where it's queued; jobs still queued run before the system stops |
