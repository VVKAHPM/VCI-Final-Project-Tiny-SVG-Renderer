#include "Assets/bundled.h"
#include "Labs/0-GettingStarted/App.h"

int main() {
    using namespace VCX;
    return Engine::RunApp<Labs::GettingStarted::App>(Engine::AppContextOptions {
        .Title      = "VCX Final Project: Tiny SVG Renderer",
        .WindowSize = { 800, 600 },
        .FontSize   = 16,

        .IconFileNames = Assets::DefaultIcons,
        .FontFileNames = Assets::DefaultFonts,
    });
}
