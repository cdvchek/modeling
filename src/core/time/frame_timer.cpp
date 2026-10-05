#include "core/time/frame_timer.hpp"

void FrameTimer::tick(f64 nowSeconds) {
    if (!m_started) {
        m_started = true;
        m_windowStart = nowSeconds;
        return;
    }

    ++m_framesInWindow;

    const f64 elapsed = nowSeconds - m_windowStart;
    if (elapsed >= UPDATE_INTERVAL) {
        m_fps = static_cast<f32>(m_framesInWindow / elapsed);
        m_framesInWindow = 0;
        m_windowStart = nowSeconds;
    }
}
