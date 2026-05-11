#pragma once

#include <Eigen/Dense>
#include <string>
#include <vector>

struct VertexIndices
{
    int vert = -1;
    int tex = -1;
    int norm = -1;
};

struct MaterialInfo
{
    std::string name = "default";
    Eigen::Vector3f kd = Eigen::Vector3f(0.8f, 0.8f, 0.8f);
    std::string mapKd;
};

/// <summary>
/// A Model stores OBJ mesh data and can also read a companion MTL file.
/// It supports Blender-style OBJ files containing:
/// v, vt, vn, f, o, g, mtllib and usemtl.
/// Faces with more than 3 vertices are fan-triangulated while loading.
/// </summary>
class Model {
private:
    std::vector<Eigen::Vector3f> verts_, vns_;
    std::vector<Eigen::Vector2f> vts_;
    std::vector<std::vector<VertexIndices>> faces_;
    std::vector<int> faceMaterials_;
    std::vector<std::string> faceObjects_;
    std::vector<MaterialInfo> materials_;

public:
    Model(const char* filename);
    ~Model();

    int nverts() const;
    int nfaces() const;
    int nmaterials() const;

    Eigen::Vector3f vert(int i) const;
    Eigen::Vector2f texCoord(int i) const;
    Eigen::Vector3f normal(int i) const;
    std::vector<VertexIndices> face(int idx) const;

    bool hasNormals() const;
    bool hasTexCoords() const;

    int faceMaterial(int faceIdx) const;
    std::string faceObject(int faceIdx) const;
    const MaterialInfo& material(int materialIdx) const;

    std::vector<std::vector<VertexIndices>> facesForMaterial(int materialIdx) const;
};
