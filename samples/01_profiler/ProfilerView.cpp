#include "ProfilerView.h"

#include <extras/IconsFontAwesome6.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace
{
constexpr int kOverlayFrames = 120;

struct FrameStats
{
    double avgMs = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
    double p99Ms = 0.0;  // 99th percentile, gives the "1% low" FPS
};

FrameStats ComputeStats(int maxFrames)
{
    const Profiler& profiler = Profiler::Get();
    const int count = std::min(maxFrames, profiler.FrameCount());
    if (count == 0)
        return {};

    std::array<double, kOverlayFrames> sorted{};
    FrameStats stats;
    for (int i = 0; i < count; ++i)
    {
        sorted[i] = profiler.GetFrame(i).durationMs;
        stats.avgMs += sorted[i];
    }
    std::sort(sorted.begin(), sorted.begin() + count);

    stats.avgMs /= count;
    stats.minMs = sorted[0];
    stats.maxMs = sorted[count - 1];
    stats.p99Ms = sorted[std::min(count - 1, count * 99 / 100)];
    return stats;
}

// Hues are handed out in order of first appearance, spaced by the golden ratio
// so that consecutive zones get clearly different colors
ImU32 ZoneColor(std::string_view name)
{
    static std::unordered_map<std::string_view, ImU32> colors;
    auto [it, inserted] = colors.try_emplace(name, 0);
    if (inserted)
    {
        const float hue = std::fmod(static_cast<float>(colors.size()) * 0.618034f, 1.0f);
        it->second = ImColor::HSV(hue, 0.55f, 0.85f);
    }
    return it->second;
}

ImVec4 BudgetColor(double ms, double budgetMs)
{
    // Some tolerance, the frame limiter usually oversleeps a little
    if (ms <= budgetMs * 1.05)
        return {0.4f, 0.9f, 0.4f, 1.0f};
    if (ms <= budgetMs * 2.0)
        return {1.0f, 0.7f, 0.2f, 1.0f};
    return {1.0f, 0.3f, 0.3f, 1.0f};
}
} // namespace

void ProfilerView::TogglePause()
{
    Profiler& profiler = Profiler::Get();
    profiler.SetPaused(!profiler.IsPaused());
    if (!profiler.IsPaused())
        m_followLatest = true;
}

void ProfilerView::DrawOverlay()
{
    if (!showOverlay)
        return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (m_corner != -1)
    {
        const float pad = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const bool right = m_corner & 1;
        const bool bottom = m_corner & 2;
        const ImVec2 pos(right ? viewport->WorkPos.x + viewport->WorkSize.x - pad : viewport->WorkPos.x + pad,
                         bottom ? viewport->WorkPos.y + viewport->WorkSize.y - pad : viewport->WorkPos.y + pad);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {right ? 1.0f : 0.0f, bottom ? 1.0f : 0.0f});
        flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (ImGui::Begin("Profiler overlay", &showOverlay, flags))
    {
        const Profiler& profiler = Profiler::Get();
        const FrameStats stats = ComputeStats(kOverlayFrames);

        if (stats.avgMs > 0.0)
        {
            ImGui::TextColored(BudgetColor(stats.avgMs, budgetMs), "%6.1f FPS  %6.2f ms", 1000.0 / stats.avgMs,
                               stats.avgMs);
            ImGui::Text("min %.2f  max %.2f ms", stats.minMs, stats.maxMs);
            ImGui::Text("1%% low   %.1f FPS", 1000.0 / stats.p99Ms);
        }
        if (profiler.IsPaused())
            ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, ICON_FA_PAUSE " PAUSED");

        int count = std::min(kOverlayFrames, profiler.FrameCount());
        auto getter = [](void* data, int i) -> float {
            const int n = *static_cast<int*>(data);
            return static_cast<float>(Profiler::Get().GetFrame(n - 1 - i).durationMs);
        };
        ImGui::PlotLines("##frametimes", getter, &count, count, 0, nullptr, 0.0f, budgetMs * 2.0f, {220, 40});

        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Free position", nullptr, m_corner == -1)) m_corner = -1;
            if (ImGui::MenuItem("Top-left", nullptr, m_corner == 0)) m_corner = 0;
            if (ImGui::MenuItem("Top-right", nullptr, m_corner == 1)) m_corner = 1;
            if (ImGui::MenuItem("Bottom-left", nullptr, m_corner == 2)) m_corner = 2;
            if (ImGui::MenuItem("Bottom-right", nullptr, m_corner == 3)) m_corner = 3;
            ImGui::Separator();
            ImGui::MenuItem("Profiler window", "F2", &showWindow);
            if (ImGui::MenuItem("Hide overlay", "F1"))
                showOverlay = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void ProfilerView::DrawWindow()
{
    if (!showWindow)
        return;

    ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({900, 700}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_FA_STOPWATCH " Profiler", &showWindow))
    {
        const Profiler& profiler = Profiler::Get();
        if (m_followLatest && profiler.FrameCount() > 0)
            m_selectedFrame = profiler.GetFrame(0).index;

        DrawToolbar();
        UpdateZoneStats();
        DrawFrameGraph();

        if (const Profiler::Frame* frame = SelectedFrame())
        {
            char title[64];
            std::snprintf(title, sizeof(title), "Frame #%llu - %.2f ms",
                          static_cast<unsigned long long>(frame->index), frame->durationMs);
            ImGui::SeparatorText(title);
            DrawFlameGraph(*frame);
            DrawZoneTable(*frame);
        }
    }
    ImGui::End();
}

