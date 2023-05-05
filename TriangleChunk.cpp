#include "TriangleChunk.h"

TriangleChunk::TriangleChunk(Vertex v1, Vertex v2, Vertex v3, float frequency, int octaves, FastNoiseLite* noise, ID3D12GraphicsCommandList* commandList)
{
	mVertices.reserve(sizeof(Vertex) * pow(mMaxLOD, 2));
	mIndices.reserve(sizeof(int) * pow(mMaxLOD, 2) * 3);

	// Subdivide with starting triangle
	Subdivide(v1, v2, v3);

	// Apply noise to each vertex
	ApplyNoise(frequency,octaves,noise,mVertices);

	// Calculate normals
	auto normals = CalculateNormals(mVertices, mIndices);
	for (int i = 0; i < mVertices.size(); i++)
	{
		mVertices[i].Normal = normals[i];
	}

	// Create new mesh
	mMesh = new Mesh();

	// Calculate buffer data
	mMesh->mVertices = mVertices;
	mMesh->mIndices = mIndices;
	mMesh->CalculateBufferData(D3DDevice.Get(), commandList);
}


bool TriangleChunk::Subdivide(Vertex v1, Vertex v2, Vertex v3, int level)
{
	// Create initial triangle
	Triangle initialTriangle = { 0, 1, 2 };
	mVertices.push_back(v1);
	mVertices.push_back(v2);
	mVertices.push_back(v3);

	// Add initial triangle to list
	std::vector<Triangle> triangles;
	triangles.reserve(sizeof(Triangle) * pow(mMaxLOD, 2));
	triangles.push_back(initialTriangle);

	// Subdivide triangles
	for (int i = 0; i < mMaxLOD; i++)
	{
		std::vector<Triangle> newTriangles;

		for (const auto& triangle : triangles)
		{
			std::vector<Triangle> subdividedTriangles = SubdivideTriangle(triangle);
			newTriangles.insert(newTriangles.end(), subdividedTriangles.begin(), subdividedTriangles.end());
		}
		triangles = newTriangles;
	}

	// Create final vertex and index lists
	for (const auto& triangle : triangles)
	{
		mIndices.push_back(triangle.Point[0]);
		mIndices.push_back(triangle.Point[1]);
		mIndices.push_back(triangle.Point[2]);
	}

	return true;
}

int TriangleChunk::GetVertexForEdge(int v1, int v2)
{
	// Create pair to use as key in map and normalise edge direction to prevent duplication
	std::pair<int, int> vertexPair(v1, v2);
	if (vertexPair.first > vertexPair.second) { std::swap(vertexPair.first, vertexPair.second); }

	// Either create or reuse vertices
	auto in = mVertexMap.insert({ vertexPair, mVertices.size() });
	if (in.second)
	{
		auto& edge1 = mVertices[v2];
		auto& edge2 = mVertices[v1];
		auto newPoint = AddFloat3(edge1.Pos, edge2.Pos);
		Normalize(&newPoint.Pos);

		// Set colours
		newPoint.Colour.x = std::lerp(edge1.Colour.x, edge2.Colour.x, 0.5);
		newPoint.Colour.y = std::lerp(edge1.Colour.y, edge2.Colour.y, 0.5);
		newPoint.Colour.z = std::lerp(edge1.Colour.z, edge2.Colour.z, 0.5);

		// Add to vertex array
		mVertices.push_back(newPoint);
	}

	return in.first->second;
}

std::vector<Triangle> TriangleChunk::SubdivideTriangle(Triangle triangle)
{
	std::vector<Triangle> newTriangles;
	newTriangles.reserve(sizeof(Triangle) * 4);
	// For each edge
	std::uint32_t mid[3];
	for (int i = 0; i < 3; i++)
	{
		mid[i] = GetVertexForEdge(triangle.Point[i], triangle.Point[(i + 1) % 3]);
	}

	// Add triangles to new array
	newTriangles.push_back({ triangle.Point[0], mid[0], mid[2] });
	newTriangles.push_back({ triangle.Point[1], mid[1], mid[0] });
	newTriangles.push_back({ triangle.Point[2], mid[2], mid[1] });
	newTriangles.push_back({ mid[0], mid[1], mid[2] });

	return newTriangles;
}

void TriangleChunk::ApplyNoise(float frequency, int octaves, FastNoiseLite* noise, std::vector<Vertex>& vertices)
{
	int index = 0;
	for (auto& vertex : vertices)
	{
		// Dont apply noise to first triangles
		if (index > 2)
		{
			auto position = MulFloat3(vertex.Pos, { 200,200,200 });
			auto elevationValue = mSphereOffset + FractalBrownianMotion(noise, position, octaves, frequency);

			elevationValue *= 0.3;

			auto Radius = Distance(vertex.Pos, XMFLOAT3{ 0,0,0 });
			vertex.Pos.x *= 1 + (elevationValue / Radius);
			vertex.Pos.y *= 1 + (elevationValue / Radius);
			vertex.Pos.z *= 1 + (elevationValue / Radius);
		}
		index++;
	}
}