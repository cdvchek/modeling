#pragma once

#include <types>

// What the renderer did in one frame, for the stats readout (stats command)
struct RenderStats {
    u32 drawCalls = 0;
    u64 triangles = 0;
    u64 lines = 0;
    u64 points = 0;
    u32 uniformUploads = 0;     // shader values set (each call to a set function)
    u32 meshUploads = 0;        // meshes whose GPU buffers were rebuilt
    u64 meshUploadBytes = 0;
    u32 meshPatches = 0;        // meshes brought up to date in place, after vertices only moved
    u64 meshPatchBytes = 0;
    f32 gpuMilliseconds = -1.0f;    // GPU time for the frame, a few frames late; -1 until one is known
};
