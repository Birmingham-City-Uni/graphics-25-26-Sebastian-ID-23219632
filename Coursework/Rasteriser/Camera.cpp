#include "Camera.hpp"

Vec3 worldToCamera(const Vec3& world, const CameraSettings& camera) {
    Vec3 p = world - camera.position;
    p = rotateZ(p, -radians(camera.rollDegrees));
    p = rotateX(p, -radians(camera.pitchDegrees));
    p = rotateY(p, -radians(camera.yawDegrees));
    return p;
}

bool projectToScreen(const Vec3& cameraSpace, int width, int height, const CameraSettings& camera, Vec3& screenOut) {
    if (cameraSpace.z <= camera.nearPlane || cameraSpace.z >= camera.farPlane) {
        return false;
    }

    const float fovY = radians(camera.fieldOfViewYDegrees);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float yScale = 1.0f / std::tan(fovY * 0.5f);
    const float xScale = yScale / aspect;

    const float ndcX = (cameraSpace.x * xScale) / cameraSpace.z;
    const float ndcY = (cameraSpace.y * yScale) / cameraSpace.z;

    // The rasteriser clamps the triangle bounds.
    screenOut = {
        (ndcX + 1.0f) * 0.5f * static_cast<float>(width),
        (1.0f - ndcY) * 0.5f * static_cast<float>(height),
        cameraSpace.z
    };

    return true;
}
