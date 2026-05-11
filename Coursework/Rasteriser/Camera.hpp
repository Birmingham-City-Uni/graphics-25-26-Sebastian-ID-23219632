#pragma once

#include "MathUtils.hpp"

struct CameraSettings {
    // Camera convention: camera looks along +Z, X is left/right, Y is up/down.
    Vec3 position{ 0.0f, 2.0f, -8.0f };
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float rollDegrees = 0.0f;
    float fieldOfViewYDegrees = 60.0f;
    float nearPlane = 0.01f;
    float farPlane = 1000.0f;
};

Vec3 worldToCamera(const Vec3& world, const CameraSettings& camera);
bool projectToScreen(const Vec3& cameraSpace, int width, int height, const CameraSettings& camera, Vec3& screenOut);
