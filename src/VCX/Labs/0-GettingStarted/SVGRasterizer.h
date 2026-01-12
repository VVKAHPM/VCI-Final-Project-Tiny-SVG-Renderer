#pragma once
#include <vector>
#include "SVGData.h"
#include "Labs/Common/ImageRGB.h"

namespace VCX::Labs::GettingStarted {

    class SVGRasterizer {
    public:
        void Rasterize(Common::ImageRGB& image, const std::vector<Shape*>& shapes);
        void Supersample(
            Common::ImageRGB &       output,
            Common::ImageRGB const & input,
            int              rate);
    
    private:
        void DrawRect(Common::ImageRGB& image, Rect* rect);
        void DrawCircle(Common::ImageRGB& image, Circle* circle);
        void DrawEllipse(Common::ImageRGB& image, Ellipse* ellipse);
        void DrawLine(Common::ImageRGB& image, const glm::vec2& p0, const glm::vec2& p1, float width, glm::vec4 color, StrokeLinecap linecap);
        void DrawTriangle(Common::ImageRGB& image, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec4 color);
        void HandleLineJoin(Common::ImageRGB& image, glm::vec2 p_prev, glm::vec2 p_curr, glm::vec2 p_next, Path* path);
        void StrokePath(Common::ImageRGB& image, Path* path);
        void DrawPath(Common::ImageRGB& image, Path* path);
        
        void SetPixel(Common::ImageRGB& image, int x, int y, const glm::vec4 color);
    };
}