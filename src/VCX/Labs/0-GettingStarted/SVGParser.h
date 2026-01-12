#pragma once
#include <string>
#include <vector>
#include "SVGData.h"
#include <tinyxml2.h>
#include <fstream>

namespace VCX::Labs::GettingStarted {
    class SVGParser {
    public:
        static std::vector<Shape*> ParseFile(const std::string& filename, int samplerate);
        static std::pair<int, int> GetSceneSize(const std::string& filename);
    
    private:
        static glm::vec4 ParseColor(const char* hexString);
        static ShapeStyle ParseStyle(tinyxml2::XMLElement* elem);
        static void ParseStyleAttribute(const char* styleStr, ShapeStyle& style);
        static void ParseElement(tinyxml2::XMLElement* elem, std::vector<Shape*>& shapes, const RenderStyle& parent, const glm::mat3& parentTransform);
        static void ParsePath(Path* path, const std::string& d, const glm::mat3& transform, float transformscale);
    };
}
