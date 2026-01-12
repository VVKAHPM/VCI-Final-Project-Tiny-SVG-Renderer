#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "Labs/Common/ImageRGB.h"

namespace VCX::Labs::GettingStarted {

    enum class ShapeType { Rectangle, Circle, Ellipse, Line, Polyline, Polygon, Path };
    enum class FillRule { NonZero, EvenOdd };
    enum class StrokeLinecap { Butt, Square, Round };
    enum class StrokeLinejoin { Miter, Round, Bevel };

    struct ShapeStyle {
        std::optional<glm::vec4> fill;
        std::optional<glm::vec4> stroke;
        std::optional<float> strokeWidth;
        std::optional<float> opacity;
        std::optional<float> fillOpacity;
        std::optional<float> strokeOpacity;
        std::optional<StrokeLinecap> strokeLinecap;
        std::optional<StrokeLinejoin> strokeLinejoin;
    };


    struct RenderStyle {
        glm::vec4 fill = {0, 0, 0, 1};
        glm::vec4 stroke = {0, 0, 0, 0};
        float strokeWidth = 1.0;
        float totalOpacity = 1.0;
        StrokeLinecap linecap = StrokeLinecap::Butt;
        StrokeLinejoin linejoin = StrokeLinejoin::Miter;
    };

    struct Shape {
        ShapeType type;
        glm::vec4 fillColor = {0, 0, 0, 1};
        glm::vec4 strokeColor = {0, 0, 0, 0};
        float strokeWidth = 0;
        StrokeLinecap linecap = StrokeLinecap::Butt;
        StrokeLinejoin linejoin = StrokeLinejoin::Miter;
        virtual ~Shape() = default;
    };

    struct Rect : Shape {
        float x, y, width, height;
        Rect() { type = ShapeType::Rectangle; }
    };

    struct Circle : Shape {
        float r, cx, cy;
        Circle() { type = ShapeType::Circle; }
    };

    struct Ellipse : Shape {
        float rx, ry;
        float cx, cy;
        Ellipse() { type = ShapeType::Ellipse; }
    };

    struct Path : Shape {
        std::vector<std::vector<glm::vec2> > sub_paths;
        FillRule fill_rule = FillRule::NonZero;
        Path() { type = ShapeType::Path; }
    };
}