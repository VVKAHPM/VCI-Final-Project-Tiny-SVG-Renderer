#include "SVGRasterizer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

namespace VCX::Labs::GettingStarted {
    void SVGRasterizer::Rasterize(Common::ImageRGB& image, const std::vector<Shape*>& shapes) {
        for (int x = 0; x < image.GetSizeX(); x++)
            for (int y = 0; y < image.GetSizeY(); y++)
                image.At(x, y) = {1.0f, 1.0f, 1.0f}; 

        for (auto shape : shapes) {
            if (shape->type == ShapeType::Rectangle) 
                DrawRect(image, static_cast<Rect*>(shape));
            else if (shape->type == ShapeType::Circle) 
                DrawCircle(image, static_cast<Circle*>(shape));
            else if (shape->type == ShapeType::Ellipse)
                DrawEllipse(image, static_cast<Ellipse*>(shape));
            else if (shape->type == ShapeType::Path) 
                DrawPath(image, static_cast<Path*>(shape));
        }
    }

    void SVGRasterizer::Supersample(
        Common::ImageRGB &       output,
        Common::ImageRGB const & input,
        int              rate) {
        // your code here:
        int oX = output.GetSizeX(), oY = output.GetSizeY();
        int iX = input.GetSizeX(), iY = input.GetSizeY();
        float r = 1.0 * iX / oX / rate;
        for (int i = 0; i < oX; i++) {
            for (int j = 0; j < oY; j++) {
                glm::vec3 color(0);
                for (int k = 0; k < rate; k++) 
                  for (int l = 0; l < rate; l++) {
                    int nx = i * r * rate + r * k, ny = j * r * rate + r * l;
                    if (nx < iX and ny < iY) {
                        color += glm::vec3(input.At(nx, ny));
                    }
                  }
                output.At(i, j) = color / (float)rate / (float)rate;
            }
        }
    }
    
    void SVGRasterizer::DrawRect(Common::ImageRGB& image, Rect* rect) {
        int minX = std::max(0, (int)rect->x);
        int maxX = std::min((int)image.GetSizeX(), (int)(rect->x + rect->width));
        int minY = std::max(0, (int)rect->y);
        int maxY = std::min((int)image.GetSizeY(), (int)(rect->y + rect->height));
    
        if (rect->fillColor.a >= 0.001f)
            for (int i = minX; i <= maxX; i++) 
                for (int j = minY; j <= maxY; j++)
                    SetPixel(image, i, j, rect->fillColor);
        
        if (rect->strokeWidth >= 0.001f and rect->strokeColor.a >= 0.001f) {
            float halfWidth = rect->strokeWidth / 2;
            auto drawBox = [&](float x1, float y1, float x2, float y2) {
                for (int i = std::max(0, (int)(x1)); i <= std::min((int)image.GetSizeX()-1, (int)(x2)); i++)
                    for (int j = std::max(0, (int)(y1)); j <= std::min((int)image.GetSizeY()-1, (int)(y2)); j++)
                        SetPixel(image, i, j, rect->strokeColor);
            };
            drawBox(rect->x - halfWidth, rect->y - halfWidth, rect->x + rect->width + halfWidth, rect->y + halfWidth);
            drawBox(rect->x - halfWidth, rect->y + rect->height - halfWidth, rect->x + rect->width + halfWidth, rect->y + rect->height + halfWidth);
            drawBox(rect->x - halfWidth, rect->y - halfWidth, rect->x + halfWidth, rect->y + rect->height + halfWidth);
            drawBox(rect->x + rect->width - halfWidth, rect->y - halfWidth, rect->x + rect->width + halfWidth, rect->y + rect->height + halfWidth);
        } 
    }
    
