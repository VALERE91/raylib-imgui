// Performance profiler overlay: scoped CPU timers, frame graph, flame graph.

#include "Profiler.h"
#include "ProfilerView.h"

#include <raylib.h>

#include <imgui.h>
#include <implot.h>
#include <rlImGui.h>
#include <extras/IconsFontAwesome6.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
struct Particle
{
    Vector2 position;
    Vector2 velocity;
    Color color;
};

struct Workload
{
    int targetFps = 60;
    int particleCount = 20000;
    int physicsSubsteps = 2;
    float aiCostMs = 2.0f;
    bool spikes = true;
    int spikeInterval = 180;
    float spikeCostMs = 30.0f;
    bool spikeNextFrame = false;
};

// Stands in for game code we don't have: burns CPU for the given time
void BusyWait(double ms)
{
    using Clock = std::chrono::steady_clock;
    const auto end = Clock::now() + std::chrono::duration<double, std::milli>(ms);
    while (Clock::now() < end)
    {
    }
}

Particle MakeParticle()
{
    const float speed = static_cast<float>(GetRandomValue(50, 300));
    const float angle = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
    const Color color = ColorFromHSV(static_cast<float>(GetRandomValue(0, 359)), 0.6f, 1.0f);
    return {{static_cast<float>(GetRandomValue(0, GetScreenWidth())), static_cast<float>(GetRandomValue(0, GetScreenHeight()))},
            {speed * std::cos(angle), speed * std::sin(angle)},
            color};
}

void UpdatePhysics(std::vector<Particle>& particles, int substeps, float dt)
{
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float step = dt / substeps;

    for (int s = 0; s < substeps; ++s)
    {
        PROFILE_SCOPE("Substep");
        for (Particle& p : particles)
        {
            p.velocity.y += 200.0f * step;
            p.position.x += p.velocity.x * step;
            p.position.y += p.velocity.y * step;

            if (p.position.x < 0.0f || p.position.x > width)
                p.velocity.x = -p.velocity.x;
            if (p.position.y > height)
            {
                p.position.y = height;
                p.velocity.y = -p.velocity.y * 0.9f;
            }
        }
    }
}

void DrawWorkloadWindow(Workload& workload)
{
    ImGui::SetNextWindowPos({940, 20}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ICON_FA_SLIDERS " Workload", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    const int fpsValues[] = {30, 60, 120, 144, 0};
    const char* fpsLabels[] = {"30", "60", "120", "144", "Unlimited"};
    int fpsIndex = 0;
    for (int i = 0; i < IM_ARRAYSIZE(fpsValues); ++i)
        if (fpsValues[i] == workload.targetFps)
            fpsIndex = i;
    if (ImGui::Combo("Target FPS", &fpsIndex, fpsLabels, IM_ARRAYSIZE(fpsLabels)))
    {
        workload.targetFps = fpsValues[fpsIndex];
        SetTargetFPS(workload.targetFps);
    }

    ImGui::SliderInt("Particles", &workload.particleCount, 0, 500000, "%d", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderInt("Physics substeps", &workload.physicsSubsteps, 1, 8);
    ImGui::SliderFloat("AI cost (ms)", &workload.aiCostMs, 0.0f, 20.0f, "%.1f");

    ImGui::SeparatorText("Spikes");
    ImGui::Checkbox("Periodic spikes", &workload.spikes);
    ImGui::SliderInt("Every N frames", &workload.spikeInterval, 10, 600);
    ImGui::SliderFloat("Spike cost (ms)", &workload.spikeCostMs, 1.0f, 100.0f, "%.0f");
    if (ImGui::Button("Spike next frame"))
        workload.spikeNextFrame = true;

    ImGui::SeparatorText("Keys");
    ImGui::TextDisabled("F1 overlay   F2 profiler   F3 workload   P pause");

    ImGui::End();
}
} // namespace

int main()
{
    // No VSync so the target FPS combo has full control over the frame rate
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1600, 900, "Profiler overlay");

    Workload workload;
    SetTargetFPS(workload.targetFps);

    rlImGuiSetup(true);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImPlot::CreateContext();

    Profiler& profiler = Profiler::Get();
    ProfilerView view;
    bool showWorkload = true;
    std::vector<Particle> particles;
    uint64_t frameCount = 0;

    while (!WindowShouldClose())
    {
        profiler.BeginFrame();

        {
            PROFILE_SCOPE("Update");

            {
                PROFILE_SCOPE("Input");
                if (IsKeyPressed(KEY_F1)) view.showOverlay = !view.showOverlay;
                if (IsKeyPressed(KEY_F2)) view.showWindow = !view.showWindow;
                if (IsKeyPressed(KEY_F3)) showWorkload = !showWorkload;
                if (IsKeyPressed(KEY_P) && !ImGui::GetIO().WantCaptureKeyboard) view.TogglePause();
            }

            {
                PROFILE_SCOPE("Spawn");
                while (static_cast<int>(particles.size()) < workload.particleCount)
                    particles.push_back(MakeParticle());
                particles.resize(workload.particleCount);
            }

            {
                PROFILE_SCOPE("AI");
                BusyWait(workload.aiCostMs);
            }

            {
                PROFILE_SCOPE("Physics");
                UpdatePhysics(particles, workload.physicsSubsteps, std::min(GetFrameTime(), 0.05f));
            }

            const bool periodicSpike = workload.spikes && frameCount % workload.spikeInterval == 0;
            if (periodicSpike || workload.spikeNextFrame)
            {
                PROFILE_SCOPE("Asset streaming");
                BusyWait(workload.spikeCostMs);
                workload.spikeNextFrame = false;
            }
        }

        {
            PROFILE_SCOPE("Draw");
            BeginDrawing();
            ClearBackground({20, 20, 28, 255});

            {
                // raylib batches draw calls: this measures building the batch,
                // the GPU submission mostly happens in EndDrawing()
                PROFILE_SCOPE("Particles");
                for (const Particle& p : particles)
                    DrawRectangleV(p.position, {3.0f, 3.0f}, p.color);
            }

            {
                PROFILE_SCOPE("UI build");
                rlImGuiBegin();
                view.DrawOverlay();
                view.DrawWindow();
                if (showWorkload)
                    DrawWorkloadWindow(workload);
            }

            {
                PROFILE_SCOPE("UI render");
                rlImGuiEnd();
            }
        }

        {
            // Flushes the batch, swaps buffers and sleeps to hit the target FPS
            PROFILE_SCOPE("EndDrawing");
            EndDrawing();
        }

        profiler.EndFrame();
        ++frameCount;
    }

    ImPlot::DestroyContext();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
