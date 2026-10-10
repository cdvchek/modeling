#pragma once

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>
#include <types>

// Runs work(i) for every i below count, spread over the machine's cores; the calling thread works too.
// Items are handed out one at a time, so uneven items (a huge mesh next to small ones) still balance.
template <typename Work>
void parallelFor(u32 count, Work&& work) {
    const u32 threads = std::min(count, std::max(1u, std::thread::hardware_concurrency()));

    if (threads <= 1) {
        for (u32 i = 0; i < count; ++i) work(i);
        return;
    }

    std::atomic<u32> next { 0 };
    auto loop = [&] {
        for (u32 i = next.fetch_add(1); i < count; i = next.fetch_add(1)) work(i);
    };

    // jthreads join when the vector goes out of scope
    std::vector<std::jthread> helpers;
    helpers.reserve(threads - 1);
    for (u32 t = 1; t < threads; ++t) helpers.emplace_back(loop);
    loop();
}