const Profiler::Frame* ProfilerView::SelectedFrame() const
{
    const Profiler& profiler = Profiler::Get();
    if (profiler.FrameCount() == 0)
        return nullptr;

    const uint64_t latest = profiler.GetFrame(0).index;
    const uint64_t age = latest >= m_selectedFrame ? latest - m_selectedFrame : 0;
    return &profiler.GetFrame(static_cast<int>(std::min<uint64_t>(age, profiler.FrameCount() - 1)));
}

void ProfilerView::DrawToolbar()
{
    const bool paused = Profiler::Get().IsPaused();
    if (ImGui::Button(paused ? ICON_FA_PLAY " Resume" : ICON_FA_PAUSE " Pause"))
        TogglePause();
    ImGui::SetItemTooltip("P");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    const float budgets[] = {30.0f, 60.0f, 120.0f, 144.0f};
    char preview[32];
    std::snprintf(preview, sizeof(preview), "%.0f FPS (%.2f ms)", 1000.0f / budgetMs, budgetMs);
    if (ImGui::BeginCombo("Budget", preview))
    {
        for (float fps : budgets)
        {
            char label[32];
            std::snprintf(label, sizeof(label), "%.0f FPS (%.2f ms)", fps, 1000.0f / fps);
            if (ImGui::Selectable(label, std::abs(budgetMs - 1000.0f / fps) < 0.01f))
                budgetMs = 1000.0f / fps;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Click on the graph to inspect a frame");
}

void ProfilerView::DrawFrameGraph()
{
    const Profiler& profiler = Profiler::Get();
    const int count = profiler.FrameCount();
    if (count == 0)
        return;

    // The graph stacks the top-level zones of each frame on top of each other
    m_topLevelNames.clear();
    for (int age = 0; age < count; ++age)
        for (const Profiler::Zone& zone : profiler.GetFrame(age).zones)
            if (zone.depth == 0 && std::find(m_topLevelNames.begin(), m_topLevelNames.end(), zone.name) == m_topLevelNames.end())
                m_topLevelNames.push_back(zone.name);

    m_xs.resize(count);
    m_frameMs.resize(count);
    m_stacks.resize(m_topLevelNames.size() + 1);
    for (std::vector<double>& stack : m_stacks)
        stack.assign(count, 0.0);

    double maxMs = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const Profiler::Frame& frame = profiler.GetFrame(count - 1 - i);
        m_xs[i] = static_cast<double>(frame.index);
        m_frameMs[i] = frame.durationMs;
        maxMs = std::max(maxMs, frame.durationMs);

        for (const Profiler::Zone& zone : frame.zones)
        {
            if (zone.depth != 0)
                continue;
            const auto it = std::find(m_topLevelNames.begin(), m_topLevelNames.end(), zone.name);
            m_stacks[1 + (it - m_topLevelNames.begin())][i] += zone.DurationMs();
        }
        for (size_t k = 1; k < m_stacks.size(); ++k)
            m_stacks[k][i] += m_stacks[k - 1][i];
    }

    if (ImPlot::BeginPlot("##frames", {-1, 220}, ImPlotFlags_NoMouseText))
    {
        ImPlot::SetupAxes(nullptr, "ms");
        ImPlot::SetupLegend(ImPlotLocation_NorthWest);
        if (!profiler.IsPaused())
        {
            ImPlot::SetupAxisLimits(ImAxis_X1, m_xs.front(), m_xs.back(), ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, std::max(budgetMs * 2.0, maxMs * 1.1), ImPlotCond_Always);
        }

        for (size_t k = 0; k < m_topLevelNames.size(); ++k)
        {
            ImPlotSpec spec;
            spec.FillColor = ImGui::ColorConvertU32ToFloat4(ZoneColor(m_topLevelNames[k]));
            spec.FillAlpha = 0.8f;
            ImPlot::PlotShaded(m_topLevelNames[k], m_xs.data(), m_stacks[k].data(), m_stacks[k + 1].data(), count, spec);
        }

        ImPlotSpec frameSpec;
        frameSpec.LineColor = {1.0f, 1.0f, 1.0f, 1.0f};
        ImPlot::PlotLine("Frame", m_xs.data(), m_frameMs.data(), count, frameSpec);

        const double budget = budgetMs;
        ImPlotSpec budgetSpec;
        budgetSpec.LineColor = {1.0f, 0.3f, 0.3f, 1.0f};
        budgetSpec.Flags = ImPlotInfLinesFlags_Horizontal;
        ImPlot::PlotInfLines("Budget", &budget, 1, budgetSpec);

        if (!m_followLatest)
        {
            const double selected = static_cast<double>(m_selectedFrame);
            ImPlotSpec selectedSpec;
            selectedSpec.LineColor = {1.0f, 0.8f, 0.2f, 1.0f};
            ImPlot::PlotInfLines("##selected", &selected, 1, selectedSpec);
            ImPlot::TagX(selected, selectedSpec.LineColor, "#%llu", static_cast<unsigned long long>(m_selectedFrame));
        }

        if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const double x = std::clamp(ImPlot::GetPlotMousePos().x, m_xs.front(), m_xs.back());
            m_selectedFrame = static_cast<uint64_t>(x + 0.5);
            m_followLatest = false;
            Profiler::Get().SetPaused(true);
        }

        ImPlot::EndPlot();
    }
}

