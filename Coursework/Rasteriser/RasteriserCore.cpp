#include "RasteriserCore.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <lodepng.h>

Rasteriser::Rasteriser(int width, int height)
    : m_width(width), m_height(height),
      m_colourBuffer(width * height * 4, 255),
      m_zBuffer(width * height, std::numeric_limits<float>::infinity()) {}

void Rasteriser::clear(const Color& colour) {
    std::fill(m_zBuffer.begin(), m_zBuffer.end(), std::numeric_limits<float>::infinity());
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            setPixel(m_colourBuffer, x, y, m_width, colour);
        }
    }
}

Vec3 Rasteriser::makeFaceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return normalize(cross3(b - a, c - a));
}

Rasteriser::ClipVertex Rasteriser::interpolateClipVertex(const ClipVertex& a, const ClipVertex& b, float t) {
    ClipVertex out;
    out.world = a.world + (b.world - a.world) * t;
    out.camera = a.camera + (b.camera - a.camera) * t;
    out.normal = normalize(a.normal + (b.normal - a.normal) * t);
    out.uv = a.uv + (b.uv - a.uv) * t;
    return out;
}

std::vector<Rasteriser::ClipVertex> Rasteriser::clipAgainstZPlane(
    const std::vector<ClipVertex>& input,
    float zPlane,
    bool keepGreaterThan
) {
    std::vector<ClipVertex> output;
    if (input.empty()) {
        return output;
    }

    auto inside = [zPlane, keepGreaterThan](const ClipVertex& v) {
        return keepGreaterThan ? (v.camera.z >= zPlane) : (v.camera.z <= zPlane);
    };

    ClipVertex previous = input.back();
    bool previousInside = inside(previous);

    for (const ClipVertex& current : input) {
        bool currentInside = inside(current);

        if (currentInside != previousInside) {
            float denominator = current.camera.z - previous.camera.z;
            if (std::fabs(denominator) > 1e-8f) {
                float t = (zPlane - previous.camera.z) / denominator;
                output.push_back(interpolateClipVertex(previous, current, t));
            }
        }

        if (currentInside) {
            output.push_back(current);
        }

        previous = current;
        previousInside = currentInside;
    }

    return output;
}

bool Rasteriser::shouldSkipObject(const std::string& objectName, const std::vector<std::string>& skipObjects) {
    return std::find(skipObjects.begin(), skipObjects.end(), objectName) != skipObjects.end();
}

Vec3 Rasteriser::materialAlbedo(const Mesh& mesh, const Material& material, const Vec2& uv, const RenderOptions& options) const {
    if (material.texture >= 0 && material.texture < static_cast<int>(mesh.textures.size())) {
        Color sampled = options.useBilinearTextureFiltering
            ? sampleTextureBilinear(mesh.textures[material.texture], uv)
            : sampleTextureNearest(mesh.textures[material.texture], uv);
        return srgbToLinear(sampled);
    }

    return material.kd;
}

Vec3 Rasteriser::shade(const Vec3& worldP, const Vec3& normal, const Vec3& albedo, float depth, const CameraSettings& camera, const RenderOptions& options) const {
    const Vec3 ambient{ 0.65f, 0.65f, 0.65f };
    const Vec3 lightColour{ 0.55f, 0.55f, 0.55f };

    const Vec3 incomingLightDir = normalize(Vec3{ -0.3f, -0.7f, -0.5f });
    const Vec3 toLight = -incomingLightDir;
    const Vec3 viewDir = normalize(camera.position - worldP);
    const Vec3 halfVec = normalize(toLight + viewDir);

    float diffuseTerm = std::max(dot(normal, toLight), 0.0f);
    float specularTerm = std::pow(std::max(dot(normal, halfVec), 0.0f), 64.0f);

    Vec3 colour = multiply(ambient, albedo);
    colour += multiply(lightColour * diffuseTerm, albedo);
    colour += lightColour * (0.04f * specularTerm);

    if (options.enableFog) {
        float t = clamp01((depth - options.fogStart) / std::max(options.fogEnd - options.fogStart, 0.001f));
        colour = colour * (1.0f - t) + options.fogColour * t;
    }

    return colour;
}

