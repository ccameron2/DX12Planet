#include "Icosahedron.h"
#include "FastNoiseLite.h"
Icosahedron::Icosahedron(int numVertices, int numIndices, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList)
{
	CreateGeometry(numVertices, numIndices, d3DDevice, commandList);
}

Icosahedron::~Icosahedron()
{
}

float distance(const XMFLOAT3& v1, const XMFLOAT3& v2)
{
	XMVECTOR vector1 = XMLoadFloat3(&v1);
	XMVECTOR vector2 = XMLoadFloat3(&v2);
	XMVECTOR vectorSub = XMVectorSubtract(vector1, vector2);
	XMVECTOR length = XMVector3Length(vectorSub);

	float distance = 0.0f;
	XMStoreFloat(&distance, length);
	return distance;
}

void Icosahedron::CreateGeometry(int numVertices, int numIndices, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList)
{


	mGeometryData = std::make_unique<GeometryData>();

	const float X = 0.525731112119133606f;
	const float Z = 0.850650808352039932f;
	const float N = 0.0f;

	mVertices = 
	{
		Vertex({ XMFLOAT3(-X,N,Z), XMFLOAT4(Colors::Red)}),
		Vertex({ XMFLOAT3(X,N,Z), XMFLOAT4(Colors::Orange)}),
		Vertex({ XMFLOAT3(-X,N,-Z), XMFLOAT4(Colors::Yellow)}),
		Vertex({ XMFLOAT3(X,N,-Z), XMFLOAT4(Colors::Green)}),
		Vertex({ XMFLOAT3(N,Z,X), XMFLOAT4(Colors::Blue)}),
		Vertex({ XMFLOAT3(N,Z,-X), XMFLOAT4(Colors::Indigo)}),
		Vertex({ XMFLOAT3(N,-Z,X), XMFLOAT4(Colors::Violet)}),
		Vertex({ XMFLOAT3(N,-Z,-X), XMFLOAT4(Colors::Magenta)}),
		Vertex({ XMFLOAT3(Z,X,N), XMFLOAT4(Colors::Black)}),
		Vertex({ XMFLOAT3(-Z,X,N), XMFLOAT4(Colors::Gold)}),
		Vertex({ XMFLOAT3(Z,-X,N), XMFLOAT4(Colors::Pink)}),
		Vertex({ XMFLOAT3(-Z,-X,N), XMFLOAT4(Colors::Silver)})
	};

	mIndices =
	{
		0,4,1,	0,9,4,	9,5,4,
		4,5,8,	4,8,1,	8,10,1,
		8,3,10, 5,3,8,	5,2,3,
		2,7,3,	7,10,3,	7,6,10,
		7,11,6,	11,0,6,	0,1,6,	
		6,1,10,	9,0,11,	9,11,2,
		9,2,5,	7,2,11
	};
	
	for (int i = 0; i < mIndices.size(); i += 3)
	{
		mTriangles.push_back(Triangle{ mIndices[i],mIndices[i + 1],mIndices[i + 2] });
	}

	for (int i = 0; i < mRecursions; i++)
	{
		SubdivideIcosphere();
	}

	//FastNoiseLite noise;
	//noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

	//for (auto& vertex : mVertices)
	//{
	//	/*XMVECTOR pos = XMLoadFloat3(&vertex.Pos);
	//	pos = XMVectorMultiply(pos, { 1000,1000,1000 });
	//	XMFLOAT3 position; XMStoreFloat3(&position, pos);
	//	auto ElevationValue = 1 + FractalBrownianMotion(position, mOctaves, mFrequency);*/
	//	auto ElevationValue = 1 + noise.GetNoise(0.5 * vertex.Pos.x * 100, 0.5 * vertex.Pos.y * 100, 0.5 * vertex.Pos.z * 100);
	//	auto Radius = distance(vertex.Pos, XMFLOAT3{ 0,0,0 });
	//	vertex.Pos.x *= 1 + (ElevationValue / Radius);
	//	vertex.Pos.y *= 1 + (ElevationValue / Radius);
	//	vertex.Pos.z *= 1 + (ElevationValue / Radius);
	//}

	//mIndices.clear();

	for (int i = 0; i < mTriangles.size(); i++)
	{
		mIndices.push_back(mTriangles[i].Point[0]);
		mIndices.push_back(mTriangles[i].Point[1]);
		mIndices.push_back(mTriangles[i].Point[2]);
	}

	mGeometryData->mVertices = mVertices;
	mGeometryData->mIndices = mIndices;

	mGeometryData->CalculateBufferData(d3DDevice,commandList);
}



Vertex AddVector(XMFLOAT3 a, XMFLOAT3 b)
{
	Vertex result;

	result.Pos.x = a.x + b.x;
	result.Pos.y = a.y + b.y;
	result.Pos.z = a.z + b.z;

	return result;
}

void Normalize(XMFLOAT3* p)
{
	float w = sqrt(p->x * p->x + p->y * p->y + p->z * p->z);
	p->x /= w;
	p->y /= w;
	p->z /= w;
}

int Icosahedron::VertexForEdge(int first, int second)
{
	// Create pair to use as key in map and normalise edge direction to prevent duplication
	std::pair<int, int> vertexPair(first, second);
	if (vertexPair.first > vertexPair.second) { std::swap(vertexPair.first, vertexPair.second); }

	// Either create or reuse vertices
	auto in = mVertexMap.insert({ vertexPair, mVertices.size() });
	if (in.second)
	{
		auto& edge1 = mVertices[first];
		auto& edge2 = mVertices[second];
		auto point = AddVector(edge1.Pos,edge2.Pos);
		Normalize(&point.Pos);
		/*point.Color = edge1.Color;*/
		mVertices.push_back(point);
	}
	return in.first->second;
}


void Icosahedron::SubdivideIcosphere()
{
	std::vector<Triangle> newTriangles;
	for (auto& triangle : mTriangles)
	{
		// For each edge
		int mid[3];
		for (int e = 0; e < 3; e++)
		{
			mid[e] = VertexForEdge(triangle.Point[e], triangle.Point[(e + 1) % 3]);
		}

		// Add triangles to new array
		newTriangles.push_back({ triangle.Point[0], mid[0], mid[2] });
		newTriangles.push_back({ triangle.Point[1], mid[1], mid[0] });
		newTriangles.push_back({ triangle.Point[2], mid[2], mid[1] });
		newTriangles.push_back({ mid[0], mid[1], mid[2] });
	}

	// Swap old triangles with new ones
	mTriangles.swap(newTriangles);

	// Clear the vertex map for re-use
	mVertexMap.clear();
}


float Icosahedron::FractalBrownianMotion(XMFLOAT3 fractalInput, float octaves, float frequency)
{
	float result = 0;
	float amplitude = 0.5;
	float lacunarity = 2.0;
	float gain = 0.5;
	FastNoiseLite noise;
	noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

	// Add iterations of noise at different frequencies to get more detail from perlin noise
	for (int i = 0; i < octaves; i++)
	{
		result += amplitude * noise.GetNoise(frequency * fractalInput.x, frequency * fractalInput.y, frequency * fractalInput.z);
		frequency *= lacunarity;
		amplitude *= gain;
	}

	return result;
}