void ProfilerView::DrawFlameGraph(const Profiler::Frame& frame)
{
    int maxDepth = 0;
    for (const Profiler::Zone& zone : frame.zones)
        maxDepth = std::max(maxDepth, zone.depth);

    const float rowHeight = ImGui::GetFrameHeight();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, (maxDepth + 1) * rowHeight);
    ImGui::InvisibleButton("##flamegraph", size);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    // Scale so both the frame and the budget always fit
    const double rangeMs = std::max(frame.durationMs, static_cast<double>(budgetMs));
    const float msToPx = static_cast<float>(size.x / rangeMs);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, ImGui::GetColorU32(ImGuiCol_FrameBg));

    int hoveredZone = -1;
    for (int i = 0; i < static_cast<int>(frame.zones.size()); ++i)
    {
        const Profiler::Zone& zone = frame.zones[i];
        const ImVec2 min(origin.x + static_cast<float>(zone.startMs) * msToPx, origin.y + zone.depth * rowHeight);
        const ImVec2 max(std::max(min.x + 1.0f, origin.x + static_cast<float>(zone.endMs) * msToPx),
                         min.y + rowHeight - 1.0f);

        const bool isHovered = hovered && mouse.x >= min.x && mouse.x < max.x && mouse.y >= min.y && mouse.y < max.y;
        if (isHovered)
            hoveredZone = i;

        drawList->AddRectFilled(min, max, ZoneColor(zone.name));
        if (isHovered)
            drawList->AddRect(min, max, IM_COL32_WHITE);

        if (max.x - min.x > 30.0f)
        {
            char label[128];
            std::snprintf(label, sizeof(label), "%s  %.2f ms", zone.name, zone.DurationMs());
            drawList->PushClipRect(min, max, true);
            drawList->AddText({min.x + 4.0f, min.y + (rowHeight - ImGui::GetFontSize()) * 0.5f}, IM_COL32_BLACK, label);
            drawList->PopClipRect();
        }
    }

    const float frameEndX = origin.x + static_cast<float>(frame.durationMs) * msToPx;
    const float budgetX = origin.x + budgetMs * msToPx;
    drawList->AddLine({frameEndX, origin.y}, {frameEndX, origin.y + size.y}, IM_COL32_WHITE, 2.0f);
    drawList->AddLine({budgetX, origin.y}, {budgetX, origin.y + size.y}, IM_COL32(255, 80, 80, 255), 2.0f);

    if (hoveredZone >= 0 && ImGui::BeginTooltip())
    {
        const Profiler::Zone& zone = frame.zones[hoveredZone];
        ImGui::TextUnformatted(zone.name);
        ImGui::Separator();
        ImGui::Text("Duration  %.3f ms", zone.DurationMs());
        ImGui::Text("Start     %.3f ms", zone.startMs);
        ImGui::Text("Frame     %.1f%%", 100.0 * zone.DurationMs() / frame.durationMs);
        ImGui::EndTooltip();
    }
}

