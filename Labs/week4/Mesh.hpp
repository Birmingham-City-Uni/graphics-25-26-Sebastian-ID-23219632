#include <Eigen/Dense>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>

struct Mesh {
	std::vector<Eigen::Vector3f> verts;
	std::vector<std::vector<unsigned int>> faces;
};


Mesh loadMeshFile(const std::string& filename)
{
	Mesh mesh;

	std::ifstream file(filename);

	std::string line;
	while (std::getline(file, line))
	{
		std::stringstream lineSS(line);
		std::string lineStart;
		lineSS >> lineStart;

		if (lineStart == "v") {
			Eigen::Vector3f v;
			for (int i = 0; i < 3; ++i) lineSS >> v[i];
			mesh.verts.push_back(v);
		}

		if (lineStart == "f") {
			std::vector<unsigned int> face;
			std::string vertexToken;
			while (lineSS >> vertexToken) {
				std::stringstream vertexSS(vertexToken);
				unsigned int idx = 0;
				vertexSS >> idx;
				if (idx > 0) {
					face.push_back(idx - 1);
				}
			}
			if (face.size() > 0) mesh.faces.push_back(face);
		}
	}

	return mesh;
}
