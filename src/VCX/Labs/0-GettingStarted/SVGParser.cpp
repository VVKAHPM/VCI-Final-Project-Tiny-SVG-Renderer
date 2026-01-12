#include "SVGParser.h"
#include <sstream>
#include <iostream>
#include <string.h>
#include <map>
#include <algorithm>
#include <cctype>

namespace VCX::Labs::GettingStarted {

    enum class Transform { Translate, Scale, Rotate, SkewX, SkewY, Matrix, None };

    class PathAnalyser {
        std::string_view data;
        size_t pos = 0;

    public:
        PathAnalyser(const std::string& _data) : data(_data) {}

        void SkipSpace() {
            while (pos < data.size() and std::isspace(data[pos])) pos++;
        }

        bool Empty() {
            SkipSpace();
            return pos >= data.size();
        }

        char NextCommand() {
            SkipSpace();
            if (Empty()) return 0;
            return data[pos++];
        }

        Transform NextTransform() {
            SkipSpace();
            if (Empty()) return Transform::None;
            if (data[pos] == 't') {
                pos += 9;
                return Transform::Translate;
            }
            else if (data[pos] == 'r') {
                pos += 6;
                return Transform::Rotate;
            }
            else if (data[pos] == 'm') {
                pos += 6;
                return Transform::Matrix;
            }
            else {
                if (pos + 4 >= data.size()) {
                    std::cerr << "Parse Transform Error" << std::endl;
                    pos = data.size();
                    return Transform::None;
                } 
                pos += 5;
                if (data[pos - 1] == 'e') {
                    return Transform::Scale;
                }
                else if (data[pos - 1] == 'X') {
                    return Transform::SkewX;
                }
                else if (data[pos - 1] == 'Y') {
                    return Transform::SkewY;
                }
            }
            pos = data.size();
            std::cerr << "Parse Transform Error" << std::endl;
            return Transform::None;
        }

        float NextFloat() {
            SkipSpace();
            if (Empty()) {
                std::cerr << "Path Analyse Error!" << std::endl;
                return 0;
            }
            const char* begin = data.data() + pos;
            const char* end   = data.data() + data.size();

            float value;
            auto [ptr, ec] = std::from_chars(begin, end, value);

            if (ec != std::errc()) {
                std::cerr << "Path Analyse Error!" << std::endl;
                return 0;
            }

            pos = static_cast<std::size_t>(ptr - data.data());
            return value;
        }

        bool IsNumber() {
            SkipSpace();
            if (Empty()) return false;
            char ch = data[pos];
            return std::isdigit(ch) || ch == '-' || ch == '+' || ch == '.';
        }
    };

    struct Bezier {
        glm::vec2 p[4];
        int degree;
    };

    struct ViewBox {
        float minX, minY, width, height;
        float scale = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;

        void ComputeScale(float X, float Y, float padding) {
            if (width <= 0 or height <= 0) return;
            scale = std::min(X / width, Y / height) * padding;
            offsetX = (X - width * scale) / 2.0f - minX * scale;
            offsetY = (Y - height * scale) / 2.0f - minY * scale;
        }

        glm::vec2 Transform(glm::vec2 p) const {
            return { p.x * scale + offsetX, p.y * scale + offsetY };
        }
    } box;

    int mystrncasecmp(const char* a, const char* b, const int n) {
        if (!a or !b) return 0;
        for (int i = 0; i < n && *a && *b; a++, b++, i++) {
            int ca = std::tolower((unsigned char)*a);
            int cb = std::tolower((unsigned char)*b);
            if (ca != cb) return ca - cb;
        }
        return *a - *b;
    }

    std::string ToLower(const std::string& str) {
        std::string lowerStr = str;
        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return lowerStr;
    }

    static const std::map<std::string, StrokeLinecap> LinecapMap = {
        {"butt", StrokeLinecap::Butt},
        {"square", StrokeLinecap::Square},
        {"round", StrokeLinecap::Round}    
    };

    static const std::map<std::string, StrokeLinejoin> LinejoinMap = {
        {"miter", StrokeLinejoin::Miter},
        {"round", StrokeLinejoin::Round},
        {"bevel", StrokeLinejoin::Bevel}    
    };

