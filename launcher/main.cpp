// Terminal menu listing the samples. Enter launches the selected one, and the
// menu comes back when its window is closed.

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstdlib>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace
{
struct Sample
{
    const char* name;
    const char* description;
    const char* path;
};

const Sample kSamples[] = {
#include "samples.inc"
};

int Run(const Sample& sample)
{
    const std::string command = std::string("\"") + sample.path + "\"";
    const int status = std::system(command.c_str());
#if defined(_WIN32)
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}
} // namespace

int main()
{
    using namespace ftxui;

    std::vector<std::string> names;
    for (const Sample& sample : kSamples)
        names.emplace_back(sample.name);

    auto screen = ScreenInteractive::Fullscreen();
    int selected = 0;
    std::string status;

    MenuOption options = MenuOption::Vertical();
    options.entries = &names;
    options.selected = &selected;
    options.on_enter = [&] {
        const Sample& sample = kSamples[selected];
        int code = 0;
        screen.WithRestoredIO([&] { code = Run(sample); })();
        status = std::string(sample.name) + " exited with code " + std::to_string(code);
    };
    Component menu = Menu(options);

    Component root = Renderer(menu, [&] {
        return vbox({
                   text(" raylib + ImGui samples ") | bold | center,
                   separator(),
                   menu->Render() | vscroll_indicator | frame | flex,
                   separator(),
                   paragraph(kSamples[selected].description) | dim,
                   separator(),
                   hbox({
                       text(status),
                       filler(),
                       text("↑/↓ select   Enter launch   q quit") | dim,
                   }),
               }) |
               border;
    });

    root |= CatchEvent([&](Event event) {
        if (event == Event::Character('q') || event == Event::Escape)
        {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(root);
    return 0;
}