void ProfilerView::DrawZoneTable(const Profiler::Frame& frame)
{
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##zones", 5, flags))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Zone", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("% frame", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("avg ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("max ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < static_cast<int>(frame.zones.size()); ++i)
        if (frame.zones[i].parent == -1)
            DrawZoneRow(frame, i);

    ImGui::EndTable();
}

void ProfilerView::DrawZoneRow(const Profiler::Frame& frame, int zoneIndex)
{
    const Profiler::Zone& zone = frame.zones[zoneIndex];
    const int zoneCount = static_cast<int>(frame.zones.size());
    // Children are always recorded right after their parent
    const bool hasChildren = zoneIndex + 1 < zoneCount && frame.zones[zoneIndex + 1].parent == zoneIndex;

    ImGui::PushID(zoneIndex);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_DefaultOpen;
    if (!hasChildren)
        nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::PushStyleColor(ImGuiCol_Text, ZoneColor(zone.name));
    const bool open = ImGui::TreeNodeEx(zone.name, nodeFlags);
    ImGui::PopStyleColor();

    ImGui::TableNextColumn();
    ImGui::Text("%.3f", zone.DurationMs());
    ImGui::TableNextColumn();
    ImGui::Text("%.1f", 100.0 * zone.DurationMs() / frame.durationMs);

    const auto it = m_zoneStats.find(zone.name);
    if (it != m_zoneStats.end())
    {
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", it->second.totalMs / it->second.frames);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", it->second.maxMs);
    }

    if (open && hasChildren)
    {
        for (int i = zoneIndex + 1; i < zoneCount && frame.zones[i].depth > zone.depth; ++i)
            if (frame.zones[i].parent == zoneIndex)
                DrawZoneRow(frame, i);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void ProfilerView::UpdateZoneStats()
{
    const Profiler& profiler = Profiler::Get();
    if (profiler.IsPaused() && !m_zoneStats.empty())
        return;

    // Stats are per frame: a zone called several times in a frame is summed first
    m_zoneStats.clear();
    for (int age = 0; age < profiler.FrameCount(); ++age)
    {
        m_frameSums.clear();
        for (const Profiler::Zone& zone : profiler.GetFrame(age).zones)
            m_frameSums[zone.name] += zone.DurationMs();

        for (const auto& [name, ms] : m_frameSums)
        {
            ZoneStats& stats = m_zoneStats[name];
            stats.totalMs += ms;
            stats.maxMs = std::max(stats.maxMs, ms);
            ++stats.frames;
        }
    }
}
