#include <stb_image_write.h>
#include <algorithm>
#include <array>

#include "Labs/0-GettingStarted/CaseSVG.h"
#include "Labs/Common/ImGuiHelper.h"
#include <iostream>
#include "CaseSVG.h"
#include "SVGParser.h"
#include "portable-file-dialogs.h"

namespace VCX::Labs::GettingStarted { 

    CaseSVG::CaseSVG() {
        ImGui::GetIO().ConfigDebugHighlightIdConflicts = false;
        _recompute = true;
    }

    CaseSVG::~CaseSVG() {
        for (auto s : _shapes) delete s;
        _shapes.clear();
    }
    
    void CaseSVG::OnSetupPropsUI() {
        ImGui::Checkbox("Zoom Tooltip", &_enableZoom);
        if (ImGui::Button("Load SVG")) {
            auto sel = pfd::open_file("Open SVG", ".", {"SVG Files", "*.svg"}).result();
            if (!sel.empty()) {
                _pathname = sel[0];
                auto [x, y] = SVGParser::GetSceneSize(_pathname);
                _sizex = x, _sizey = y;
                _recompute = true;
            }
        }
        ImGui::SliderInt("Sample Rate", &_sampleRate, 1, 16);
        if (ImGui::Button("Apply SSAA")) {
            _recompute = true;
        }
        if (ImGui::Button("Export current SVG as PNG")) {
            auto destination = pfd::save_file("Export Image", ".", {"PNG Files (*.png)", "*.png"}).result();
            
            if (!destination.empty()) {
                if (destination.find(".png") == std::string::npos) destination += ".png";
    
                int w = _lastimg.GetSizeX();
                int h = _lastimg.GetSizeY();
                std::vector<uint8_t> pixels;
                pixels.reserve(w * h * 3);
    
                for (int j = 0; j < h; j++) {
                    for (int i = 0; i < w; i++) {
                        glm::vec3 color = _lastimg.At(i, j);
                        pixels.push_back(static_cast<uint8_t>(std::clamp(color.r * 255.0f, 0.0f, 255.0f)));
                        pixels.push_back(static_cast<uint8_t>(std::clamp(color.g * 255.0f, 0.0f, 255.0f)));
                        pixels.push_back(static_cast<uint8_t>(std::clamp(color.b * 255.0f, 0.0f, 255.0f)));
                    }
                }
  
                stbi_write_png(destination.c_str(), w, h, 3, pixels.data(), w * 3);
            }
        }
        if (_messageTimer > 0.0f) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "DONE: Image updated.");
            if (!_skipFrame)
                _messageTimer -= ImGui::GetIO().DeltaTime;
            else _skipFrame = false;
        }
    }
    
    void CaseSVG::LoadSVG(const std::string& path) {
        if (path.size() == 0) return;
        // 清理旧数据
        for (auto s : _shapes) delete s;
        _shapes.clear();
    
        _shapes = SVGParser::ParseFile(path, _sampleRate);
        // for (auto s : _shapes) {
        //     std::cout << static_cast<int>(s->type) << "\n";
        // }
    }
    
    Common::CaseRenderResult CaseSVG::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        int _desiredx = desiredSize.first, _desiredy = desiredSize.second;
        int x = _sizex > 0 ? _sizex : _desiredx;
        int y = _sizey > 0 ? _sizey : _desiredy;
        static int prex = x;
        static int prey = y;
        if (prex != x or prey != y) _recompute = true;
        prex = x, prey = y;
        _sizex = x, _sizey = y;
        if (_recompute) {
            LoadSVG(_pathname);
            Common::ImageRGB tempimage = Common::CreatePureImageRGB(x * _sampleRate, y * _sampleRate, glm::vec3{1.0f});
            _rasterizer.Rasterize(tempimage, _shapes);
            Common::ImageRGB image = Common::CreatePureImageRGB(x, y, glm::vec3{1.0f});
            _rasterizer.Supersample(image, tempimage, _sampleRate);
            _textures[0].Update(image);
            _lastimg = image;
            _messageTimer = 1.0f;
            _skipFrame = true;
            _recompute = false;
        }
        return Common::CaseRenderResult {
            .Fixed     = true,         // 代表⽣成的 2D 图像是固定的，即不会随着窗⼝⼤⼩的变化⽽拉伸的
            .Image     = _textures[0], // 将 _textures[0] 绑定到返回的结果上
            .ImageSize = {x, y},
        };
    }
    
    void CaseSVG::OnProcessInput(ImVec2 const & pos) {
        auto         window  = ImGui::GetCurrentWindow();
        bool         hovered = false;
        bool         anyHeld = false;
        ImVec2 const delta   = ImGui::GetIO().MouseDelta;
        ImGui::ButtonBehavior(window->Rect(), window->GetID("##io"), &hovered, &anyHeld);
        if (! hovered) return;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && delta.x != 0.f)
            ImGui::SetScrollX(window, window->Scroll.x - delta.x);
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && delta.y != 0.f)
            ImGui::SetScrollY(window, window->Scroll.y - delta.y);
        if (_enableZoom && ! anyHeld && ImGui::IsItemHovered())
            Common::ImGuiHelper::ZoomTooltip(_textures[0], {_sizex, _sizey}, pos);
    }
}
