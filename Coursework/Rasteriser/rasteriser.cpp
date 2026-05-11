#include "ObjLoader.hpp"
#include "RasteriserCore.hpp"
#include <exception>
#include <iostream>

int main() {
    try {
        const int width = 1200;
        const int height = 900;

        // OBJ, MTL, and PNG files in the models folder.
        std::string objPath = "../models/ARC_RAIDERS_BAKED.obj";

        // Camera controls. camera looks along +Z.
        CameraSettings camera;
        camera.position = { 0.0f, 1.25f, 17.0f };
        camera.pitchDegrees = 0.0f;
        camera.yawDegrees = 180.0f;
        camera.rollDegrees = 0.0f;
        camera.fieldOfViewYDegrees = 45.0f;
        camera.nearPlane = 0.01f;
        camera.farPlane = 1000.0f;

        // Model transform. preserve the Blender coordinates.
        ModelTransform transform;
        transform.scale = 1.0f;
        transform.translate = { 0.0f, 0.0f, 0.0f };
        transform.yawDegrees = 0.0f;
        transform.pitchDegrees = 0.0f;
        transform.rollDegrees = 0.0f;

        RenderOptions options;
        options.enableBackFaceCulling = true;
        options.useBilinearTextureFiltering = true;

        // Set this true for an optional atmospheric-depth pass.
        options.enableFog = true;
        options.fogColour = { 0.72f, 0.62f, 0.54f };
        options.fogStart = 25.0f;
        options.fogEnd = 120.0f;

        // Helper/volume objects can be skipped here.
        options.skipObjects = {
            "volume",
            "volume.001",
            "dust_emitter"
        };

        ObjLoader loader;
        Mesh mesh = loader.load(objPath);

        Rasteriser rasteriser(width, height);
        rasteriser.clear(Color{ 8, 10, 14, 255 });
        rasteriser.renderMesh(mesh, camera, transform, options);
        rasteriser.saveColourBuffer("rasterised.png");
        rasteriser.saveZBuffer("rasterised_zBuffer.png");

        std::cout << "Wrote rasterised.png and rasterised_zBuffer.png\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
