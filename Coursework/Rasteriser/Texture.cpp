#include "Texture.hpp"
#include <iostream>
#include <lodepng.h>

void setPixel(std::vector<uint8_t>& image, int x, int y, int width, const Color& c) {
    const int idx = 4 * (x + y * width);
    image[idx + 0] = c.r;
    image[idx + 1] = c.g;
    image[idx + 2] = c.b;
    image[idx + 3] = c.a;
}

Color getPixel(const std::vector<uint8_t>& image, int x, int y, int width) {
    const int idx = 4 * (x + y * width);
    return { image[idx + 0], image[idx + 1], image[idx + 2], image[idx + 3] };
}

CpuTexture loadTexture(const std::string& filename) {
    CpuTexture tex;
    tex.filename = filename;

    std::string name = lower(basenameOf(filename));
    bool isPng = name.size() >= 4 && name.substr(name.size() - 4) == ".png";

    if (!isPng) {
        std::cerr << "Warning: skipping non-PNG map_Kd texture: " << filename << "\n";
        return tex;
    }

    unsigned error = lodepng::decode(tex.rgba, tex.width, tex.height, filename);
    if (error) {
        std::cerr << "Warning: failed to load texture '" << filename << "': "
            << lodepng_error_text(error) << "\n";
        tex.rgba.clear();
        tex.width = 0;
        tex.height = 0;
    }

    return tex;
}

Color sampleTextureNearest(const CpuTexture& tex, const Vec2& uv) {
    if (!tex.valid()) {
        return { 200, 200, 200, 255 };
    }

    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    int x = std::min(static_cast<int>(u * tex.width), static_cast<int>(tex.width) - 1);
    int y = std::min(static_cast<int>((1.0f - v) * tex.height), static_cast<int>(tex.height) - 1);

    return getPixel(tex.rgba, x, y, static_cast<int>(tex.width));
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static Color lerpColor(const Color& a, const Color& b, float t) {
    return {
        static_cast<uint8_t>(lerp(static_cast<float>(a.r), static_cast<float>(b.r), t)),
        static_cast<uint8_t>(lerp(static_cast<float>(a.g), static_cast<float>(b.g), t)),
        static_cast<uint8_t>(lerp(static_cast<float>(a.b), static_cast<float>(b.b), t)),
        static_cast<uint8_t>(lerp(static_cast<float>(a.a), static_cast<float>(b.a), t))
    };
}

Color sampleTextureBilinear(const CpuTexture& tex, const Vec2& uv) {
    if (!tex.valid()) {
        return { 200, 200, 200, 255 };
    }

    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    float x = u * static_cast<float>(tex.width - 1);
    float y = (1.0f - v) * static_cast<float>(tex.height - 1);

    int x0 = std::max(0, std::min(static_cast<int>(std::floor(x)), static_cast<int>(tex.width) - 1));
    int y0 = std::max(0, std::min(static_cast<int>(std::floor(y)), static_cast<int>(tex.height) - 1));
    int x1 = std::max(0, std::min(x0 + 1, static_cast<int>(tex.width) - 1));
    int y1 = std::max(0, std::min(y0 + 1, static_cast<int>(tex.height) - 1));

    float tx = x - static_cast<float>(x0);
    float ty = y - static_cast<float>(y0);

    Color c00 = getPixel(tex.rgba, x0, y0, static_cast<int>(tex.width));
    Color c10 = getPixel(tex.rgba, x1, y0, static_cast<int>(tex.width));
    Color c01 = getPixel(tex.rgba, x0, y1, static_cast<int>(tex.width));
    Color c11 = getPixel(tex.rgba, x1, y1, static_cast<int>(tex.width));

    Color top = lerpColor(c00, c10, tx);
    Color bottom = lerpColor(c01, c11, tx);
    return lerpColor(top, bottom, ty);
}

Vec3 srgbToLinear(const Color& c) {
    return {
        std::pow(c.r / 255.0f, 2.2f),
        std::pow(c.g / 255.0f, 2.2f),
        std::pow(c.b / 255.0f, 2.2f)
    };
}

Color linearToSrgb(const Vec3& c) {
    return {
        static_cast<uint8_t>(clamp01(std::pow(std::max(c.x, 0.0f), 1.0f / 2.2f)) * 255.0f),
        static_cast<uint8_t>(clamp01(std::pow(std::max(c.y, 0.0f), 1.0f / 2.2f)) * 255.0f),
        static_cast<uint8_t>(clamp01(std::pow(std::max(c.z, 0.0f), 1.0f / 2.2f)) * 255.0f),
        255
    };
}
