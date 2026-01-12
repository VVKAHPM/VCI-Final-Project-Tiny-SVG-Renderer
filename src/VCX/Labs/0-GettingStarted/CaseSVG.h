#pragma once

#include "Engine/Async.hpp"
#include "Labs/Common/ICase.h"
#include "Labs/Common/ImageRGB.h"
#include "SVGData.h"
#include "SVGRasterizer.h"
#include <vector>

namespace VCX::Labs::GettingStarted {

    class CaseSVG : public Common::ICase {
    public:
        CaseSVG();
        ~CaseSVG();

        virtual std::string_view const GetName() override { return "Tiny SVG Renderer"; }
        
        virtual void OnSetupPropsUI() override;
        virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override;
        virtual void OnProcessInput(ImVec2 const & pos) override;
    
    private:

        Common::ImageRGB _lastimg;
        std::vector<Shape*> _shapes;
        std::string _pathname;
        SVGRasterizer _rasterizer;
        float _messageTimer = 0.0f;
        int _sizex = 800;
        int _sizey = 600;
        int _sampleRate = 1;
        std::array<Engine::GL::UniqueTexture2D, 2> _textures;
        std::array<Common::ImageRGB, 2>            _empty;
        Engine::Async<Common::ImageRGB>            _task;
        bool _enableZoom = true;
        bool _recompute = false;
        bool _skipFrame = false;

        void LoadSVG(const std::string& filepath);

    };
}
