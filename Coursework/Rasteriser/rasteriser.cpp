#define _USE_MATH_DEFINES
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <lodepng.h>

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

static float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float cross2(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
}

static Vec3 multiply(const Vec3& a, const Vec3& b) {
    return { a.x * b.x, a.y * b.y, a.z * b.z };
}

static float length(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

static Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return l > 0.0f ? v / l : Vec3{ 0.0f, 0.0f, 0.0f };
}

static float clamp01(float v) {
    return std::min(std::max(v, 0.0f), 1.0f);
}

static float radians(float degrees) {
    return degrees * static_cast<float>(M_PI) / 180.0f;
}

static std::string lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static Vec3 rotateX(const Vec3& p, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    return { p.x, c * p.y - s * p.z, s * p.y + c * p.z };
}

static Vec3 rotateY(const Vec3& p, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    return { c * p.x + s * p.z, p.y, -s * p.x + c * p.z };
}

static Vec3 rotateZ(const Vec3& p, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    return { c * p.x - s * p.y, s * p.x + c * p.y, p.z };
}

static Vec3 rotateXYZ(const Vec3& p, float pitchDeg, float yawDeg, float rollDeg) {
    Vec3 out = p;
    out = rotateX(out, radians(pitchDeg));
    out = rotateY(out, radians(yawDeg));
    out = rotateZ(out, radians(rollDeg));
    return out;
}

static std::string directoryOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

static std::string basenameOf(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return static_cast<bool>(f);
}

static std::string resolveTexturePath(const std::string& mtlDir, const std::string& rawPath) {
    // Filename search
    std::string nameOnly = basenameOf(rawPath);
    std::string besideMtl = mtlDir + "/" + nameOnly;

    if (fileExists(besideMtl)) {
        return besideMtl;
    }

    if (fileExists(rawPath)) {
        return rawPath;
    }

    return besideMtl;
}

struct VertexRef {
    int v = -1;
    int vt = -1;
    int vn = -1;
};

struct Face {
    std::array<VertexRef, 3> corners;
    int material = 0;
};

struct CpuTexture {
    std::string filename;
    unsigned width = 0;
    unsigned height = 0;
    std::vector<uint8_t> rgba;

    bool valid() const {
        return width > 0 && height > 0 && !rgba.empty();
    }
};

struct Material {
    std::string name = "default";
    Vec3 kd{ 0.8f, 0.8f, 0.8f };
    std::string mapKd;
    int texture = -1;
};

struct Mesh {
    std::vector<Vec3> positions;
    std::vector<Vec2> texcoords;
    std::vector<Vec3> normals;
    std::vector<Face> faces;
    std::vector<Material> materials;
    std::vector<CpuTexture> textures;
};

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

struct CameraSettings {
    //   the camera looks along +Z
    //   X is left/right
    //   Y is up/down

    Vec3 position{ 0.0f, 2.0f, -8.0f };
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float rollDegrees = 0.0f;
    float fieldOfViewYDegrees = 60.0f;
    float nearPlane = 0.01f;
    float farPlane = 1000.0f;
};

static void setPixel(std::vector<uint8_t>& image, int x, int y, int width, const Color& c) {
    const int idx = 4 * (x + y * width);
    image[idx + 0] = c.r;
    image[idx + 1] = c.g;
    image[idx + 2] = c.b;
    image[idx + 3] = c.a;
}

static Color getPixel(const std::vector<uint8_t>& image, int x, int y, int width) {
    const int idx = 4 * (x + y * width);
    return { image[idx + 0], image[idx + 1], image[idx + 2], image[idx + 3] };
}

static Color sampleTexture(const CpuTexture& tex, const Vec2& uv) {
    if (!tex.valid()) {
        return { 200, 200, 200, 255 };
    }

    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    int x = std::min(static_cast<int>(u * tex.width), static_cast<int>(tex.width) - 1);
    int y = std::min(static_cast<int>((1.0f - v) * tex.height), static_cast<int>(tex.height) - 1);

    return getPixel(tex.rgba, x, y, static_cast<int>(tex.width));
}

static Vec3 srgbToLinear(const Color& c) {
    return {
        std::pow(c.r / 255.0f, 2.2f),
        std::pow(c.g / 255.0f, 2.2f),
        std::pow(c.b / 255.0f, 2.2f)
    };
}

static Color linearToSrgb(const Vec3& c) {
    return {
        static_cast<uint8_t>(clamp01(std::pow(std::max(c.x, 0.0f), 1.0f / 2.2f)) * 255.0f),
        static_cast<uint8_t>(clamp01(std::pow(std::max(c.y, 0.0f), 1.0f / 2.2f)) * 255.0f),
        static_cast<uint8_t>(clamp01(std::pow(std::max(c.z, 0.0f), 1.0f / 2.2f)) * 255.0f),
        255
    };
}

