#pragma once

#include <types>

// Averages frame rate over a short window so the displayed value is steady
class FrameTimer {
public:
    static constexpr f64 UPDATE_INTERVAL = 0.5;

    void tick(f64 nowSeconds);
    f32 getFps() const { return m_fps; }

private:
    bool m_started = false;
    f64 m_windowStart = 0.0;
    u32 m_framesInWindow = 0;
    f32 m_fps = 0.0f;
};