    static const std::map<std::string, glm::vec4> g_ColorMap = {
        {"aliceblue", {0.941f, 0.973f, 1.0f, 1.0f}}, {"antiquewhite", {0.98f, 0.922f, 0.843f, 1.0f}},
        {"aqua", {0.0f, 1.0f, 1.0f, 1.0f}}, {"aquamarine", {0.498f, 1.0f, 0.831f, 1.0f}},
        {"azure", {0.941f, 1.0f, 1.0f, 1.0f}}, {"beige", {0.961f, 0.961f, 0.863f, 1.0f}},
        {"bisque", {1.0f, 0.894f, 0.769f, 1.0f}}, {"black", {0.0f, 0.0f, 0.0f, 1.0f}},
        {"blanchedalmond", {1.0f, 0.922f, 0.804f, 1.0f}}, {"blue", {0.0f, 0.0f, 1.0f, 1.0f}},
        {"blueviolet", {0.541f, 0.169f, 0.886f, 1.0f}}, {"brown", {0.647f, 0.165f, 0.165f, 1.0f}},
        {"burlywood", {0.871f, 0.722f, 0.529f, 1.0f}}, {"cadetblue", {0.373f, 0.62f, 0.627f, 1.0f}},
        {"chartreuse", {0.498f, 1.0f, 0.0f, 1.0f}}, {"chocolate", {0.824f, 0.412f, 0.118f, 1.0f}},
        {"coral", {1.0f, 0.498f, 0.314f, 1.0f}}, {"cornflowerblue", {0.392f, 0.584f, 0.929f, 1.0f}},
        {"cornsilk", {1.0f, 0.973f, 0.863f, 1.0f}}, {"crimson", {0.863f, 0.078f, 0.235f, 1.0f}},
        {"cyan", {0.0f, 1.0f, 1.0f, 1.0f}}, {"darkblue", {0.0f, 0.0f, 0.545f, 1.0f}},
        {"darkcyan", {0.0f, 0.545f, 0.545f, 1.0f}}, {"darkgoldenrod", {0.722f, 0.525f, 0.043f, 1.0f}},
        {"darkgray", {0.663f, 0.663f, 0.663f, 1.0f}}, {"darkgreen", {0.0f, 0.392f, 0.0f, 1.0f}}, // 你缺的这个
        {"darkgrey", {0.663f, 0.663f, 0.663f, 1.0f}}, {"darkkhaki", {0.741f, 0.718f, 0.42f, 1.0f}},
        {"darkmagenta", {0.545f, 0.0f, 0.545f, 1.0f}}, {"darkolivegreen", {0.333f, 0.42f, 0.184f, 1.0f}},
        {"darkorange", {1.0f, 0.549f, 0.0f, 1.0f}}, {"darkorchid", {0.6f, 0.196f, 0.8f, 1.0f}},
        {"darkred", {0.545f, 0.0f, 0.0f, 1.0f}}, {"darksalmon", {0.914f, 0.588f, 0.478f, 1.0f}},
        {"darkseagreen", {0.561f, 0.737f, 0.561f, 1.0f}}, {"darkslateblue", {0.282f, 0.239f, 0.545f, 1.0f}},
        {"darkslategray", {0.184f, 0.31f, 0.31f, 1.0f}}, {"darkslategrey", {0.184f, 0.31f, 0.31f, 1.0f}},
        {"darkturquoise", {0.0f, 0.808f, 0.82f, 1.0f}}, {"darkviolet", {0.58f, 0.0f, 0.827f, 1.0f}},
        {"deeppink", {1.0f, 0.078f, 0.576f, 1.0f}}, {"deepskyblue", {0.0f, 0.749f, 1.0f, 1.0f}},
        {"dimgray", {0.412f, 0.412f, 0.412f, 1.0f}}, {"dimgrey", {0.412f, 0.412f, 0.412f, 1.0f}},
        {"dodgerblue", {0.118f, 0.565f, 1.0f, 1.0f}}, {"firebrick", {0.698f, 0.133f, 0.133f, 1.0f}},
        {"floralwhite", {1.0f, 0.98f, 0.941f, 1.0f}}, {"forestgreen", {0.133f, 0.545f, 0.133f, 1.0f}},
        {"fuchsia", {1.0f, 0.0f, 1.0f, 1.0f}}, {"gainsboro", {0.863f, 0.863f, 0.863f, 1.0f}},
        {"ghostwhite", {0.973f, 0.973f, 1.0f, 1.0f}}, {"gold", {1.0f, 0.843f, 0.0f, 1.0f}},
        {"goldenrod", {0.855f, 0.647f, 0.125f, 1.0f}}, {"gray", {0.502f, 0.502f, 0.502f, 1.0f}},
        {"green", {0.0f, 0.502f, 0.0f, 1.0f}}, {"greenyellow", {0.678f, 1.0f, 0.184f, 1.0f}},
        {"grey", {0.502f, 0.502f, 0.502f, 1.0f}}, {"honeydew", {0.941f, 1.0f, 0.941f, 1.0f}},
        {"hotpink", {1.0f, 0.412f, 0.706f, 1.0f}}, {"indianred", {0.804f, 0.361f, 0.361f, 1.0f}},
        {"indigo", {0.294f, 0.0f, 0.51f, 1.0f}}, {"ivory", {1.0f, 1.0f, 0.941f, 1.0f}},
        {"khaki", {0.941f, 0.902f, 0.549f, 1.0f}}, {"lavender", {0.902f, 0.902f, 0.98f, 1.0f}},
        {"lavenderblush", {1.0f, 0.941f, 0.961f, 1.0f}}, {"lawngreen", {0.486f, 0.988f, 0.0f, 1.0f}},
        {"lemonchiffon", {1.0f, 0.98f, 0.804f, 1.0f}}, {"lightblue", {0.678f, 0.847f, 0.902f, 1.0f}},
        {"lightcoral", {0.941f, 0.502f, 0.502f, 1.0f}}, {"lightcyan", {0.878f, 1.0f, 1.0f, 1.0f}},
        {"lightgoldenrodyellow", {0.98f, 0.98f, 0.824f, 1.0f}}, {"lightgray", {0.827f, 0.827f, 0.827f, 1.0f}},
        {"lightgreen", {0.565f, 0.933f, 0.565f, 1.0f}}, {"lightgrey", {0.827f, 0.827f, 0.827f, 1.0f}},
        {"lightpink", {1.0f, 0.714f, 0.757f, 1.0f}}, {"lightsalmon", {1.0f, 0.627f, 0.478f, 1.0f}},
        {"lightseagreen", {0.125f, 0.698f, 0.667f, 1.0f}}, {"lightskyblue", {0.529f, 0.808f, 0.98f, 1.0f}},
        {"lightslategray", {0.467f, 0.533f, 0.6f, 1.0f}}, {"lightslategrey", {0.467f, 0.533f, 0.6f, 1.0f}},
        {"lightsteelblue", {0.69f, 0.769f, 0.871f, 1.0f}}, {"lightyellow", {1.0f, 1.0f, 0.878f, 1.0f}},
        {"lime", {0.0f, 1.0f, 0.0f, 1.0f}}, {"limegreen", {0.196f, 0.804f, 0.196f, 1.0f}},
        {"linen", {0.98f, 0.941f, 0.902f, 1.0f}}, {"magenta", {1.0f, 0.0f, 1.0f, 1.0f}},
        {"maroon", {0.502f, 0.0f, 0.0f, 1.0f}}, {"mediumaquamarine", {0.4f, 0.804f, 0.667f, 1.0f}},
        {"mediumblue", {0.0f, 0.0f, 0.804f, 1.0f}}, {"mediumorchid", {0.729f, 0.333f, 0.827f, 1.0f}},
        {"mediumpurple", {0.576f, 0.439f, 0.859f, 1.0f}}, {"mediumseagreen", {0.235f, 0.702f, 0.443f, 1.0f}},
        {"mediumslateblue", {0.482f, 0.408f, 0.933f, 1.0f}}, {"mediumspringgreen", {0.0f, 0.98f, 0.604f, 1.0f}},
        {"mediumturquoise", {0.282f, 0.82f, 0.8f, 1.0f}}, {"mediumvioletred", {0.78f, 0.082f, 0.522f, 1.0f}},
        {"midnightblue", {0.098f, 0.098f, 0.439f, 1.0f}}, {"mintcream", {0.961f, 1.0f, 0.98f, 1.0f}},
        {"mistyrose", {1.0f, 0.894f, 0.882f, 1.0f}}, {"moccasin", {1.0f, 0.894f, 0.714f, 1.0f}},
        {"navajowhite", {1.0f, 0.871f, 0.678f, 1.0f}}, {"navy", {0.0f, 0.0f, 0.502f, 1.0f}},
        {"none", {0.0f, 0.0f, 0.0f, 0.0f}}, {"oldlace", {0.992f, 0.961f, 0.902f, 1.0f}},
        {"olive", {0.502f, 0.502f, 0.0f, 1.0f}}, {"olivedrab", {0.42f, 0.557f, 0.137f, 1.0f}},
        {"orange", {1.0f, 0.647f, 0.0f, 1.0f}}, {"orangered", {1.0f, 0.271f, 0.0f, 1.0f}},
        {"orchid", {0.855f, 0.439f, 0.839f, 1.0f}}, {"palegoldenrod", {0.933f, 0.91f, 0.667f, 1.0f}},
        {"palegreen", {0.596f, 0.984f, 0.596f, 1.0f}}, {"paleturquoise", {0.686f, 0.933f, 0.933f, 1.0f}},
        {"palevioletred", {0.859f, 0.439f, 0.576f, 1.0f}}, {"papayawhip", {1.0f, 0.937f, 0.835f, 1.0f}},
        {"peachpuff", {1.0f, 0.855f, 0.725f, 1.0f}}, {"peru", {0.804f, 0.522f, 0.247f, 1.0f}},
        {"pink", {1.0f, 0.753f, 0.796f, 1.0f}}, {"plum", {0.867f, 0.627f, 0.867f, 1.0f}},
        {"powderblue", {0.69f, 0.878f, 0.902f, 1.0f}}, {"purple", {0.502f, 0.0f, 0.502f, 1.0f}},
        {"rebeccapurple", {0.4f, 0.2f, 0.6f, 1.0f}}, {"red", {1.0f, 0.0f, 0.0f, 1.0f}},
        {"rosybrown", {0.737f, 0.561f, 0.561f, 1.0f}}, {"royalblue", {0.255f, 0.412f, 0.882f, 1.0f}},
        {"saddlebrown", {0.545f, 0.271f, 0.075f, 1.0f}}, {"salmon", {0.98f, 0.502f, 0.447f, 1.0f}},
        {"sandybrown", {0.957f, 0.643f, 0.376f, 1.0f}}, {"seagreen", {0.18f, 0.545f, 0.341f, 1.0f}},
        {"seashell", {1.0f, 0.961f, 0.933f, 1.0f}}, {"sienna", {0.627f, 0.322f, 0.176f, 1.0f}},
        {"silver", {0.753f, 0.753f, 0.753f, 1.0f}}, {"skyblue", {0.529f, 0.808f, 0.922f, 1.0f}},
        {"slateblue", {0.416f, 0.353f, 0.804f, 1.0f}}, {"slategray", {0.439f, 0.502f, 0.565f, 1.0f}},
        {"slategrey", {0.439f, 0.502f, 0.565f, 1.0f}}, {"snow", {1.0f, 0.98f, 0.98f, 1.0f}},
        {"springgreen", {0.0f, 1.0f, 0.498f, 1.0f}}, {"steelblue", {0.275f, 0.51f, 0.706f, 1.0f}},
        {"tan", {0.824f, 0.706f, 0.549f, 1.0f}}, {"teal", {0.0f, 0.502f, 0.502f, 1.0f}},
        {"thistle", {0.847f, 0.749f, 0.847f, 1.0f}}, {"tomato", {1.0f, 0.388f, 0.278f, 1.0f}},
        {"transparent", {0.0f, 0.0f, 0.0f, 0.0f}}, {"turquoise", {0.251f, 0.878f, 0.816f, 1.0f}},
        {"violet", {0.933f, 0.51f, 0.933f, 1.0f}}, {"wheat", {0.961f, 0.871f, 0.702f, 1.0f}},
        {"white", {1.0f, 1.0f, 1.0f, 1.0f}}, {"whitesmoke", {0.961f, 0.961f, 0.961f, 1.0f}},
        {"yellow", {1.0f, 1.0f, 0.0f, 1.0f}}, {"yellowgreen", {0.604f, 0.804f, 0.196f, 1.0f}}
    };

