#include "Planet.h"

Planet::Planet()
{
}

Planet::~Planet()
{
}

void Planet::CreatePlanet(float frequency, int octaves, int lod)
{
	mMaxLOD = lod;
	if (mGeometryData) mGeometryData.reset();

	ResetGeometry();

	for (auto& node : mTriangleTree->mChildren)
	{
		Subdivide(node);
	}

	BuildIndices();

	ApplyNoise(frequency, octaves);

	mGeometryData = make_unique<GeometryData>();

	mGeometryData->mVertices = mVertices;
	mGeometryData->mIndices = mIndices;

	mGeometryData->CalculateDynamicBufferData();
}

void Planet::ResetGeometry()
{
	mVertices.clear();
	mIndices.clear();
	mTriangles.clear();
	mVertexMap.clear();
	mTriangleTree.reset();

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
		Vertex({ XMFLOAT3(Z,X,N), XMFLOAT4(Colors::Cyan)}),
		Vertex({ XMFLOAT3(-Z,X,N), XMFLOAT4(Colors::Gold)}),
		Vertex({ XMFLOAT3(Z,-X,N), XMFLOAT4(Colors::Pink)}),
		Vertex({ XMFLOAT3(-Z,-X,N), XMFLOAT4(Colors::Silver)})
	};

	mIndices =
	{
		1,4,0,	4,9,0,	4,5,9,
		8,5,4,	1,8,4,	1,10,8,
		10,3,8, 8,3,5,	3,2,5,
		3,7,2,	3,10,7,	10,6,7,
		6,11,7,	6,0,11,	6,1,0,
		10,1,6,	11,0,9,	2,11,9,
		5,2,9,	11,2,7
	};


	// Build triangles
	for (int i = 0; i < mIndices.size(); i += 3)
	{
		mTriangles.push_back(Triangle{ mIndices[i],mIndices[i + 1],mIndices[i + 2] });
	}

	mTriangleTree = make_unique<Node>();
	for (auto& triangle : mTriangles)
	{
		mTriangleTree->AddChild(triangle);
	}

	mTriangles.clear();

}

void Planet::GetTriangles(Node* node)
{
	if (node->mChildren.size() > 0)
	{
		for (auto& subNode : node->mChildren)
		{
			GetTriangles(subNode);
		}
	}
	else 
	{
		mTriangles.push_back(node->mTriangle);
	}
}

void Planet::Update()
{
}

void Planet::Subdivide(Node* node, int level)
{
	if (level >= mMaxLOD) return;
	level++;
	std::vector<Triangle> newTriangles = SubdivideTriangle(node->mTriangle);
	for (auto& triangle : newTriangles)
	{
		Subdivide(node->AddChild(triangle),level);
	}
}

int Planet::GetVertexForEdge(int v1, int v2)
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

std::vector<Triangle> Planet::SubdivideTriangle(Triangle triangle)
{
	std::vector<Triangle> newTriangles;

	// For each edge
	std::uint32_t mid[3];
	for (int e = 0; e < 3; e++)
	{
		mid[e] = GetVertexForEdge(triangle.Point[e], triangle.Point[(e + 1) % 3]);
	}

	// Add triangles to new array
	newTriangles.push_back({ triangle.Point[0], mid[0], mid[2] });
	newTriangles.push_back({ triangle.Point[1], mid[1], mid[0] });
	newTriangles.push_back({ triangle.Point[2], mid[2], mid[1] });
	newTriangles.push_back({ mid[0], mid[1], mid[2] });

	return newTriangles;
}

void Planet::BuildIndices()
{
	for (auto& node : mTriangleTree->mChildren)
	{
		GetTriangles(node);
	}

	mIndices.clear();

	for (int i = 0; i < mTriangles.size(); i++)
	{
		mIndices.push_back(mTriangles[i].Point[0]);
		mIndices.push_back(mTriangles[i].Point[1]);
		mIndices.push_back(mTriangles[i].Point[2]);
	}
}

float Planet::FractalBrownianMotion(FastNoiseLite fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency)
{
	float result = 0;
	float amplitude = 0.5;
	float lacunarity = 2.0;
	float gain = 0.5;

	// Add iterations of noise at different frequencies to get more detail from perlin noise
	for (int i = 0; i < octaves; i++)
	{
		result += amplitude * fastNoise.GetNoise(frequency * fractalInput.x, frequency * fractalInput.y, frequency * fractalInput.z);
		frequency *= lacunarity;
		amplitude *= gain;
	}

	return result;
}

void Planet::ApplyNoise(float frequency, int octaves)
{
	FastNoiseLite noise;
	noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	for (auto& vertex : mVertices)
	{
		XMVECTOR pos = XMLoadFloat3(&vertex.Pos);
		pos = XMVectorMultiply(pos, { 100,100,100 });
		XMFLOAT3 position; XMStoreFloat3(&position, pos);
		auto ElevationValue = 1 + FractalBrownianMotion(noise, position, octaves, frequency);
		//auto ElevationValue = 1 + noise.GetNoise(0.5 * vertex.Pos.x * 100, 0.5 * vertex.Pos.y * 100, 0.5 * vertex.Pos.z * 100);
		ElevationValue *= 1.5;
		auto Radius = Distance(vertex.Pos, XMFLOAT3{ 0,0,0 });
		vertex.Pos.x *= 1 + (ElevationValue / Radius);
		vertex.Pos.y *= 1 + (ElevationValue / Radius);
		vertex.Pos.z *= 1 + (ElevationValue / Radius);
	}
}