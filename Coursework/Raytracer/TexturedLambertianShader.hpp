#pragma once
#include "Shader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/// <summary>
/// Lambertian reflectance shader that samples albedo values from an RGBA texture.
/// </summary>
class TexturedLambertianShader : public Shader
{
private:
    const std::vector<uint8_t>* albedoTexture_;
    const int texWidth_, texHeight_;
    bool shadowTest_;
public:
    TexturedLambertianShader(const std::vector<uint8_t>* albedoTexture, int texWidth, int texHeight, bool shadowTest=true)
        :shadowTest_(shadowTest), albedoTexture_(albedoTexture),
        texWidth_(texWidth), texHeight_(texHeight)
    {}

    virtual Eigen::Vector3f getColor(const HitInfo& hitInfo,
        const Renderable* scene,
        const std::vector<std::unique_ptr<Light>>& lights,
        const Eigen::Vector3f& ambientLight,
        int currBounceCount,
        const int maxBounces) const
    {
        Eigen::Vector3f albedo(0.8f, 0.8f, 0.8f);

        if (albedoTexture_ && texWidth_ > 0 && texHeight_ > 0 && !albedoTexture_->empty()) {
            Eigen::Vector2f tex = hitInfo.texCoords;

            // Wrap UVs, matching the rasteriser behaviour.
            float u = tex.x() - std::floor(tex.x());
            float v = tex.y() - std::floor(tex.y());

            int pixX = static_cast<int>(u * texWidth_);
            int pixY = static_cast<int>((1.0f - v) * texHeight_);

            pixX = std::max(0, std::min(pixX, texWidth_ - 1));
            pixY = std::max(0, std::min(pixY, texHeight_ - 1));

            int offset = (pixX + texWidth_ * pixY) * 4;
            albedo.x() = static_cast<float>((*albedoTexture_)[offset + 0]) / 255.f;
            albedo.y() = static_cast<float>((*albedoTexture_)[offset + 1]) / 255.f;
            albedo.z() = static_cast<float>((*albedoTexture_)[offset + 2]) / 255.f;
        }

        Eigen::Vector3f color = coefftWiseMul(albedo, ambientLight);

        for (auto& light : lights) {
            if (shadowTest_) {
                if (!light->visibilityCheck(hitInfo.location, scene))
                    continue;
            }
            Eigen::Vector3f lightVec = light->getVecToLight(hitInfo.location);
            float dotProd = std::max(lightVec.dot(hitInfo.normal), 0.f);
            color += dotProd * coefftWiseMul(light->getIntensity(hitInfo.location), albedo);
        }

        return color;
    }
};
