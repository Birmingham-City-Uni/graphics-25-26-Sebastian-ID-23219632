#pragma once

#include "Camera.hpp"
#include "ObjLoader.hpp"
#include <string>
#include <vector>

struct RenderOptions {
    bool enableBackFaceCulling = true;
    bool useBilinearTextureFiltering = true;
    bool enableFog = false;
    Vec3 fogColour{ 0.72f, 0.62f, 0.54f };
    float fogStart = 25.0f;
    float fogEnd = 120.0f;
    std::vector<std::string> skipObjects;
};

struct ModelTransform {
    float scale = 1.0f;
    Vec3 translate{ 0.0f, 0.0f, 0.0f };
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float rollDegrees = 0.0f;
};

class Rasteriser {
public:
    Rasteriser(int width, int height);

    void clear(const Color& colour);
    void renderMesh(const Mesh& mesh, const CameraSettings& camera, const ModelTransform& transform, const RenderOptions& options);
    void saveColourBuffer(const std::string& filename) const;
    void saveZBuffer(const std::string& filename) const;

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    struct ScreenVertex {
        Vec3 screen;
        Vec3 world;
        Vec3 camera;
        Vec3 normal;
        Vec2 uv;
    };

    struct ClipVertex {
        Vec3 world;
        Vec3 camera;
        Vec3 normal;
        Vec2 uv;
    };

    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_colourBuffer;
    std::vector<float> m_zBuffer;

    Vec3 materialAlbedo(const Mesh& mesh, const Material& material, const Vec2& uv, const RenderOptions& options) const;
    Vec3 shade(const Vec3& worldP, const Vec3& normal, const Vec3& albedo, float depth, const CameraSettings& camera, const RenderOptions& options) const;
    void drawTriangle(const std::array<ScreenVertex, 3>& tri, const Mesh& mesh, const Material& material, const CameraSettings& camera, const RenderOptions& options);

    static Vec3 makeFaceNormal(const Vec3& a, const Vec3& b, const Vec3& c);
    static ClipVertex interpolateClipVertex(const ClipVertex& a, const ClipVertex& b, float t);
    static std::vector<ClipVertex> clipAgainstZPlane(const std::vector<ClipVertex>& input, float zPlane, bool keepGreaterThan);
    static bool shouldSkipObject(const std::string& objectName, const std::vector<std::string>& skipObjects);
};
