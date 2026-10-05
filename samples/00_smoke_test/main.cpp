// Uses every library once to check that the project builds and runs.
// --smoke-test renders 60 frames, saves a screenshot and exits.

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <imgui.h>
#include <rlImGui.h>
#include <extras/IconsFontAwesome6.h>

#include <implot.h>
#include <ImGuizmo.h>
#include <imnodes.h>
#include <TextEditor.h>

#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace
{
constexpr int kScreenWidth  = 1600;
constexpr int kScreenHeight = 900;

struct FrameTimeHistory
{
    static constexpr int kCount = 300;
    std::array<float, kCount> ms{};
    int offset = 0;

    void Push(float valueMs)
    {
        ms[offset] = valueMs;
        offset = (offset + 1) % kCount;
    }
};

// Same projection as BeginMode3D()
Matrix CameraProjection(const Camera3D& camera, float aspect)
{
    return MatrixPerspective(camera.fovy * DEG2RAD, aspect, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
}

void DrawScene(const Camera3D& camera, const float model[16])
{
    BeginMode3D(camera);
    DrawGrid(20, 1.0f);

    rlPushMatrix();
    rlMultMatrixf(model);
    DrawCube({0, 0, 0}, 1.0f, 1.0f, 1.0f, ORANGE);
    DrawCubeWires({0, 0, 0}, 1.0f, 1.0f, 1.0f, MAROON);
    rlPopMatrix();

    EndMode3D();
}

// ImGuizmo needs the draw list of an actual window (it can crash with the
// background draw list), hence the invisible fullscreen window.
void DrawGizmo(Camera3D& camera, float model[16], ImGuizmo::OPERATION op)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("##SceneOverlay", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav);

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x, viewport->Size.y);

    Matrix view = GetCameraMatrix(camera);
    Matrix proj = CameraProjection(camera, viewport->Size.x / viewport->Size.y);
    float16 viewF = MatrixToFloatV(view);
    float16 projF = MatrixToFloatV(proj);

    ImGuizmo::Manipulate(viewF.v, projF.v, op, ImGuizmo::WORLD, model);

    const float cubeSize = 128.0f;
    const float distance = Vector3Distance(camera.position, camera.target);
    ImGuizmo::ViewManipulate(viewF.v, distance,
                             {viewport->Pos.x + viewport->Size.x - cubeSize, viewport->Pos.y + 20.0f},
                             {cubeSize, cubeSize}, 0x10101010);
    if (ImGuizmo::IsUsingViewManipulate())
    {
        // Rebuild the camera from the view matrix (column-major)
        const float* v = viewF.v;
        const Vector3 right = {v[0], v[4], v[8]};
        const Vector3 up    = {v[1], v[5], v[9]};
        const Vector3 back  = {v[2], v[6], v[10]};
        camera.position = Vector3Negate(Vector3Add(Vector3Add(Vector3Scale(right, v[12]),
                                                              Vector3Scale(up, v[13])),
                                                   Vector3Scale(back, v[14])));
        camera.target   = Vector3Subtract(camera.position, Vector3Scale(back, distance));
        camera.up       = up;
    }

    ImGui::End();
}

void DrawNodeGraph(std::vector<std::pair<int, int>>& links)
{
    ImNodes::BeginNodeEditor();

    ImNodes::BeginNode(1);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted("Input");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginOutputAttribute(10);
    ImGui::TextUnformatted("value");
    ImNodes::EndOutputAttribute();
    ImNodes::EndNode();

    ImNodes::BeginNode(2);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted("Output");
    ImNodes::EndNodeTitleBar();
    ImNodes::BeginInputAttribute(20);
    ImGui::TextUnformatted("value");
    ImNodes::EndInputAttribute();
    ImNodes::EndNode();

    for (int i = 0; i < static_cast<int>(links.size()); ++i)
        ImNodes::Link(i, links[i].first, links[i].second);

    ImNodes::EndNodeEditor();

    int start = 0, end = 0;
    if (ImNodes::IsLinkCreated(&start, &end))
        links.emplace_back(start, end);
    int destroyed = 0;
    if (ImNodes::IsLinkDestroyed(&destroyed))
        links.erase(links.begin() + destroyed);
}
} // namespace