static int materialIndex(Mesh& mesh, const std::string& name) {
    for (size_t i = 0; i < mesh.materials.size(); ++i) {
        if (mesh.materials[i].name == name) {
            return static_cast<int>(i);
        }
    }

    Material material;
    material.name = name;
    mesh.materials.push_back(material);
    return static_cast<int>(mesh.materials.size() - 1);
}

static CpuTexture loadTexture(const std::string& filename) {
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

static void loadMtl(const std::string& filename, Mesh& mesh) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Warning: unable to open MTL: " << filename << "\n";
        return;
    }

    std::cout << "Loading MTL: " << filename << "\n";

    std::string mtlDir = directoryOf(filename);
    Material* current = nullptr;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "newmtl") {
            std::string name;
            std::getline(ss >> std::ws, name);
            int idx = materialIndex(mesh, name);
            current = &mesh.materials[idx];
            current->name = name;
        }
        else if (current && tag == "Kd") {
            ss >> current->kd.x >> current->kd.y >> current->kd.z;
        }
        else if (current && tag == "map_Kd") {
            std::string token;
            std::string lastToken;
            while (ss >> token) {
                lastToken = token;
            }

            if (!lastToken.empty()) {
                current->mapKd = resolveTexturePath(mtlDir, lastToken);
            }
        }
    }

    std::unordered_map<std::string, int> alreadyLoaded;

    for (Material& material : mesh.materials) {
        if (material.mapKd.empty()) {
            continue;
        }

        auto found = alreadyLoaded.find(material.mapKd);
        if (found != alreadyLoaded.end()) {
            material.texture = found->second;
            continue;
        }

        CpuTexture tex = loadTexture(material.mapKd);
        if (tex.valid()) {
            mesh.textures.push_back(std::move(tex));
            material.texture = static_cast<int>(mesh.textures.size() - 1);
            alreadyLoaded[material.mapKd] = material.texture;
        }
    }
}

static VertexRef parseVertexRef(const std::string& token, const Mesh& mesh) {
    VertexRef out;
    std::stringstream ss(token);
    std::string part;

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int index = std::stoi(part);
        out.v = index > 0 ? index - 1 : static_cast<int>(mesh.positions.size()) + index;
    }

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int index = std::stoi(part);
        out.vt = index > 0 ? index - 1 : static_cast<int>(mesh.texcoords.size()) + index;
    }

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int index = std::stoi(part);
        out.vn = index > 0 ? index - 1 : static_cast<int>(mesh.normals.size()) + index;
    }

    return out;
}

static Mesh loadObj(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open OBJ: " + filename);
    }

    Mesh mesh;
    materialIndex(mesh, "default");
    int currentMaterial = 0;
    std::string objDir = directoryOf(filename);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "mtllib") {
            std::string mtlName;
            std::getline(ss >> std::ws, mtlName);
            loadMtl(objDir + "/" + basenameOf(mtlName), mesh);
        }
        else if (tag == "usemtl") {
            std::string name;
            std::getline(ss >> std::ws, name);
            currentMaterial = materialIndex(mesh, name);
        }
        else if (tag == "v") {
            Vec3 p;
            ss >> p.x >> p.y >> p.z;
            mesh.positions.push_back(p);
        }
        else if (tag == "vt") {
            Vec2 t;
            ss >> t.x >> t.y;
            mesh.texcoords.push_back(t);
        }
        else if (tag == "vn") {
            Vec3 n;
            ss >> n.x >> n.y >> n.z;
            mesh.normals.push_back(normalize(n));
        }
        else if (tag == "f") {
            std::vector<VertexRef> refs;
            std::string refToken;

            while (ss >> refToken) {
                refs.push_back(parseVertexRef(refToken, mesh));
            }

            // Fan triangulate
            for (size_t i = 1; i + 1 < refs.size(); ++i) {
                mesh.faces.push_back({ {refs[0], refs[i], refs[i + 1]}, currentMaterial });
            }
        }
    }

    std::cout << "Loaded OBJ: " << filename << "\n";
    std::cout << "  vertices:  " << mesh.positions.size() << "\n";
    std::cout << "  uvs:       " << mesh.texcoords.size() << "\n";
    std::cout << "  normals:   " << mesh.normals.size() << "\n";
    std::cout << "  triangles: " << mesh.faces.size() << "\n";
    std::cout << "  materials: " << mesh.materials.size() << "\n";
    std::cout << "  textures:  " << mesh.textures.size() << "\n";

    return mesh;
}

