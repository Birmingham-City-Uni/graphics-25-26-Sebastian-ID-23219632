#include "Model.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

static std::string directoryOf(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

static std::string basenameOf(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static bool fileExists(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    return static_cast<bool>(f);
}

static std::string resolveTexturePath(const std::string& mtlDir, const std::string& rawPath)
{
    // Blender often writes absolute paths or paths beginning with //. For portability,
    // first try to find the image beside the MTL using only the filename.
    std::string nameOnly = basenameOf(rawPath);
    std::string besideMtl = mtlDir + "/" + nameOnly;

    if (fileExists(besideMtl)) return besideMtl;
    if (fileExists(rawPath)) return rawPath;
    return besideMtl;
}

static int findOrCreateMaterial(std::vector<MaterialInfo>& materials, const std::string& name)
{
    for (int i = 0; i < static_cast<int>(materials.size()); ++i) {
        if (materials[i].name == name) return i;
    }

    MaterialInfo material;
    material.name = name;
    materials.push_back(material);
    return static_cast<int>(materials.size() - 1);
}

static VertexIndices parseVertexRef(const std::string& token, const Model& model)
{
    VertexIndices out;
    std::stringstream ss(token);
    std::string part;

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int idx = std::stoi(part);
        out.vert = idx > 0 ? idx - 1 : model.nverts() + idx;
    }

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int idx = std::stoi(part);
        out.tex = idx > 0 ? idx - 1 : static_cast<int>(model.hasTexCoords()) + idx;
        // Correct negative texcoord references using the actual vt count cannot be done
        // through the public API without adding a count method, so Blender positive
        // indices are expected here. The fallback below keeps missing refs safe.
    }

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int idx = std::stoi(part);
        out.norm = idx > 0 ? idx - 1 : static_cast<int>(model.hasNormals()) + idx;
    }

    return out;
}

static VertexIndices parseVertexRefWithCounts(const std::string& token, int nVerts, int nTex, int nNorm)
{
    VertexIndices out;
    std::stringstream ss(token);
    std::string part;

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int idx = std::stoi(part);
        out.vert = idx > 0 ? idx - 1 : nVerts + idx;
    }

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int idx = std::stoi(part);
        out.tex = idx > 0 ? idx - 1 : nTex + idx;
    }

    std::getline(ss, part, '/');
    if (!part.empty()) {
        int idx = std::stoi(part);
        out.norm = idx > 0 ? idx - 1 : nNorm + idx;
    }

    return out;
}

static void loadMtlFile(const std::string& filename, std::vector<MaterialInfo>& materials)
{
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Warning: could not open MTL file: " << filename << std::endl;
        return;
    }

    std::cerr << "Loading MTL: " << filename << std::endl;

    std::string mtlDir = directoryOf(filename);
    MaterialInfo* current = nullptr;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "newmtl") {
            std::string name;
            std::getline(ss >> std::ws, name);
            int idx = findOrCreateMaterial(materials, name);
            current = &materials[idx];
            current->name = name;
        }
        else if (current && tag == "Kd") {
            ss >> current->kd.x() >> current->kd.y() >> current->kd.z();
        }
        else if (current && tag == "map_Kd") {
            // MTL supports options before the filename. Blender's simple export usually
            // places the filename last, so keep the last token.
            std::string token, lastToken;
            while (ss >> token) lastToken = token;

            if (!lastToken.empty()) {
                current->mapKd = resolveTexturePath(mtlDir, lastToken);
            }
        }
    }
}

Model::Model(const char* filename)
    : verts_(), vns_(), vts_(), faces_(), faceMaterials_(), faceObjects_(), materials_()
{
    std::ifstream in(filename, std::ifstream::in);
    if (in.fail()) throw std::runtime_error("Couldn't open input model file!");

    std::string objDir = directoryOf(filename);
    int currentMaterial = findOrCreateMaterial(materials_, "default");
    std::string currentObject = "default";

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "mtllib") {
            std::string mtlName;
            std::getline(iss >> std::ws, mtlName);
            loadMtlFile(objDir + "/" + basenameOf(mtlName), materials_);
        }
        else if (tag == "usemtl") {
            std::string name;
            std::getline(iss >> std::ws, name);
            currentMaterial = findOrCreateMaterial(materials_, name);
        }
        else if (tag == "o" || tag == "g") {
            std::string name;
            std::getline(iss >> std::ws, name);
            if (!name.empty()) currentObject = name;
        }
        else if (tag == "v") {
            Eigen::Vector3f v;
            iss >> v.x() >> v.y() >> v.z();
            verts_.push_back(v);
        }
        else if (tag == "vt") {
            Eigen::Vector2f vt;
            iss >> vt.x() >> vt.y();
            vts_.push_back(vt);
        }
        else if (tag == "vn") {
            Eigen::Vector3f vn;
            iss >> vn.x() >> vn.y() >> vn.z();
            vns_.push_back(vn.normalized());
        }
        else if (tag == "f") {
            std::vector<VertexIndices> polygon;
            std::string refToken;

            while (iss >> refToken) {
                polygon.push_back(parseVertexRefWithCounts(
                    refToken,
                    static_cast<int>(verts_.size()),
                    static_cast<int>(vts_.size()),
                    static_cast<int>(vns_.size())));
            }

            // Fan triangulate quads/ngons from Blender.
            for (size_t i = 1; i + 1 < polygon.size(); ++i) {
                std::vector<VertexIndices> tri;
                tri.push_back(polygon[0]);
                tri.push_back(polygon[i]);
                tri.push_back(polygon[i + 1]);
                faces_.push_back(tri);
                faceMaterials_.push_back(currentMaterial);
                faceObjects_.push_back(currentObject);
            }
        }
    }

    std::cerr << "Loaded OBJ: " << filename << std::endl;
    std::cerr << "  v# " << verts_.size()
              << " vt# " << vts_.size()
              << " vn# " << vns_.size()
              << " tri# " << faces_.size()
              << " materials# " << materials_.size()
              << std::endl;
}

Model::~Model() {}

int Model::nverts() const { return static_cast<int>(verts_.size()); }
int Model::nfaces() const { return static_cast<int>(faces_.size()); }
int Model::nmaterials() const { return static_cast<int>(materials_.size()); }

bool Model::hasNormals() const { return !vns_.empty(); }
bool Model::hasTexCoords() const { return !vts_.empty(); }

std::vector<VertexIndices> Model::face(int idx) const { return faces_[idx]; }
Eigen::Vector3f Model::vert(int i) const { return verts_[i]; }
Eigen::Vector2f Model::texCoord(int i) const { return vts_[i]; }
Eigen::Vector3f Model::normal(int i) const { return vns_[i]; }

int Model::faceMaterial(int faceIdx) const { return faceMaterials_[faceIdx]; }
std::string Model::faceObject(int faceIdx) const { return faceObjects_[faceIdx]; }
const MaterialInfo& Model::material(int materialIdx) const { return materials_[materialIdx]; }

std::vector<std::vector<VertexIndices>> Model::facesForMaterial(int materialIdx) const
{
    std::vector<std::vector<VertexIndices>> out;
    for (int f = 0; f < nfaces(); ++f) {
        if (faceMaterials_[f] == materialIdx) {
            out.push_back(faces_[f]);
        }
    }
    return out;
}
