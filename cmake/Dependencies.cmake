include(FetchContent)

set(FETCHCONTENT_QUIET ON)

# raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)

FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        6.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SYSTEM
)
FetchContent_MakeAvailable(raylib)

# FTXUI, for the terminal launcher
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v7.0.3
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SYSTEM
)
FetchContent_MakeAvailable(ftxui)

# The ImGui libraries are source-only: SOURCE_SUBDIR points to a folder that
# doesn't exist so their own CMakeLists are not used.
macro(fetch_sources name repo tag)
    FetchContent_Declare(${name}
        GIT_REPOSITORY ${repo}
        GIT_TAG        ${tag}
        GIT_PROGRESS   TRUE
        SOURCE_SUBDIR  __no_cmake__
    )
    FetchContent_MakeAvailable(${name})
endmacro()

fetch_sources(imgui    https://github.com/ocornut/imgui.git              v1.92.9b-docking)
fetch_sources(rlimgui  https://github.com/raylib-extras/rlImGui.git       1550009359ad975927f7f0e4a3f47e3f27123ea9)
fetch_sources(implot   https://github.com/epezent/implot.git              09e2ba71766e25d88053a2173936c9d1043bae42)
fetch_sources(imguizmo https://github.com/CedricGuillemet/ImGuizmo.git    18cef5e031d8c6973d80284c67f60549fafd78c1)
fetch_sources(imnodes  https://github.com/Nelarius/imnodes.git            eb36902c892548ef94f88f51ad7e7c9c7058a71c)
fetch_sources(textedit https://github.com/goossens/ImGuiColorTextEdit.git 133614b0d5e1008527a26f46a93fdb1d751ca115)

# ImGui and all its extensions in a single library, so they are all built
# against the same imgui.h / imconfig.h.
add_library(imgui_stack STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp

    ${rlimgui_SOURCE_DIR}/rlImGui.cpp

    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp

    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
    ${imguizmo_SOURCE_DIR}/src/ImSequencer.cpp
    ${imguizmo_SOURCE_DIR}/src/ImCurveEdit.cpp
    ${imguizmo_SOURCE_DIR}/src/ImGradient.cpp
    ${imguizmo_SOURCE_DIR}/src/GraphEditor.cpp

    ${imnodes_SOURCE_DIR}/imnodes.cpp

    ${textedit_SOURCE_DIR}/TextEditor.cpp
    ${textedit_SOURCE_DIR}/TextDiff.cpp
)
add_library(imgui::stack ALIAS imgui_stack)

target_include_directories(imgui_stack SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/misc/cpp
    ${rlimgui_SOURCE_DIR}
    ${implot_SOURCE_DIR}
    ${imguizmo_SOURCE_DIR}/src
    ${imnodes_SOURCE_DIR}
    ${textedit_SOURCE_DIR}
)

target_compile_features(imgui_stack PUBLIC cxx_std_20)
target_link_libraries(imgui_stack PUBLIC raylib)

if(MSVC)
    target_compile_options(imgui_stack PRIVATE /W0)
else()
    target_compile_options(imgui_stack PRIVATE -w)
endif()
