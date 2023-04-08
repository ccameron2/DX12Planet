#pragma once

#include "Utility.h"
#include <map>
#include <vector>
#include "Mesh.h"
#include "FastNoiseLite.h"

class TriangleChunk
{
public:
	TriangleChunk(Vertex v1, Vertex v2, Vertex v3, float frequency, int octaves, FastNoiseLite* noise, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	~TriangleChunk();
	std::map<std::pair<int, int>, int> mVertexMap;
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;

	bool Subdivide(Vertex v1, Vertex v2, Vertex v3, int level = 0);
	int GetVertexForEdge(int v1, int v2);
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);
	float FractalBrownianMotion(FastNoiseLite* fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency);
	void ApplyNoise(float frequency, int octaves, FastNoiseLite* noise);
	int mMaxLOD = 4;
	Mesh* mMesh;
private:

};

