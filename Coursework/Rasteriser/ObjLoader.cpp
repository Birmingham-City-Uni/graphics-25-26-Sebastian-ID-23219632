#include "ObjLoader.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

int ObjLoader::materialIndex(Mesh& mesh, const std::string& name) {
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

void ObjLoader::loadMtl(const std::string& filename, Mesh& mesh) {
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
                current->mapKd = resolveAssetPath(mtlDir, lastToken);
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

VertexRef ObjLoader::parseVertexRef(const std::string& token, const Mesh& mesh) {
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

Mesh ObjLoader::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Unable to open OBJ: " + filename);
    }

    Mesh mesh;
    materialIndex(mesh, "default");
    int currentMaterial = 0;
    std::string currentObject = "default";
    std::string objDir = directoryOf(filename);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "o" || tag == "g") {
            std::getline(ss >> std::ws, currentObject);
            if (currentObject.empty()) {
                currentObject = "default";
            }
        }
        else if (tag == "mtllib") {
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

            // Fan triangulation supports Blender quads and ngons.
            for (size_t i = 1; i + 1 < refs.size(); ++i) {
                mesh.faces.push_back({ {refs[0], refs[i], refs[i + 1]}, currentMaterial, currentObject });
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