void Rasteriser::drawTriangle(
    const std::array<ScreenVertex, 3>& tri,
    const Mesh& mesh,
    const Material& material,
    const CameraSettings& camera,
    const RenderOptions& options
) {
    const Vec2 p0{ tri[0].screen.x, tri[0].screen.y };
    const Vec2 p1{ tri[1].screen.x, tri[1].screen.y };
    const Vec2 p2{ tri[2].screen.x, tri[2].screen.y };

    float area = cross2(p1 - p0, p2 - p0);
    if (std::fabs(area) < 1e-6f) {
        return;
    }

    const float guard = 100000.0f;
    float rawMinX = std::max(-guard, std::min({ p0.x, p1.x, p2.x }));
    float rawMaxX = std::min( guard, std::max({ p0.x, p1.x, p2.x }));
    float rawMinY = std::max(-guard, std::min({ p0.y, p1.y, p2.y }));
    float rawMaxY = std::min( guard, std::max({ p0.y, p1.y, p2.y }));

    int minX = std::max(0, static_cast<int>(std::floor(rawMinX)));
    int maxX = std::min(m_width - 1, static_cast<int>(std::ceil(rawMaxX)));
    int minY = std::max(0, static_cast<int>(std::floor(rawMinY)));
    int maxY = std::min(m_height - 1, static_cast<int>(std::ceil(rawMaxY)));

    if (minX > maxX || minY > maxY) {
        return;
    }

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec2 p{ x + 0.5f, y + 0.5f };

            float w0 = cross2(p1 - p, p2 - p) / area;
            float w1 = cross2(p2 - p, p0 - p) / area;
            float w2 = cross2(p0 - p, p1 - p) / area;

            if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) {
                continue;
            }

            float invZ0 = 1.0f / tri[0].camera.z;
            float invZ1 = 1.0f / tri[1].camera.z;
            float invZ2 = 1.0f / tri[2].camera.z;

            float recip = w0 * invZ0 + w1 * invZ1 + w2 * invZ2;
            if (recip <= 0.0f) {
                continue;
            }

            float depth = 1.0f / recip;
            int zIndex = x + y * m_width;

            if (depth >= m_zBuffer[zIndex]) {
                continue;
            }

            m_zBuffer[zIndex] = depth;

            Vec3 worldP =
                (tri[0].world * (w0 * invZ0) +
                 tri[1].world * (w1 * invZ1) +
                 tri[2].world * (w2 * invZ2)) * depth;

            Vec3 normal = normalize(
                (tri[0].normal * (w0 * invZ0) +
                 tri[1].normal * (w1 * invZ1) +
                 tri[2].normal * (w2 * invZ2)) * depth
            );

            Vec2 uv =
                (tri[0].uv * (w0 * invZ0) +
                 tri[1].uv * (w1 * invZ1) +
                 tri[2].uv * (w2 * invZ2)) * depth;

            Vec3 albedo = materialAlbedo(mesh, material, uv, options);
            Vec3 lit = shade(worldP, normal, albedo, depth, camera, options);
            setPixel(m_colourBuffer, x, y, m_width, linearToSrgb(lit));
        }
    }
}

