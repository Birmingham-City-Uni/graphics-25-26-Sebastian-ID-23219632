#include <Eigen/Dense>
#include <lodepng.h>
#include <json/json.hpp>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "BVHNode.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "PointLight.hpp"
#include "DirectionalLight.hpp"
#include "LambertianShader.hpp"
#include "TexturedLambertianShader.hpp"
#include "Model.hpp"

nlohmann::json loadConfig(const std::string& filename)
{
    std::ifstream configStream(filename);
    if (!configStream) {
        throw std::runtime_error("Could not open config file: " + filename);
    }
    return nlohmann::json::parse(configStream);
}

Eigen::Vector3f loadVec3FromConfig(const nlohmann::json& config)
{
    return Eigen::Vector3f(config[0], config[1], config[2]);
}

static std::string lower(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool endsWith(const std::string& text, const std::string& suffix)
{
    if (suffix.size() > text.size()) return false;
    return lower(text.substr(text.size() - suffix.size())) == lower(suffix);
}

struct LoadedTexture
{
    std::vector<uint8_t> pixels;
    unsigned width = 0;
    unsigned height = 0;
};

static bool loadPngTexture(const std::string& filename, LoadedTexture& texture)
{
    if (!endsWith(filename, ".png")) {
        std::cerr << "Warning: only PNG map_Kd textures are supported by this project. Skipping: "
                  << filename << std::endl;
        return false;
    }

    unsigned error = lodepng::decode(texture.pixels, texture.width, texture.height, filename);
    if (error) {
        std::cerr << "Warning: failed to load texture '" << filename << "': "
                  << lodepng_error_text(error) << std::endl;
        texture.pixels.clear();
        texture.width = 0;
        texture.height = 0;
        return false;
    }

    std::cerr << "Loaded texture: " << filename << " (" << texture.width << "x" << texture.height << ")" << std::endl;
    return true;
}

static float getFloatOrDefault(const nlohmann::json& config, const std::string& key, float fallback)
{
    auto it = config.find(key);
    if (it == config.end()) return fallback;
    return *it;
}

static int getIntOrDefault(const nlohmann::json& config, const std::string& key, int fallback)
{
    auto it = config.find(key);
    if (it == config.end()) return fallback;
    return *it;
}

static bool getBoolOrDefault(const nlohmann::json& config, const std::string& key, bool fallback)
{
    auto it = config.find(key);
    if (it == config.end()) return fallback;
    return *it;
}

static std::string getStringOrDefault(const nlohmann::json& config, const std::string& key, const std::string& fallback)
{
    auto it = config.find(key);
    if (it == config.end()) return fallback;
    return *it;
}

int main(int argc, char* argv[])
{
    try {
        auto config = loadConfig("../config/config.json");

        const int pixHeight = getIntOrDefault(config, "pixHeight", 720);
        const int pixWidth = getIntOrDefault(config, "pixWidth", 1280);
        const int nChannels = 4;

        Camera cam(
            loadVec3FromConfig(config["cameraPos"]),
            loadVec3FromConfig(config["cameraForward"]),
            loadVec3FromConfig(config["cameraUp"]),
            pixWidth, pixHeight,
            getFloatOrDefault(config, "cameraFov", 0.785f));

        std::vector<uint8_t> outImage(pixHeight * pixWidth * nChannels);

        Eigen::Vector4i clearColor(0, 0, 0, 255);
        if (config.find("clearColor") != config.end()) {
            clearColor = Eigen::Vector4i(config["clearColor"][0], config["clearColor"][1], config["clearColor"][2], config["clearColor"][3]);
        }

        const std::string modelPath = getStringOrDefault(config, "modelPath", "../models/ARC_RAIDERS_BAKED.obj");
        const int bvhDepth = getIntOrDefault(config, "bvhDepth", 18);
        const bool backfaceCulling = getBoolOrDefault(config, "backfaceCulling", false);
        const bool shadowTest = getBoolOrDefault(config, "shadowTest", true);

        // Keep ownership stable for shaders/textures used by the scene.
        std::vector<std::unique_ptr<LoadedTexture>> ownedTextures;
        std::vector<std::unique_ptr<Shader>> ownedShaders;

        Model model(modelPath.c_str());

        Scene scene;
        Eigen::Matrix4f modelToWorld = Eigen::Matrix4f::Identity();

        // Build one BVH mesh per OBJ material. That lets the MTL decide which texture/shader
        // each group of faces uses, while keeping the project's original BVH dependency/layout.
        for (int m = 0; m < model.nmaterials(); ++m) {
            std::vector<std::vector<VertexIndices>> faces = model.facesForMaterial(m);
            if (faces.empty()) continue;

            const MaterialInfo& mat = model.material(m);
            const Shader* shaderPtr = nullptr;

            if (!mat.mapKd.empty()) {
                auto texture = std::make_unique<LoadedTexture>();
                if (loadPngTexture(mat.mapKd, *texture)) {
                    ownedTextures.push_back(std::move(texture));
                    LoadedTexture* tex = ownedTextures.back().get();
                    ownedShaders.push_back(std::make_unique<TexturedLambertianShader>(
                        &tex->pixels,
                        static_cast<int>(tex->width),
                        static_cast<int>(tex->height),
                        shadowTest));
                    shaderPtr = ownedShaders.back().get();
                }
            }

            if (!shaderPtr) {
                ownedShaders.push_back(std::make_unique<LambertianShader>(mat.kd, shadowTest));
                shaderPtr = ownedShaders.back().get();
            }

            std::cerr << "Adding material '" << mat.name << "' with " << faces.size() << " triangles";
            if (!mat.mapKd.empty()) std::cerr << " map_Kd=" << mat.mapKd;
            std::cerr << std::endl;

            scene.renderables.push_back(std::make_shared<BVHNode>(
                model,
                shaderPtr,
                bvhDepth,
                modelToWorld,
                &faces,
                backfaceCulling));
        }

        // Lighting. Baked textures generally look better with stronger ambient and gentle direct light.
        Eigen::Vector3f ambientLight = config.find("ambientLight") != config.end()
            ? loadVec3FromConfig(config["ambientLight"])
            : Eigen::Vector3f(0.45f, 0.45f, 0.45f);

        std::vector<std::unique_ptr<Light>> lightSources;
        if (config.find("directionalLightDir") != config.end() && config.find("directionalLightIntensity") != config.end()) {
            lightSources.push_back(std::make_unique<DirectionalLight>(
                loadVec3FromConfig(config["directionalLightDir"]),
                loadVec3FromConfig(config["directionalLightIntensity"])));
        }
        else {
            lightSources.push_back(std::make_unique<DirectionalLight>(
                Eigen::Vector3f(-0.3f, -0.7f, 0.4f),
                Eigen::Vector3f(0.55f, 0.55f, 0.55f)));
        }

        // Render.
        std::vector<unsigned int> scanlines(pixHeight);
        for (int i = 0; i < pixHeight; ++i) scanlines[i] = i;

        if (getBoolOrDefault(config, "shuffleScanlines", true)) {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(scanlines.begin(), scanlines.end(), g);
        }

        auto startTime = std::chrono::steady_clock::now();

        #pragma omp parallel for
        for (int y = 0; y < pixHeight; ++y) {
            for (int x = 0; x < pixWidth; ++x) {
                Ray ray = cam.getRay(x, scanlines[y]);
                HitInfo hitInfo;

                int line = (pixHeight - scanlines[y]) - 1;
                int offset = (x + line * pixWidth) * nChannels;

                if (scene.intersect(ray, 1e-5f, 1e7f, hitInfo, VISIBLE_BITMASK)) {
                    Eigen::Vector3f color = hitInfo.shader->getColor(
                        hitInfo,
                        &scene,
                        lightSources,
                        ambientLight,
                        0,
                        getIntOrDefault(config, "maxBounces", 1));

                    color.x() = std::min(std::max(color.x(), 0.f), 1.f);
                    color.y() = std::min(std::max(color.y(), 0.f), 1.f);
                    color.z() = std::min(std::max(color.z(), 0.f), 1.f);

                    outImage[offset + 0] = static_cast<uint8_t>(color.x() * 255.f);
                    outImage[offset + 1] = static_cast<uint8_t>(color.y() * 255.f);
                    outImage[offset + 2] = static_cast<uint8_t>(color.z() * 255.f);
                    outImage[offset + 3] = 255;
                }
                else {
                    outImage[offset + 0] = static_cast<uint8_t>(clearColor.x());
                    outImage[offset + 1] = static_cast<uint8_t>(clearColor.y());
                    outImage[offset + 2] = static_cast<uint8_t>(clearColor.z());
                    outImage[offset + 3] = static_cast<uint8_t>(clearColor.w());
                }
            }

            #ifdef _OPENMP
            if (omp_get_thread_num() == omp_get_num_threads() - 1) {
                std::clog << "\rScanlines remaining: " << (pixHeight - y) << ' ' << std::flush;
            }
            #endif
        }

        auto renderTime = std::chrono::steady_clock::now() - startTime;
        std::cout << "Render duration "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(renderTime).count() * 1e-3f
                  << " seconds." << std::endl;

        int errorCode = lodepng::encode(getStringOrDefault(config, "outputFilename", "output.png"), outImage, pixWidth, pixHeight);
        if (errorCode) {
            std::cout << "lodepng error encoding image: " << lodepng_error_text(errorCode) << std::endl;
            return errorCode;
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