static Vec3 worldToCamera(const Vec3& world, const CameraSettings& camera) {
    // Inverse camera transform.
    Vec3 p = world - camera.position;
    p = rotateZ(p, -radians(camera.rollDegrees));
    p = rotateX(p, -radians(camera.pitchDegrees));
    p = rotateY(p, -radians(camera.yawDegrees));
    return p;
}

static bool projectToScreen(const Vec3& camera, int width, int height, const CameraSettings& cam, Vec3& screenOut) {
    if (camera.z <= cam.nearPlane || camera.z >= cam.farPlane) {
        return false;
    }

    const float fovY = radians(cam.fieldOfViewYDegrees);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float yScale = 1.0f / std::tan(fovY * 0.5f);
    const float xScale = yScale / aspect;

    const float ndcX = (camera.x * xScale) / camera.z;
    const float ndcY = (camera.y * yScale) / camera.z;

    screenOut = {
        (ndcX + 1.0f) * 0.5f * static_cast<float>(width),
        (1.0f - ndcY) * 0.5f * static_cast<float>(height),
        camera.z
    };

    return true;
}

static Vec3 makeFaceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return normalize(cross3(b - a, c - a));
}

static ClipVertex interpolateClipVertex(const ClipVertex& a, const ClipVertex& b, float t) {
    ClipVertex out;
    out.world = a.world + (b.world - a.world) * t;
    out.camera = a.camera + (b.camera - a.camera) * t;
    out.normal = normalize(a.normal + (b.normal - a.normal) * t);
    out.uv = a.uv + (b.uv - a.uv) * t;
    return out;
}

static std::vector<ClipVertex> clipAgainstZPlane(
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

static Vec3 materialAlbedo(const Mesh& mesh, const Material& material, const Vec2& uv) {
    if (material.texture >= 0 && material.texture < static_cast<int>(mesh.textures.size())) {
        return srgbToLinear(sampleTexture(mesh.textures[material.texture], uv));
    }

    return material.kd;
}

static Vec3 shade(const Vec3& worldP, const Vec3& normal, const Vec3& albedo, const CameraSettings& camera) {
    // Simple lighting since textures are baked
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

    return colour;
}

static void drawTriangle(
    std::vector<uint8_t>& image,
    std::vector<float>& zBuffer,
    int width,
    int height,
    const std::array<ScreenVertex, 3>& tri,
    const Mesh& mesh,
    const Material& material,
    const CameraSettings& camera
) {
    const Vec2 p0{ tri[0].screen.x, tri[0].screen.y };
    const Vec2 p1{ tri[1].screen.x, tri[1].screen.y };
    const Vec2 p2{ tri[2].screen.x, tri[2].screen.y };

    float area = cross2(p1 - p0, p2 - p0);
    if (std::fabs(area) < 1e-6f) {
        return;
    }

    // Int conversion safeguard
    const float guard = 100000.0f;
    float rawMinX = std::max(-guard, std::min({ p0.x, p1.x, p2.x }));
    float rawMaxX = std::min(guard, std::max({ p0.x, p1.x, p2.x }));
    float rawMinY = std::max(-guard, std::min({ p0.y, p1.y, p2.y }));
    float rawMaxY = std::min(guard, std::max({ p0.y, p1.y, p2.y }));

    int minX = std::max(0, static_cast<int>(std::floor(rawMinX)));
    int maxX = std::min(width - 1, static_cast<int>(std::ceil(rawMaxX)));
    int minY = std::max(0, static_cast<int>(std::floor(rawMinY)));
    int maxY = std::min(height - 1, static_cast<int>(std::ceil(rawMaxY)));

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
            int zIndex = x + y * width;

            if (depth >= zBuffer[zIndex]) {
                continue;
            }

            zBuffer[zIndex] = depth;

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

            Vec3 albedo = materialAlbedo(mesh, material, uv);
            Vec3 lit = shade(worldP, normal, albedo, camera);
            setPixel(image, x, y, width, linearToSrgb(lit));
        }
    }
}