void Rasteriser::renderMesh(
    const Mesh& mesh,
    const CameraSettings& camera,
    const ModelTransform& transform,
    const RenderOptions& options
) {
    size_t submitted = 0;
    size_t culled = 0;
    size_t skipped = 0;

    for (const Face& face : mesh.faces) {
        if (shouldSkipObject(face.objectName, options.skipObjects)) {
            ++skipped;
            continue;
        }

        const Material& material = mesh.materials[
            std::max(0, std::min(face.material, static_cast<int>(mesh.materials.size()) - 1))
        ];

        std::array<Vec3, 3> worldPositions;
        for (int i = 0; i < 3; ++i) {
            Vec3 p = mesh.positions.at(face.corners[i].v);
            p = p * transform.scale;
            p = rotateXYZ(p, transform.pitchDegrees, transform.yawDegrees, transform.rollDegrees);
            p = p + transform.translate;
            worldPositions[i] = p;
        }

        Vec3 faceNormal = makeFaceNormal(worldPositions[0], worldPositions[1], worldPositions[2]);

        if (options.enableBackFaceCulling) {
            Vec3 faceCentre = (worldPositions[0] + worldPositions[1] + worldPositions[2]) / 3.0f;
            Vec3 cameraToFace = normalize(faceCentre - camera.position);
            // This culls triangles
            if (dot(faceNormal, cameraToFace) >= 0.0f) {
                ++culled;
                continue;
            }
        }

        std::vector<ClipVertex> polygon;
        polygon.reserve(3);

        for (int i = 0; i < 3; ++i) {
            const VertexRef& ref = face.corners[i];

            Vec3 n = faceNormal;
            if (ref.vn >= 0 && ref.vn < static_cast<int>(mesh.normals.size())) {
                n = mesh.normals[ref.vn];
                n = rotateXYZ(n, transform.pitchDegrees, transform.yawDegrees, transform.rollDegrees);
                n = normalize(n);
            }

            Vec2 uv{ 0.0f, 0.0f };
            if (ref.vt >= 0 && ref.vt < static_cast<int>(mesh.texcoords.size())) {
                uv = mesh.texcoords[ref.vt];
            }

            ClipVertex cv;
            cv.world = worldPositions[i];
            cv.camera = worldToCamera(cv.world, camera);
            cv.normal = n;
            cv.uv = uv;
            polygon.push_back(cv);
        }

        polygon = clipAgainstZPlane(polygon, camera.nearPlane, true);
        polygon = clipAgainstZPlane(polygon, camera.farPlane, false);

        if (polygon.size() < 3) {
            continue;
        }

        std::vector<ScreenVertex> projected;
        projected.reserve(polygon.size());

        bool failedProjection = false;
        for (const ClipVertex& cv : polygon) {
            ScreenVertex sv;
            sv.world = cv.world;
            sv.camera = cv.camera;
            sv.normal = cv.normal;
            sv.uv = cv.uv;

            if (!projectToScreen(cv.camera, m_width, m_height, camera, sv.screen)) {
                failedProjection = true;
                break;
            }

            projected.push_back(sv);
        }

        if (failedProjection) {
            continue;
        }

        for (size_t i = 1; i + 1 < projected.size(); ++i) {
            std::array<ScreenVertex, 3> tri = {
                projected[0],
                projected[i],
                projected[i + 1]
            };

            drawTriangle(tri, mesh, material, camera, options);
            ++submitted;
        }
    }

    std::cout << "Rasteriser stats: submitted " << submitted
              << " triangles, culled " << culled
              << ", skipped " << skipped << "\n";
}

void Rasteriser::saveColourBuffer(const std::string& filename) const {
    unsigned error = lodepng::encode(filename, m_colourBuffer, m_width, m_height);
    if (error) {
        throw std::runtime_error(std::string("Failed to save ") + filename + ": " + lodepng_error_text(error));
    }
}

void Rasteriser::saveZBuffer(const std::string& filename) const {
    float minDepth = std::numeric_limits<float>::max();
    float maxDepth = 0.0f;

    for (float z : m_zBuffer) {
        if (std::isfinite(z)) {
            minDepth = std::min(minDepth, z);
            maxDepth = std::max(maxDepth, z);
        }
    }

    std::vector<uint8_t> image(m_width * m_height * 4, 255);
    float range = std::max(maxDepth - minDepth, 1e-5f);

    for (size_t i = 0; i < m_zBuffer.size(); ++i) {
        uint8_t v = std::isfinite(m_zBuffer[i])
            ? static_cast<uint8_t>(255.0f * (m_zBuffer[i] - minDepth) / range)
            : 255;

        image[i * 4 + 0] = v;
        image[i * 4 + 1] = v;
        image[i * 4 + 2] = v;
        image[i * 4 + 3] = 255;
    }

    unsigned error = lodepng::encode(filename, image, m_width, m_height);
    if (error) {
        std::cerr << "Warning: failed to save z-buffer image: " << lodepng_error_text(error) << "\n";
    }
}
