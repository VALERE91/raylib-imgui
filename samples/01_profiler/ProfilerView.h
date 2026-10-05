#pragma once

#include "Profiler.h"

#include <imgui.h>

#include <string_view>
#include <unordered_map>
#include <vector>

class ProfilerView
{
public:
    void DrawOverlay();
    void DrawWindow();
    void TogglePause();

    bool showOverlay = true;
    bool showWindow = true;
    float budgetMs = 1000.0f / 60.0f;

private:
    struct ZoneStats
    {
        double totalMs = 0.0;  // summed over the history
        double maxMs = 0.0;    // worst single frame
        int frames = 0;        // frames where the zone appears
    };

    const Profiler::Frame* SelectedFrame() const;

    void DrawToolbar();
    void DrawFrameGraph();
    void DrawFlameGraph(const Profiler::Frame& frame);
    void DrawZoneTable(const Profiler::Frame& frame);
    void DrawZoneRow(const Profiler::Frame& frame, int zoneIndex);
    void UpdateZoneStats();

    int m_corner = 1;  // 0..3 = TL, TR, BL, BR, -1 = free
    bool m_followLatest = true;
    uint64_t m_selectedFrame = 0;

    std::unordered_map<std::string_view, ZoneStats> m_zoneStats;
    std::unordered_map<std::string_view, double> m_frameSums;

    // Scratch buffers for the frame graph, kept around to avoid allocating every frame
    std::vector<double> m_xs;
    std::vector<double> m_frameMs;
    std::vector<std::vector<double>> m_stacks;
    std::vector<const char*> m_topLevelNames;
};