    void SVGRasterizer::DrawCircle(Common::ImageRGB& image, Circle* circle) {
        float r = circle->r;
        float sw = circle->strokeWidth;
        float outerR = r + sw / 2.0f;
        float innerR = r - sw / 2.0f;

        int minX = std::max(0, (int)std::floor(circle->cx - outerR));
        int maxX = std::min((int)image.GetSizeX() - 1, (int)std::ceil(circle->cx + outerR));
        int minY = std::max(0, (int)std::floor(circle->cy - outerR));
        int maxY = std::min((int)image.GetSizeY() - 1, (int)std::ceil(circle->cy + outerR));

        for (int i = minX; i <= maxX; i++) {
            for (int j = minY; j <= maxY; j++) {
                float dx = (float)i - circle->cx;
                float dy = (float)j - circle->cy;
                float distSq = dx * dx + dy * dy;
                float dist = std::sqrt(distSq);

                if (circle->strokeColor.a > 0.001f && dist <= outerR && dist >= innerR) {
                    SetPixel(image, i, j, circle->strokeColor);
                }
                else if (dist < innerR) {
                    SetPixel(image, i, j, circle->fillColor);
                }
            }
        }
    }

    void SVGRasterizer::DrawTriangle(Common::ImageRGB& image, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec4 color) {
        int minX = std::max(0, (int)std::floor(std::min({p1.x, p2.x, p3.x})));
        int maxX = std::min((int)image.GetSizeX() - 1, (int)std::ceil(std::max({p1.x, p2.x, p3.x})));
        int minY = std::max(0, (int)std::floor(std::min({p1.y, p2.y, p3.y})));
        int maxY = std::min((int)image.GetSizeY() - 1, (int)std::ceil(std::max({p1.y, p2.y, p3.y})));
    
        auto cross_product = [](glm::vec2 a, glm::vec2 b, glm::vec2 c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        };
    
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                glm::vec2 p(x + 0.5, y + 0.5);
                float d1 = cross_product(p1, p2, p);
                float d2 = cross_product(p2, p3, p);
                float d3 = cross_product(p3, p1, p);
                if ((d1 >= 0 && d2 >= 0 && d3 >= 0) || (d1 <= 0 && d2 <= 0 && d3 <= 0)) {
                    SetPixel(image, x, y, color);
                }
            }
        }
    }

    void SVGRasterizer::HandleLineJoin(Common::ImageRGB& image, glm::vec2 p_prev, glm::vec2 p_curr, glm::vec2 p_next, Path* path) {
        float r = path->strokeWidth * 0.5f;
        
        glm::vec2 v1 = glm::normalize(p_curr - p_prev);
        glm::vec2 v2 = glm::normalize(p_next - p_curr);
    
        glm::vec2 n1 = glm::vec2(-v1.y, v1.x);
        glm::vec2 n2 = glm::vec2(-v2.y, v2.x);
    
        float cross = v1.x * v2.y - v1.y * v2.x;
        if (std::abs(cross) < 1e-6f) return;
    
        float side = (cross < 0) ? 1.0f : -1.0f; 
        glm::vec2 edge1 = p_curr + n1 * r * side;
        glm::vec2 edge2 = p_curr + n2 * r * side;
    
        if (path->linejoin == StrokeLinejoin::Bevel) {
            DrawTriangle(image, p_curr, edge1, edge2, path->strokeColor);
        } 
        else if (path->linejoin == StrokeLinejoin::Miter) {
            glm::vec2 miterDir = glm::normalize(n1 + n2);
            float cosAlpha = glm::dot(miterDir, n1);
            float miterLen = r / cosAlpha;
            if (std::abs(miterLen) > 4.0 * r) {
                DrawTriangle(image, p_curr, edge1, edge2, path->strokeColor);
            } else {
                glm::vec2 miterPoint = p_curr + miterDir * miterLen * side;
                DrawTriangle(image, p_curr, edge1, miterPoint, path->strokeColor);
                DrawTriangle(image, p_curr, edge2, miterPoint, path->strokeColor);
            }
        }
    }

    void SVGRasterizer::DrawEllipse(Common::ImageRGB& image, Ellipse* ellipse) {
        float rx = ellipse->rx;
        float ry = ellipse->ry;
        float sw = ellipse->strokeWidth;
        float halfSw = sw / 2.0f;

        int minX = std::max(0, (int)std::floor(ellipse->cx - rx - sw));
        int maxX = std::min((int)image.GetSizeX() - 1, (int)std::ceil(ellipse->cx + rx + sw));
        int minY = std::max(0, (int)std::floor(ellipse->cy - ry - sw));
        int maxY = std::min((int)image.GetSizeY() - 1, (int)std::ceil(ellipse->cy + ry + sw));

        for (int i = minX; i <= maxX; i++) {
            for (int j = minY; j <= maxY; j++) {
                float dx = (float)i - ellipse->cx;
                float dy = (float)j - ellipse->cy;
                float f = (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) - 1.0f;

                float gradX = (2.0f * dx) / (rx * rx);
                float gradY = (2.0f * dy) / (ry * ry);
                float gradLen = std::sqrt(gradX * gradX + gradY * gradY);
                float dist = f / (gradLen + 0.0001f); 

                if (ellipse->strokeColor.a >= 0.001f && std::abs(dist) <= halfSw) {
                    SetPixel(image, i, j, ellipse->strokeColor);
                }
                else if (dist < -halfSw) {
                    SetPixel(image, i, j, ellipse->fillColor);
                }
            }
        }
    }

    void SVGRasterizer::DrawLine(Common::ImageRGB& image, const glm::vec2& p1, const glm::vec2& p2, float width, glm::vec4 color, StrokeLinecap linecap) {
        if (width < 1e-6 or color.a < 1e-6) return;
        float halfwidth = width * 0.5f;
        halfwidth = std::max(0.5f, halfwidth);
        int minX = std::max(0, (int)std::floor(std::min(p1.x, p2.x) - halfwidth));
        int maxX = std::min((int)image.GetSizeX() - 1, (int)std::ceil(std::max(p1.x, p2.x) + halfwidth));
        int minY = std::max(0, (int)std::floor(std::min(p1.y, p2.y) - halfwidth));
        int maxY = std::min((int)image.GetSizeY() - 1, (int)std::ceil(std::max(p1.y, p2.y) + halfwidth));

        glm::vec2 line = p2 - p1;
        float line_length = glm::length(line);
        if (line_length < 1e-6) return;

        for (int x = minX; x <= maxX; x++) {
            for (int y = minY; y <= maxY; y++) {
                glm::vec2 p(x + 0.5, y + 0.5);
                float t = glm::dot(p - p1, line) / line_length / line_length;
                glm::vec2 proj = p1 + t * line;
                bool inside = false;
                if (linecap == StrokeLinecap::Butt) {
                    if (t < 0.0f or t > 1.0f) continue;
                    inside = (glm::length(p - proj) <= halfwidth + 0.01);
                } else if (linecap == StrokeLinecap::Square) {
                    float offset = halfwidth / line_length;
                    if (t < -offset or t > 1.0f + offset) continue;
                    inside = (glm::length(p - proj) <= halfwidth + 0.01);
                } else {
                    if (t < 0.0f) {
                        inside = (glm::length(p - p1) <= halfwidth + 0.01);
                    } else if (t > 1.0f) {
                        inside = (glm::length(p - p2) <= halfwidth + 0.01);
                    } else inside = (glm::length(p - proj) <= halfwidth + 0.01);
                }

                if (inside) {
                    SetPixel(image, x, y, color);
                }
            }
        }
    }

    struct Edge {
        float y_max;
        float x_now;
        float dx;
        int dir;

        bool operator<(const Edge& other) const {
            return x_now < other.x_now;
        }
    };

    void SVGRasterizer::StrokePath(Common::ImageRGB& image, Path* path) {
        for (auto subpath : path->sub_paths) {
            if (subpath.size() < 2) continue;
            for (int i = 0; i < subpath.size() - 1; i++) {
                DrawLine(image, subpath[i], subpath[i + 1], path->strokeWidth, path->strokeColor, path->linecap);
            }

            bool flag = glm::length(subpath[0] - *subpath.rbegin()) <= 1e-6;
            glm::vec2 prev, cur, next;
            for (int i = 0; i < subpath.size() - 1; i++) {
                if (i > 0) {
                    prev = subpath[i - 1];
                    cur = subpath[i];
                    next = subpath[i + 1];
                }
                else if (flag) {
                    prev = subpath[subpath.size() - 2];
                    cur = subpath[0];
                    next = subpath[1];
                } 
                else continue;
                if (path->linejoin == StrokeLinejoin::Round) {
                    Circle joincircle;
                    joincircle.cx = cur.x;
                    joincircle.cy = cur.y;
                    joincircle.fillColor = path->strokeColor;
                    joincircle.strokeColor = {0, 0, 0, 0};
                    joincircle.r = path->strokeWidth * 0.5f;
                    DrawCircle(image, &joincircle);
                }
                else {
                    HandleLineJoin(image, prev, cur, next, path);
                }
            } 
        }
    }

    void SVGRasterizer::DrawPath(Common::ImageRGB& image, Path* path) {
        // std::cout << path->fillColor.r << " " << path->fillColor.g << " " << path->fillColor.b << " " << path->fillColor.a << std::endl;
        int MAXY = image.GetSizeY();
        std::vector<std::vector<Edge> > EdgeTable(MAXY);
        auto Addedge = [&](const glm::vec2& p1, const glm::vec2& p2) {
            if (std::abs(p1.y - p2.y) < 1e-6) return;
            Edge e;
            const glm::vec2& bottom = (p1.y < p2.y) ? p1 : p2;
            const glm::vec2& top = (p1.y < p2.y) ? p2 : p1;
            
            int y_start = static_cast<int>(std::ceil(bottom.y));
            if (y_start < 0) y_start = 0;
            if (y_start >= MAXY or y_start >= top.y) return;

            e.y_max   = top.y;
            e.dx      = (p2.x - p1.x) / (p2.y - p1.y);
            e.x_now   = bottom.x + (static_cast<float>(y_start) - bottom.y) * e.dx;
            e.dir = (p1.y < p2.y) ? 1 : -1;
            EdgeTable[y_start].push_back(e);
        };

        for (auto subpath : path->sub_paths) {
            if (subpath.size() <= 0) continue;
            for (int i = 0; i < subpath.size() - 1; i++) 
                Addedge(subpath[i], subpath[i + 1]);
            if (subpath.back() != subpath.front())
                Addedge(subpath.back(), subpath.front());
        }

        for (int y = 0; y < MAXY; y++) {
            if (EdgeTable[y].size() == 0) continue;
            std::sort(EdgeTable[y].begin(), EdgeTable[y].end());
            int dircount = 0, numbercount = 0;
            int prex = std::ceil(EdgeTable[y][0].x_now), nowx;
            dircount += EdgeTable[y][0].dir;
            numbercount++;
            if (EdgeTable[y][0].y_max > y + 1 and y + 1 < MAXY) {
                EdgeTable[y][0].x_now += EdgeTable[y][0].dx;
                EdgeTable[y + 1].push_back(EdgeTable[y][0]);
            }
            for (int i = 1; i < EdgeTable[y].size(); i++) {
                nowx = std::ceil(EdgeTable[y][i].x_now);
                if ((path->fill_rule == FillRule::EvenOdd and numbercount % 2 == 1) or (path->fill_rule == FillRule::NonZero and dircount != 0) ) {
                    
                    for (int x = prex; x < nowx; x++) {
                        SetPixel(image, x, y, path->fillColor);
                    }
                }
                dircount += EdgeTable[y][i].dir;
                numbercount++;
                if (EdgeTable[y][i].y_max > y + 1 and y + 1 < MAXY) {
                    EdgeTable[y][i].x_now += EdgeTable[y][i].dx;
                    EdgeTable[y + 1].push_back(EdgeTable[y][i]);
                }
                prex = nowx;
            }
        }
        if (path->strokeColor.a > 1e-6 and path->strokeWidth > 1e-6) {
            StrokePath(image, path);
        }
    }
    
    void SVGRasterizer::SetPixel(Common::ImageRGB& image, int x, int y, const glm::vec4 color) {
        if (x >= 0 && x < image.GetSizeX() && y >= 0 && y < image.GetSizeY()) { 
            glm::vec3 bgColor = image.At(x, y);
            image.At(x, y) = glm::vec3(color) * color.a + bgColor * (1.0f - color.a); 
        }
    }
    
}