static void drawMesh(
    std::vector<uint8_t>& image,
    std::vector<float>& zBuffer,
    int width,
    int height,
    const Mesh& mesh,
    const CameraSettings& camera,
    float modelScale,
    Vec3 modelTranslate,
    float modelYawDegrees,
    float modelPitchDegrees,
    float modelRollDegrees
) {
    for (const Face& face : mesh.faces) {
        const Material& material = mesh.materials[
            std::max(0, std::min(face.material, static_cast<int>(mesh.materials.size()) - 1))
        ];

        std::array<Vec3, 3> worldPositions;

        for (int i = 0; i < 3; ++i) {
            Vec3 p = mesh.positions.at(face.corners[i].v);
            p = p * modelScale;
            p = rotateXYZ(p, modelPitchDegrees, modelYawDegrees, modelRollDegrees);
            p = p + modelTranslate;
            worldPositions[i] = p;
        }

        Vec3 fallbackNormal = makeFaceNormal(worldPositions[0], worldPositions[1], worldPositions[2]);

        std::vector<ClipVertex> polygon;
        polygon.reserve(3);

        for (int i = 0; i < 3; ++i) {
            const VertexRef& ref = face.corners[i];

            Vec3 n = fallbackNormal;
            if (ref.vn >= 0 && ref.vn < static_cast<int>(mesh.normals.size())) {
                n = mesh.normals[ref.vn];
                n = rotateXYZ(n, modelPitchDegrees, modelYawDegrees, modelRollDegrees);
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

        // Clip only against near/far planes. Off-screen bounding-box clamping.
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

            if (!projectToScreen(cv.camera, width, height, camera, sv.screen)) {
                failedProjection = true;
                break;
            }

            projected.push_back(sv);
        }

        if (failedProjection) {
            continue;
        }

        // Re-triangulate clipped polygon as a fan.
        for (size_t i = 1; i + 1 < projected.size(); ++i) {
            std::array<ScreenVertex, 3> tri = {
                projected[0],
                projected[i],
                projected[i + 1]
            };

            drawTriangle(image, zBuffer, width, height, tri, mesh, material, camera);
        }
    }
}

static void saveZBufferImage(const std::string& filename, const std::vector<float>& zBuffer, int width, int height) {
    float minDepth = std::numeric_limits<float>::max();
    float maxDepth = 0.0f;

    for (float z : zBuffer) {
        if (std::isfinite(z)) {
            minDepth = std::min(minDepth, z);
            maxDepth = std::max(maxDepth, z);
        }
    }

    std::vector<uint8_t> image(width * height * 4, 255);
    float range = std::max(maxDepth - minDepth, 1e-5f);

    for (size_t i = 0; i < zBuffer.size(); ++i) {
        uint8_t v = std::isfinite(zBuffer[i])
            ? static_cast<uint8_t>(255.0f * (zBuffer[i] - minDepth) / range)
            : 255;

        image[i * 4 + 0] = v;
        image[i * 4 + 1] = v;
        image[i * 4 + 2] = v;
        image[i * 4 + 3] = 255;
    }

    unsigned error = lodepng::encode(filename, image, width, height);
    if (error) {
        std::cerr << "Warning: failed to save z-buffer image: " << lodepng_error_text(error) << "\n";
    }
}

int main() {
    try {


        const int width = 1200;
        const int height = 900;

        // OBJ and MTL calls for model
        std::string objPath = "../models/ARC_RAIDERS_BAKED.obj";

        // Camera controls.
        CameraSettings camera;
        camera.position = { 0.0f, 1.25f, 17.0f };
        camera.pitchDegrees = 0.0;
        camera.yawDegrees = 180.0f;
        camera.rollDegrees = 0.0f;

        camera.fieldOfViewYDegrees = 45.0f;
        camera.nearPlane = 0.01f;
        camera.farPlane = 1000.0f;

        // Model transform
        float modelScale = 1.0f;
        Vec3 modelTranslate{ 0.0f, 0.0f, 0.0f };
        float modelYawDegrees = 0.0f;
        float modelPitchDegrees = 0.0f;
        float modelRollDegrees = 0.0f;


        std::vector<uint8_t> image(width * height * 4, 255);
        std::vector<float> zBuffer(width * height, std::numeric_limits<float>::infinity());

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                setPixel(image, x, y, width, Color{ 8, 10, 14, 255 });
            }
        }

        Mesh mesh = loadObj(objPath);

        drawMesh(
            image,
            zBuffer,
            width,
            height,
            mesh,
            camera,
            modelScale,
            modelTranslate,
            modelYawDegrees,
            modelPitchDegrees,
            modelRollDegrees
        );

        unsigned error = lodepng::encode("rasterised.png", image, width, height);
        if (error) {
            throw std::runtime_error(std::string("Failed to save rasterised.png: ") + lodepng_error_text(error));
        }

        saveZBufferImage("rasterised_zBuffer.png", zBuffer, width, height);

        std::cout << "Wrote rasterised.png and rasterised_zBuffer.png\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}