    glm::vec4 SVGParser::ParseColor(const char* colorString) {
        if (!colorString) return {0, 0, 0, 1};
        std::string raw = colorString;
        // Hex style
        if (raw[0] == '#') {
            std::string hex = raw.substr(1);
            unsigned int r = 0, g = 0, b = 0;
            std::stringstream ss;
            if (hex.length() == 3) {
                 std::string fullHex = "";
                 fullHex += hex[0]; fullHex += hex[0];
                 fullHex += hex[1]; fullHex += hex[1];
                 fullHex += hex[2]; fullHex += hex[2];
                 hex = fullHex;
            }

            ss << std::hex << hex.substr(0, 2); ss >> r; ss.clear();
            ss << std::hex << hex.substr(2, 2); ss >> g; ss.clear();
            ss << std::hex << hex.substr(4, 2); ss >> b;
            return {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
        }

        // string style
        std::string lowerName = ToLower(raw);
        auto it = g_ColorMap.find(lowerName);
        if (it != g_ColorMap.end()) {
            return it->second;
        }

        // rgb style
        if (lowerName.find("rgb(") == 0) {
            int r, g, b;
            if (sscanf(raw.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3 ||
                sscanf(raw.c_str(), "rgb(%d, %d, %d)", &r, &g, &b) == 3) {
                return {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
            }
        }

        // rgba style
        if (lowerName.find("rgba(") == 0) {
            int r, g, b;
            float a;
            if (sscanf(raw.c_str(), "rgba(%d,%d,%d,%f)", &r, &g, &b, &a) == 4 ||
                sscanf(raw.c_str(), "rgba(%d, %d, %d, %f)", &r, &g, &b, &a) == 4) {
                return {r / 255.0f, g / 255.0f, b / 255.0f, a};
            }
        }

        std::cerr << "Warning: Unknown Color format:" << colorString << std::endl;

        return {0, 0, 0, 1};
    }

    std::string Trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        auto end   = s.find_last_not_of(" \t\r\n");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }
    
    void SVGParser::ParseStyleAttribute(const char* styleStr, ShapeStyle& style) {
        if (!styleStr) return;
        
        std::stringstream ss(styleStr);
        std::string item;
        while (std::getline(ss, item, ';')) {
            size_t colonPos = item.find(':');
            if (colonPos == std::string::npos) continue;
    
            std::string key = Trim(item.substr(0, colonPos));
            std::string val = Trim(item.substr(colonPos + 1));
    
            if (key == "fill") {
                style.fill = SVGParser::ParseColor(val.c_str());
            } else if (key == "stroke") {
                style.stroke = SVGParser::ParseColor(val.c_str());
            } else if (key == "stroke-width") {
                style.strokeWidth = std::stof(val);
            } else if (key == "opacity") {
                style.opacity = std::stof(val);
            } else if (key == "fill-opacity") {
                style.fillOpacity = std::stof(val);
            } else if (key == "stroke-opacity") {
                style.strokeOpacity = std::stof(val);
            } else if (key == "stroke-linecap") {
                style.strokeLinecap = LinecapMap.find(val)->second;
            } else if (key == "stroke-linejoin") {
                style.strokeLinejoin = LinejoinMap.find(val)->second;
            }
        }
    }

    ShapeStyle SVGParser::ParseStyle(tinyxml2::XMLElement* elem) {
        ShapeStyle style;
        if (auto f = elem->Attribute("fill")) style.fill = SVGParser::ParseColor(f);
        if (auto s = elem->Attribute("stroke")) style.stroke = SVGParser::ParseColor(s);
        if (elem->Attribute("stroke-width")) style.strokeWidth = elem->FloatAttribute("stroke-width");
        if (elem->Attribute("opacity")) style.opacity = elem->FloatAttribute("opacity");
        if (elem->Attribute("fill-opacity")) style.fillOpacity = elem->FloatAttribute("fill-opacity");
        if (elem->Attribute("stroke-opacity")) style.strokeOpacity = elem->FloatAttribute("stroke-opacity");
        if (auto stylestr = elem->Attribute("style")) {
            ParseStyleAttribute(stylestr, style);
        }
        if (auto lc = elem->Attribute("stroke-linecap")) {
            style.strokeLinecap = LinecapMap.find(std::string(lc))->second;
        }
        if (auto jc = elem->Attribute("stroke-linejoin")) {
            style.strokeLinejoin = LinejoinMap.find(std::string(jc))->second;
        }
        return style;
    }

    RenderStyle InheritStyle(const RenderStyle& parent, const ShapeStyle& local) {
        RenderStyle result = parent;
        if (local.fill) result.fill = *local.fill;
        if (local.stroke) result.stroke = *local.stroke;
        if (local.strokeWidth) result.strokeWidth = *local.strokeWidth;
        if (local.opacity) result.totalOpacity *= (*local.opacity);
        if (local.fillOpacity) {
            result.fill.a = (*local.fillOpacity);
        }
        if (local.strokeOpacity) {
            result.stroke.a = (*local.strokeOpacity);
        }
        if (local.strokeLinecap) {
            result.linecap = (*local.strokeLinecap);
        }
        if (local.strokeLinejoin) {
            result.linejoin = (*local.strokeLinejoin);
        }
        return result;
    }

    glm::vec2 ApplyTransform(glm::vec3 p, const glm::mat3& transform) {
        p = transform * p;
        return glm::vec2(p);
    }

    void SubdivideBezier(const Bezier& b, Bezier& left, Bezier& right) {
        int d = b.degree;
        left.degree = right.degree = d;

        glm::vec2 v[4][4];
        for (int i = 0; i <= d; i++) v[0][i] = b.p[i];
        for (int j = 1; j <= d; j++) {
            for (int i = 0; i <= d - j; i++) {
                v[j][i] = (v[j - 1][i] + v[j - 1][i + 1]) * 0.5f;
            }
        }

        for (int i = 0; i <= d; i++) left.p[i] = v[i][0];
        for (int i = 0; i <= d; i++) right.p[i] = v[d - i][i];
    }

    bool IsFlat(const Bezier& b, float transformscale) {
        int d = b.degree;
        glm::vec2 start = b.p[0];
        glm::vec2 end = b.p[d];
        glm::vec2 line = end - start;
        float line_length = glm::length(line);
        if (line_length < 1e-6) return true;

        for (int i = 1; i < b.degree; i++) {
            glm::vec2 p = b.p[i];
            float dist = std::abs(line.x * (p.y - start.y) - line.y * (p.x - start.x)) / line_length;
            if (dist * box.scale * transformscale> 0.5f) return false;
        }

        return true;
    }

    void ParseBezier(const Bezier& b, std::vector<glm::vec2>& points, const glm::mat3& transform, float scale) {
        if (IsFlat(b, scale)) {
            points.push_back(box.Transform(ApplyTransform(glm::vec3{b.p[b.degree], 1}, transform)));
            return;
        }

        Bezier left, right;
        SubdivideBezier(b, left, right);
        ParseBezier(left, points, transform, scale);
        ParseBezier(right, points, transform, scale);
    }

    void ParseArc(const glm::vec2& p0, const glm::vec2& p1, float rx, float ry, float angle, bool large_flag, bool sweep_flag, std::vector<glm::vec2>& points, const glm::mat3& transform) {
        if (p0 == p1) return;
        rx = glm::abs(rx), ry = glm::abs(ry);
        if (rx < 1e-6 || ry < 1e-6) {
            points.push_back(box.Transform(p1));
            return;
        }
        float phi = angle / 180 * glm::pi<float>();
        float cosv = glm::cos(phi);
        float sinv = glm::sin(phi);
        glm::vec2 p = (p0 - p1) * 0.5f;
        glm::vec2 rotatep = {cosv * p.x + sinv * p.y, -sinv * p.x + cosv * p.y};
        float delta = (rotatep.x * rotatep.x) / (rx * rx) + (rotatep.y * rotatep.y) / (ry * ry);
        if (delta > 1.0) {
            rx *= std::sqrt(delta);
            ry *= std::sqrt(delta);
            delta = 1.0;
        }

        glm::vec2 rotatecentre = std::sqrt(1 / delta - 1) * glm::vec2(rx * rotatep.y / ry, -ry * rotatep.x / rx);
        if (!(large_flag ^ sweep_flag)) rotatecentre = -rotatecentre;
        glm::vec2 centre = glm::vec2{cosv * rotatecentre.x - sinv * rotatecentre.y, sinv * rotatecentre.x + cosv * rotatecentre.y} + 0.5f * (p0 + p1);
        
        auto get_angle = [](glm::vec2 u, glm::vec2 v) {
            float cosvalue = glm::dot(u, v) / glm::length(u) / glm::length(v);
            float res = glm::acos(std::clamp(cosvalue, -1.0f, 1.0f));
            if (u.x * v.y - u.y * v.x < 0) res = -res;
            return res;
        };
        glm::vec2 u = {(rotatep.x - rotatecentre.x) / rx, (rotatep.y - rotatecentre.y) / ry};
        glm::vec2 v = {(-rotatep.x - rotatecentre.x) / rx, (-rotatep.y - rotatecentre.y) / ry};
        
        float theta1 = get_angle({1, 0}, u);
        float dTheta = get_angle(u, v);
        if (sweep_flag and dTheta < 0) dTheta += 2 * glm::pi<float>();
        if (!sweep_flag and dTheta > 0) dTheta -= 2 * glm::pi<float>();

        int t = std::max(1, (int)(std::abs(dTheta) * 30 * box.scale));
        for (int i = 1; i <= t; i++) {
            float theta = theta1 + dTheta * (i / (float)t);
            float x = rx * std::cos(theta);
            float y = ry * std::sin(theta);
            float fx = cosv * x - sinv * y + centre.x;
            float fy = sinv * x + cosv * y + centre.y;
            points.push_back(box.Transform(ApplyTransform(glm::vec3(fx, fy, 1), transform)));
        }
    }

    void SVGParser::ParsePath(Path* path, const std::string& d, const glm::mat3& transform, float transformscale) {
        PathAnalyser T(d);
        char command = 0, lastcommand = 0;
        glm::vec2 currentPos(0, 0);
        glm::vec2 startPos(0, 0);
        glm::vec2 lastControl(0, 0);

        while (!T.Empty()) {
            // deal with omitted command
            if  (!T.IsNumber()) {
                command = T.NextCommand();
            }
            else {
                if (command == 'M') command = 'L';
                if (command == 'm') command = 'l';
            }

            if (command == 'M' or command == 'm') {
                float x = T.NextFloat();
                float y = T.NextFloat();
                if (command == 'm') currentPos += glm::vec2(x, y);
                else currentPos = glm::vec2(x, y);
                startPos = currentPos;
                path->sub_paths.push_back({});
                path->sub_paths.back().push_back(box.Transform(ApplyTransform({currentPos, 1}, transform)));
            }

            if (command == 'L' or command == 'l') {
                float x = T.NextFloat();
                float y = T.NextFloat();
                if (command == 'l') currentPos += glm::vec2(x, y);
                else currentPos = glm::vec2(x, y);
                path->sub_paths.back().push_back(box.Transform(ApplyTransform({currentPos, 1}, transform)));
            }

            if (command == 'H' or command == 'h') {
                float x = T.NextFloat();
                if (command == 'h') currentPos.x += x;
                else currentPos.x = x;
                path->sub_paths.back().push_back(box.Transform(ApplyTransform({currentPos, 1}, transform)));
            }

            if (command == 'V' or command == 'v') {
                float y = T.NextFloat();
                if (command == 'v') currentPos.y += y;
                else currentPos.y = y;
                path->sub_paths.back().push_back(box.Transform(ApplyTransform({currentPos, 1}, transform)));
            }

            if (command == 'Z' or command == 'z') {
                currentPos = startPos;
                path->sub_paths.back().push_back(box.Transform(ApplyTransform({currentPos, 1}, transform)));
            }

            Bezier b;
            b.degree = 0;
            if (command == 'C' or command == 'c') {
                b.degree = 3;
                b.p[0] = currentPos;
                for (int i = 1; i <= 3; i++) {
                    b.p[i] = glm::vec2{ T.NextFloat(), T.NextFloat() };
                    if (command == 'c') b.p[i] += currentPos;
                }
                lastControl = b.p[2];
                currentPos = b.p[3];
            }
            if (command == 'Q' or command == 'q') {
                b.degree = 2;
                b.p[0] = currentPos;
                for (int i = 1; i <= 2; i++) {
                    b.p[i] = glm::vec2{ T.NextFloat(), T.NextFloat() };
                    if (command == 'q') b.p[i] += currentPos;
                }
                lastControl = b.p[1];
                currentPos = b.p[2];
            }
            if (command == 'S' or command == 's') {
                b.degree = 3;
                b.p[0] = currentPos;
                if (lastcommand == 'C' or lastcommand == 'c' or lastcommand == 'S' or lastcommand == 's')
                    b.p[1] = 2.0f * currentPos - lastControl;
                else 
                    b.p[1] = currentPos;
                for (int i = 2; i <= 3; i++) {
                    b.p[i] = glm::vec2{ T.NextFloat(), T.NextFloat() };
                    if (command == 's') b.p[i] += currentPos;
                }
                lastControl = b.p[2];
                currentPos = b.p[3];
            }
            if (command == 'T' or command == 't') {
                b.degree = 2;
                b.p[0] = currentPos;
                if (lastcommand == 'Q' or lastcommand == 'q' or lastcommand == 'T' or lastcommand == 't')
                    b.p[1] = 2.0f * currentPos - lastControl;
                else 
                    b.p[1] = currentPos;
                for (int i = 2; i <= 2; i++) {
                    b.p[i] = glm::vec2{ T.NextFloat(), T.NextFloat() };
                    if (command == 't') b.p[i] += currentPos;
                }
                lastControl = b.p[1];
                currentPos = b.p[2];
            }
            if (command == 'A' or command == 'a') {
                float rx = T.NextFloat();
                float ry = T.NextFloat();
                float phi = T.NextFloat();
                int large_flag = T.NextFloat();
                int sweep_flag = T.NextFloat();
                glm::vec2 p1 = {T.NextFloat(), T.NextFloat()};
                if (command == 'a') p1 += currentPos;
                ParseArc(currentPos, p1, rx, ry, phi, large_flag != 0, sweep_flag != 0, path->sub_paths.back(), transform);
                currentPos = p1;
            }
            if (b.degree != 0) ParseBezier(b, path->sub_paths.back(), transform, transformscale);
            lastcommand = command;
        }
    }

    void ParseTransform(std::string s, glm::mat3& local) {
        PathAnalyser T(s);
        float tx, ty, cx, cy, sx, sy, angle;
        while (!T.Empty()) {
            glm::mat3 matrix(1.0f);
            Transform t = T.NextTransform();
            switch (t) {
                case Transform::Translate:
                    tx = T.NextFloat();
                    ty = 0;
                    if (T.IsNumber()) ty = T.NextFloat();
                    matrix[2][0] = tx;
                    matrix[2][1] = ty;
                    break;
                case Transform::Scale:
                    sx = T.NextFloat();
                    sy = sx;
                    if (T.IsNumber()) sy = T.NextFloat();
                    matrix[0][0] = sx;
                    matrix[1][1] = sy;
                    break;
                case Transform::Rotate:
                    angle = T.NextFloat() * glm::pi<float>() / 180.0f;
                    cx = 0, cy = 0;
                    if (T.IsNumber()) {
                        cx = T.NextFloat();
                        cy = T.NextFloat();
                    }
                    matrix[0][0] = glm::cos(angle);
                    matrix[1][0] = -glm::sin(angle);
                    matrix[0][1] = glm::sin(angle);
                    matrix[1][1] = glm::cos(angle);
                    if (cx != 0 or cy != 0) {
                        glm::mat3 t1(1.0f), t2(1.0f);
                        t1[2][0] = cx;
                        t1[2][1] = cy;
                        t2[2][0] = -cx;
                        t2[2][1] = -cy;
                        matrix = t1 * matrix * t2;
                    }
                    break;
                case Transform::SkewX:
                    angle = T.NextFloat() * glm::pi<float>() / 180.0f;
                    matrix[1][0] = glm::tan(angle);
                    break;
                case Transform::SkewY:
                    angle = T.NextFloat() * glm::pi<float>() / 180.0f;
                    matrix[0][1] = glm::tan(angle);
                    break;
                case Transform::Matrix:
                    matrix[0][0] = T.NextFloat();
                    matrix[0][1] = T.NextFloat();
                    matrix[1][0] = T.NextFloat();
                    matrix[1][1] = T.NextFloat();
                    matrix[2][0] = T.NextFloat();
                    matrix[2][1] = T.NextFloat();
                    break;
            
                default:
                    break;
            }
            local = local * matrix;
        }
    }
    
    void SVGParser::ParseElement(tinyxml2::XMLElement* elem, std::vector<Shape*>& shapes, const RenderStyle& parent, const glm::mat3& parentTransform) {
        if (!elem) return;
        auto local = ParseStyle(elem);
        auto state = InheritStyle(parent, local);
        glm::mat3 localTransform = glm::mat3(1.0f);
        auto transformstr = elem->Attribute("transform");
        if (transformstr) {
            std::string transforms = transformstr;
            for (char& c : transforms) {
                if (c == '(' or c == ')' or c == ',') c = ' ';
            }
            ParseTransform(transforms, localTransform);
        }
        localTransform = parentTransform * localTransform;
        float transformScale = std::sqrt(std::abs(glm::determinant(localTransform)));
        

        Shape* shape = nullptr;
        std::string name = elem->Name();
        if (name == "rect") {
            float x = elem->FloatAttribute("x");
            float y = elem->FloatAttribute("y");
            float width = elem->FloatAttribute("width");
            float height = elem->FloatAttribute("height");

            Path* path = new Path();
            path->sub_paths.push_back({});
            path->sub_paths.back().push_back(box.Transform(ApplyTransform({x, y, 1}, localTransform)));
            path->sub_paths.back().push_back(box.Transform(ApplyTransform({x + width, y, 1}, localTransform)));
            path->sub_paths.back().push_back(box.Transform(ApplyTransform({x + width, y + height, 1}, localTransform)));
            path->sub_paths.back().push_back(box.Transform(ApplyTransform({x, y + height, 1}, localTransform)));
            path->sub_paths.back().push_back(box.Transform(ApplyTransform({x, y, 1}, localTransform)));
            shape = path;
        }
        else if (name == "circle" || name == "ellipse") {
            float cx = elem->FloatAttribute("cx");
            float cy = elem->FloatAttribute("cy");
            float rx = (name == "circle") ? elem->FloatAttribute("r") : elem->FloatAttribute("rx");
            float ry = (name == "circle") ? rx : elem->FloatAttribute("ry");

            Path* path = new Path();
            path->sub_paths.push_back({});
            auto& pts = path->sub_paths.back();
            float worldRadius = std::max(rx, ry) * box.scale * transformScale;
            int N = worldRadius * 20.0f; 
            for (int i = 0; i < N; ++i) {
                float theta = 2.0f * glm::pi<float>() * i / N;
                float x = cx + rx * std::cos(theta);
                float y = cy + ry * std::sin(theta);
                pts.push_back(box.Transform(ApplyTransform({x, y, 1}, localTransform)));
            } 
            pts.push_back(pts.front());
            shape = path;
        }
        else if (name == "path") {
            Path* path = new Path();
            std::string d = elem->Attribute("d");
            std::replace(d.begin(), d.end(), ',', ' ');
            ParsePath(path, d, localTransform, transformScale);
            const char *fillruleAttr = elem->Attribute("fill-rule");
            if (!fillruleAttr) path->fill_rule = FillRule::NonZero;
            else {
                if (mystrncasecmp(fillruleAttr, "nonzero", 8) == 0) path->fill_rule = FillRule::NonZero;
                else path->fill_rule = FillRule::EvenOdd;
            }
            shape = path;
        }
        else if (name == "polygon") {
            Path* path = new Path();
            path->sub_paths.push_back({});
            const char* pointstr = elem->Attribute("points");
            if (pointstr) {
                std::string points = pointstr;
                std::replace(points.begin(), points.end(), ',', ' ');
                PathAnalyser T(points);
                while (!T.Empty()) {
                    float x = T.NextFloat();
                    float y = T.NextFloat();
                    path->sub_paths.back().push_back(box.Transform(ApplyTransform(glm::vec3{x, y, 1}, localTransform)));
                }
                path->sub_paths.back().push_back(path->sub_paths.back().front());
            }
            shape = path;
        }
        else if (name == "polyline") {
            Path* path = new Path();
            path->sub_paths.push_back({});
            const char* pointstr = elem->Attribute("points");
            if (pointstr) {
                std::string points = pointstr;
                std::replace(points.begin(), points.end(), ',', ' ');
                PathAnalyser T(points);
                while (!T.Empty()) {
                    float x = T.NextFloat();
                    float y = T.NextFloat();
                    path->sub_paths.back().push_back(box.Transform(ApplyTransform(glm::vec3{x, y, 1}, localTransform)));
                }
            }
            const char *fillruleAttr = elem->Attribute("fill-rule");
            if (!fillruleAttr) path->fill_rule = FillRule::NonZero;
            else {
                if (mystrncasecmp(fillruleAttr, "nonzero", 8) == 0) path->fill_rule = FillRule::NonZero;
                else path->fill_rule = FillRule::EvenOdd;
            }
            shape = path;
        } 
        else if (name == "line") {
            Path* path = new Path();
            float x1 = elem->FloatAttribute("x1");
            float x2 = elem->FloatAttribute("x2");
            float y1 = elem->FloatAttribute("y1");
            float y2 = elem->FloatAttribute("y2");
            path->sub_paths.push_back({});
            path->sub_paths.back().push_back(box.Transform(ApplyTransform(glm::vec3{x1, y1, 1}, localTransform)));
            path->sub_paths.back().push_back(box.Transform(ApplyTransform(glm::vec3{x2, y2, 1}, localTransform)));
            shape = path;
        }
        else if (name == "g") {
        }

        if (shape) {
            shape->fillColor = state.fill;
            shape->fillColor.a *= state.totalOpacity;
            shape->strokeColor = state.stroke;
            shape->strokeColor.a *= state.totalOpacity;
            shape->strokeWidth = state.strokeWidth * box.scale * transformScale;
            shape->linecap = state.linecap;
            shape->linejoin = state.linejoin;
            shapes.push_back(shape);
        }

        tinyxml2::XMLElement* child = elem->FirstChildElement();
        while (child) {
            ParseElement(child, shapes, state, localTransform);
            child = child->NextSiblingElement();
        }
    }

    #ifdef _WIN32
    #include <windows.h>
    // 将 UTF-8 转换为 UTF-16 的辅助函数
    std::wstring Utf8ToWstring(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring strTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &strTo[0], size_needed);
        return strTo;
    }
    #endif

    std::vector<Shape*> SVGParser::ParseFile(const std::string& filename, int samplerate) {
        std::vector<Shape*> shapes;
        tinyxml2::XMLDocument doc;

        tinyxml2::XMLError error;

        #ifdef _WIN32
            std::wstring wfilename = Utf8ToWstring(filename);
            FILE* fp = _wfopen(wfilename.c_str(), L"rb");
            if (!fp) {
                std::cerr << "Failed to load SVG file: " << filename << std::endl;
                return shapes;
            }
            error = doc.LoadFile(fp);
            fclose(fp);
        #else
            error = doc.LoadFile(filename.c_str());
        #endif

        if (error != tinyxml2::XML_SUCCESS) {
            std::cerr << "Failed to load SVG file: " << filename << std::endl;
            return shapes;
        }

        tinyxml2::XMLElement* root = doc.RootElement(); // <svg>
        if (!root) return shapes;
    
        const char* ViewBoxstr = root->Attribute("viewBox");
        box = ViewBox();
        box.scale = 1.0f, box.offsetX = box.offsetY = 0.0f;
        box.minX = box.minY = 0;
        box.width = 800, box.height = 600;
        if (ViewBoxstr) {
            std::string asstring = ViewBoxstr;
            std::replace(asstring.begin(), asstring.end(), ',', ' ');
            sscanf(asstring.c_str(), "%f %f %f %f", &box.minX, &box.minY, &box.width, &box.height);
        }
        float canvasWidth = root->IntAttribute("width", 800);
        float canvasHeight = root->IntAttribute("height", 600);
        canvasWidth /= 1.1, canvasHeight /= 1.1;
        box.ComputeScale(canvasWidth * samplerate, canvasHeight * samplerate, 0.9);
        ParseElement(root, shapes, {}, glm::mat3(1.0f));

        return shapes;
    }

    std::pair<int, int> SVGParser::GetSceneSize(const std::string& filename) {
        tinyxml2::XMLDocument doc;
        int x, y;
        tinyxml2::XMLError error;

        #ifdef _WIN32
            std::wstring wfilename = Utf8ToWstring(filename);
            FILE* fp = _wfopen(wfilename.c_str(), L"rb");
            if (!fp) {
                std::cerr << "Failed to load SVG file: " << filename << std::endl;
                return {-1, -1};
            }
            error = doc.LoadFile(fp);
            fclose(fp);
        #else
            error = doc.LoadFile(filename.c_str());
        #endif

        if (error != tinyxml2::XML_SUCCESS) {
            std::cerr << "Failed to load SVG file: " << filename << std::endl;
            return {-1, -1};
        }

        tinyxml2::XMLElement* root = doc.RootElement();
        std::string name = root->Name();
        if (name == "svg") {
            x = root->IntAttribute("width", -1);
            y = root->IntAttribute("height", -1);
            return {x, y};
        }
        else return {-1, -1};
    }
}