#pragma once

#include "Utility.h"
#include <map>
#include <vector>
#include "Mesh.h"
#include "FastNoiseLite.h"
#include "Common.h"

class TriangleChunk
{
public:
	TriangleChunk(Vertex v1, Vertex v2, Vertex v3, float frequency, int octaves, FastNoiseLite* noise, ID3D12GraphicsCommandList* commandList);
	~TriangleChunk() { delete mMesh; mMesh = nullptr; };

	// Geometry
	std::map<std::pair<int, int>, int> mVertexMap;
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;

	Mesh* mMesh;
	bool mCombine = false;
private:
	// Subdivide mesh
	bool Subdivide(Vertex v1, Vertex v2, Vertex v3, int level = 0);

	// Get vertex for triangle edge
	int GetVertexForEdge(int v1, int v2);

	// Subdivide triangle
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);

	// Apply noise
	void ApplyNoise(float frequency, int octaves, FastNoiseLite* noise, std::vector<Vertex>& vertices);

	int mMaxLOD = 6;
	float mSphereOffset = 0.0;

};

