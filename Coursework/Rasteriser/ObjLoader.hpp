#pragma once

#include "MathUtils.hpp"
#include "Texture.hpp"
#include <array>
#include <string>
#include <vector>

struct VertexRef {
    int v = -1;
    int vt = -1;
    int vn = -1;
};

struct Face {
    std::array<VertexRef, 3> corners;
    int material = 0;
    std::string objectName;
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

class ObjLoader {
public:
    Mesh load(const std::string& filename);

private:
    int materialIndex(Mesh& mesh, const std::string& name);
    VertexRef parseVertexRef(const std::string& token, const Mesh& mesh);
    void loadMtl(const std::string& filename, Mesh& mesh);
};
