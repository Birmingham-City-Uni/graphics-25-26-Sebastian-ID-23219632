#pragma once

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& r) const { return { x + r.x, y + r.y }; }
    Vec2 operator-(const Vec2& r) const { return { x - r.x, y - r.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& r) const { return { x + r.x, y + r.y, z + r.z }; }
    Vec3 operator-(const Vec3& r) const { return { x - r.x, y - r.y, z - r.z }; }
    Vec3 operator-() const { return { -x, -y, -z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }
    Vec3& operator+=(const Vec3& r) { x += r.x; y += r.y; z += r.z; return *this; }
};

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

float dot(const Vec3& a, const Vec3& b);
Vec3 cross3(const Vec3& a, const Vec3& b);
float cross2(const Vec2& a, const Vec2& b);
Vec3 multiply(const Vec3& a, const Vec3& b);
float length(const Vec3& v);
Vec3 normalize(const Vec3& v);
float clamp01(float v);
float radians(float degrees);
std::string lower(std::string s);

Vec3 rotateX(const Vec3& p, float theta);
Vec3 rotateY(const Vec3& p, float theta);
Vec3 rotateZ(const Vec3& p, float theta);
Vec3 rotateXYZ(const Vec3& p, float pitchDeg, float yawDeg, float rollDeg);

std::string directoryOf(const std::string& path);
std::string basenameOf(std::string path);
bool fileExists(const std::string& path);
std::string resolveAssetPath(const std::string& baseDirectory, const std::string& rawPath);
