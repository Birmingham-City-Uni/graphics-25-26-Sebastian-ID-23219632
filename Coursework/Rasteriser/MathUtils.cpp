#include "MathUtils.hpp"

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float cross2(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
}

Vec3 multiply(const Vec3& a, const Vec3& b) {
    return { a.x * b.x, a.y * b.y, a.z * b.z };
}

float length(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return l > 0.0f ? v / l : Vec3{ 0.0f, 0.0f, 0.0f };
}

float clamp01(float v) {
    return std::min(std::max(v, 0.0f), 1.0f);
}

float radians(float degrees) {
    return degrees * 3.14159265358979323846f / 180.0f;
}

std::string lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

Vec3 rotateX(const Vec3& p, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    return { p.x, c * p.y - s * p.z, s * p.y + c * p.z };
}

Vec3 rotateY(const Vec3& p, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    return { c * p.x + s * p.z, p.y, -s * p.x + c * p.z };
}

Vec3 rotateZ(const Vec3& p, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    return { c * p.x - s * p.y, s * p.x + c * p.y, p.z };
}

Vec3 rotateXYZ(const Vec3& p, float pitchDeg, float yawDeg, float rollDeg) {
    Vec3 out = p;
    out = rotateX(out, radians(pitchDeg));
    out = rotateY(out, radians(yawDeg));
    out = rotateZ(out, radians(rollDeg));
    return out;
}

std::string directoryOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

std::string basenameOf(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return static_cast<bool>(f);
}

std::string resolveAssetPath(const std::string& baseDirectory, const std::string& rawPath) {
    std::string nameOnly = basenameOf(rawPath);
    std::string besideBase = baseDirectory + "/" + nameOnly;

    if (fileExists(besideBase)) {
        return besideBase;
    }

    if (fileExists(rawPath)) {
        return rawPath;
    }

    return besideBase;
}
