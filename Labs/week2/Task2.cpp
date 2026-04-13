#include <iostream>
#include <lodepng.h>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include "Vector3.hpp"

// The goal for this lab is to draw a triangle mesh loaded from an OBJ file from scratch,
// building on the image drawing code from last week's lab.
// The mesh consists of a list of 3D vertices, that describe the points forming the mesh.
// It also has a list of triangle indices, that determine which vertices are used to form each triangle.
// This time, we'll also load the triangle indices, and use them to draw lines connecting the vertices.
// This will make a wireframe render of the mesh.

void setPixel(std::vector<uint8_t>& image, int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	int pixelIdx = x + y * width;
	image[pixelIdx * 4 + 0] = r;
	image[pixelIdx * 4 + 1] = g;
	image[pixelIdx * 4 + 2] = b;
	image[pixelIdx * 4 + 3] = a;
}

void drawLine(std::vector<uint8_t>& image, int width, int height, int startX, int startY, int endX, int endY)
{
	// Task 1: Bresenham's line algorithm
	int dx = std::abs(endX - startX);
	int dy = std::abs(endY - startY);
	int sx = (startX < endX) ? 1 : -1;
	int sy = (startY < endY) ? 1 : -1;
	int err = dx - dy;

	while (true) {
		if (startX >= 0 && startX < width && startY >= 0 && startY < height) {
			setPixel(image, startX, startY, width, height, 255, 255, 255);
		}
		if (startX == endX && startY == endY) break;
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			startX += sx;
		}
		if (e2 < dx) {
			err += dx;
			startY += sy;
		}
	}
}


int main()
{
	std::string outputFilename = "output.png";

	const int width = 512, height = 512;
	const int nChannels = 4;

	// Setting up an image buffer
	// This std::vector has one 8-bit value for each pixel in each row and column of the image, and
	// for each of the 4 channels (red, green, blue and alpha).
	// Remember 8-bit unsigned values can range from 0 to 255.
	std::vector<uint8_t> imageBuffer(height*width*nChannels);

	// This line sets the memory block occupied by the image to all zeros.
	memset(&imageBuffer[0], 0, width * height * nChannels * sizeof(uint8_t));

	std::string bunnyFilename = "../models/stanford_bunny_simplified.obj";

	std::ifstream bunnyFile(bunnyFilename);


	// *** Task 2 ***
	// Your next task is to load all the vertices from the OBJ file.
	// I've given you some starter code here that reads through each line of the
	// OBJ file and makes it into a stringstream.
	// For these V lines, you should load the X, Y and Z coordinates into a new vector
	// and push it back into your array of vertices.
	std::vector<Vector3> vertices;
	std::vector<std::vector<unsigned int>> faces;
	std::string line;
	while (!bunnyFile.eof())
	{
		std::getline(bunnyFile, line);
		std::stringstream lineSS(line.c_str());
		char lineStart;
		lineSS >> lineStart;
		char ignoreChar;
		if (lineStart == 'v') {
			Vector3 v;
			for (int i = 0; i < 3; ++i) lineSS >> v[i];
			vertices.push_back(v);
		}



		if (lineStart == 'f') {

			std::vector<unsigned int> face;
			// *** YOUR CODE HERE ***
			// This time we care about faces!
			// Load this face from the line, pushing it back into the list of faces.
			// Be careful to ignore the "/" characters, and the extra texture and normal indices.
			unsigned int idx, idxTex, idxNorm;
			while (lineSS >> idx >> ignoreChar >> idxTex >> ignoreChar >> idxNorm) {
				face.push_back(idx - 1);
			}
			if (face.size() > 0) faces.push_back(face);
		}
	}

	for (int f = 0; f < faces.size(); ++f) {
		// **** Task 3 ****
		// Finally, let's draw the faces!

		// First, load the vertices, and resize them like we did in Task1.cpp
		// Then, call DrawLine three times, to draw each side of the triangle!

		int x0 = vertices[faces[f][0]][0] * 250 + width / 2;
		int y0 = -vertices[faces[f][0]][1] * 250 + height / 2;
		int x1 = vertices[faces[f][1]][0] * 250 + width / 2;
		int y1 = -vertices[faces[f][1]][1] * 250 + height / 2;
		int x2 = vertices[faces[f][2]][0] * 250 + width / 2;
		int y2 = -vertices[faces[f][2]][1] * 250 + height / 2;
		drawLine(imageBuffer, width, height, x0, y0, x1, y1);
		drawLine(imageBuffer, width, height, x1, y1, x2, y2);
		drawLine(imageBuffer, width, height, x2, y2, x0, y0);
	}

	// *** Encoding image data ***
	// PNG files are compressed to save storage space. 
	// The lodepng::encode function applies this compression to the image buffer and saves the result 
	// to the filename given.
	int errorCode;
	errorCode = lodepng::encode(outputFilename, imageBuffer, width, height);
	if (errorCode) { // check the error code, in case an error occurred.
		std::cout << "lodepng error encoding image: " << lodepng_error_text(errorCode) << std::endl;
		return errorCode;
	}

	return 0;
}
