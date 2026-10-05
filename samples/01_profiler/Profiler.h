#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

// Minimal instrumentation profiler: zones are recorded with PROFILE_SCOPE and
// every completed frame is kept in a fixed-size history.
class Profiler
{
public:
    struct Zone
    {
        const char* name;
        int depth;
        int parent;      // index in Frame::zones, -1 for top-level zones
        double startMs;  // relative to the frame start
        double endMs;

        double DurationMs() const { return endMs - startMs; }
    };

    struct Frame
    {
        uint64_t index = 0;
        double durationMs = 0.0;
        std::vector<Zone> zones;
    };

    static constexpr int kHistorySize = 600;

    static Profiler& Get();

    void BeginFrame();
    void EndFrame();

    void BeginZone(const char* name);
    void EndZone();

    bool IsPaused() const { return m_paused; }
    void SetPaused(bool paused) { m_paused = paused; }

    int FrameCount() const { return m_count; }
    // age 0 is the most recent completed frame
    const Frame& GetFrame(int age) const;

    // Duration of the last frame, even when paused
    double LastFrameMs() const { return m_lastFrameMs; }

private:
    using Clock = std::chrono::steady_clock;

    double NowMs() const;

    std::array<Frame, kHistorySize> m_history;
    int m_head = 0;
    int m_count = 0;

    Frame m_current;
    std::vector<int> m_openZones;
    Clock::time_point m_frameStart;
    uint64_t m_frameIndex = 0;
    double m_lastFrameMs = 0.0;
    bool m_paused = false;
};

class ProfileScope
{
public:
    explicit ProfileScope(const char* name) { Profiler::Get().BeginZone(name); }
    ~ProfileScope() { Profiler::Get().EndZone(); }

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};

#define PROFILE_CONCAT_IMPL(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_IMPL(a, b)
#define PROFILE_SCOPE(name) ProfileScope PROFILE_CONCAT(profileScope_, __LINE__)(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__func__)
