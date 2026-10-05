#include "Profiler.h"

#include <cassert>
#include <utility>

Profiler& Profiler::Get()
{
    static Profiler instance;
    return instance;
}

double Profiler::NowMs() const
{
    return std::chrono::duration<double, std::milli>(Clock::now() - m_frameStart).count();
}

void Profiler::BeginFrame()
{
    m_frameStart = Clock::now();
    m_current.index = m_frameIndex++;
    m_current.zones.clear();
    m_openZones.clear();
}

void Profiler::EndFrame()
{
    assert(m_openZones.empty() && "PROFILE_SCOPE still open at the end of the frame");
    while (!m_openZones.empty())
        EndZone();

    m_current.durationMs = NowMs();
    m_lastFrameMs = m_current.durationMs;

    if (m_paused)
        return;

    // Swapping instead of copying reuses the oldest frame's zone vector, so we
    // stop allocating once the history is full.
    std::swap(m_history[m_head], m_current);
    m_head = (m_head + 1) % kHistorySize;
    if (m_count < kHistorySize)
        ++m_count;
}

void Profiler::BeginZone(const char* name)
{
    const int parent = m_openZones.empty() ? -1 : m_openZones.back();
    const int depth = static_cast<int>(m_openZones.size());
    const double now = NowMs();

    m_openZones.push_back(static_cast<int>(m_current.zones.size()));
    m_current.zones.push_back({name, depth, parent, now, now});
}

void Profiler::EndZone()
{
    assert(!m_openZones.empty());
    m_current.zones[m_openZones.back()].endMs = NowMs();
    m_openZones.pop_back();
}

const Profiler::Frame& Profiler::GetFrame(int age) const
{
    assert(age >= 0 && age < m_count);
    const int index = (m_head - 1 - age + kHistorySize) % kHistorySize;
    return m_history[index];
}