int main(int argc, char** argv)
{
    const bool smokeTest = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(kScreenWidth, kScreenHeight, "raylib + Dear ImGui - debug sample");
    SetTargetFPS(144);

    Camera3D camera{};
    camera.position   = {6.0f, 5.0f, 6.0f};
    camera.target     = {0.0f, 0.5f, 0.0f};
    camera.up         = {0.0f, 1.0f, 0.0f};
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    ImPlot::CreateContext();
    ImNodes::CreateContext();
    ImNodes::SetNodeGridSpacePos(1, {40, 60});
    ImNodes::SetNodeGridSpacePos(2, {280, 120});

    FrameTimeHistory frameTimes;
    float16 modelF = MatrixToFloatV(MatrixTranslate(0.0f, 0.5f, 0.0f));
    ImGuizmo::OPERATION gizmoOp = ImGuizmo::TRANSLATE;
    std::vector<std::pair<int, int>> links{{10, 20}};
    bool showImGuiDemo = false;
    bool showImPlotDemo = false;

    TextEditor editor;
    editor.SetLanguage(TextEditor::Language::Cpp());
    editor.SetText("// ImGuiColorTextEdit works!\n"
                   "int main()\n"
                   "{\n"
                   "    return 42;\n"
                   "}\n");

    int frame = 0;
    while (!WindowShouldClose())
    {
        frameTimes.Push(GetFrameTime() * 1000.0f);

        BeginDrawing();
        ClearBackground(DARKGRAY);
        DrawScene(camera, modelF.v);

        rlImGuiBegin();
        ImGuizmo::BeginFrame();

        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        DrawGizmo(camera, modelF.v, gizmoOp);

        ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({420, 300}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(ICON_FA_GAUGE " Stats"))
        {
            ImGui::Text("%d FPS  (%.2f ms)", GetFPS(), GetFrameTime() * 1000.0f);
            ImGui::Checkbox("ImGui demo", &showImGuiDemo);
            ImGui::SameLine();
            ImGui::Checkbox("ImPlot demo", &showImPlotDemo);

            if (ImPlot::BeginPlot("##frametimes", {-1, -1}))
            {
                ImPlot::SetupAxes("frame", "ms", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLimits(ImAxis_X1, 0, FrameTimeHistory::kCount, ImGuiCond_Always);
                ImPlotSpec spec;
                spec.Offset = frameTimes.offset;
                ImPlot::PlotLine("frame time", frameTimes.ms.data(), FrameTimeHistory::kCount, 1.0, 0.0, spec);
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos({20, 340}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(ICON_FA_CUBE " Gizmo", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::RadioButton("Translate", gizmoOp == ImGuizmo::TRANSLATE)) gizmoOp = ImGuizmo::TRANSLATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate", gizmoOp == ImGuizmo::ROTATE)) gizmoOp = ImGuizmo::ROTATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale", gizmoOp == ImGuizmo::SCALE)) gizmoOp = ImGuizmo::SCALE;
            ImGui::Text("Using gizmo: %s", ImGuizmo::IsUsing() ? "yes" : "no");
        }
        ImGui::End();

        ImGui::SetNextWindowPos({460, 560}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({500, 300}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(ICON_FA_DIAGRAM_PROJECT " Nodes"))
            DrawNodeGraph(links);
        ImGui::End();

        ImGui::SetNextWindowPos({1000, 560}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({560, 300}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(ICON_FA_CODE " Editor"))
            editor.Render("##editor");
        ImGui::End();

        if (showImGuiDemo)  ImGui::ShowDemoWindow(&showImGuiDemo);
        if (showImPlotDemo) ImPlot::ShowDemoWindow(&showImPlotDemo);

        rlImGuiEnd();
        EndDrawing();

        if (smokeTest && ++frame == 60)
        {
            TakeScreenshot("smoke_test.png");
            break;
        }
    }

    ImNodes::DestroyContext();
    ImPlot::DestroyContext();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
