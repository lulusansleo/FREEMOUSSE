#include "core/audio_state.hpp"

namespace dv {

void SharedAudioState::write(const AudioState& s) noexcept
{
    int next = 1 - m_writeIdx.load(std::memory_order_relaxed);
    m_buf[next] = s;
    m_writeIdx.store(next, std::memory_order_release);
}

AudioState SharedAudioState::read() const noexcept
{
    return m_buf[m_writeIdx.load(std::memory_order_acquire)];
}

} // namespace dv
