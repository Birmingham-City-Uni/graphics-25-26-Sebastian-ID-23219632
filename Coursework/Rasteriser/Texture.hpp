#pragma once

#include "MathUtils.hpp"
#include <string>
#include <vector>

struct CpuTexture {
    std::string filename;
    unsigned width = 0;
    unsigned height = 0;
    std::vector<uint8_t> rgba;

    bool valid() const {
        return width > 0 && height > 0 && !rgba.empty();
    }
};

void setPixel(std::vector<uint8_t>& image, int x, int y, int width, const Color& c);
Color getPixel(const std::vector<uint8_t>& image, int x, int y, int width);
CpuTexture loadTexture(const std::string& filename);
Color sampleTextureNearest(const CpuTexture& tex, const Vec2& uv);
Color sampleTextureBilinear(const CpuTexture& tex, const Vec2& uv);
Vec3 srgbToLinear(const Color& c);
Color linearToSrgb(const Vec3& c);
