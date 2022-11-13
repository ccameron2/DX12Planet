#include "Icosahedron.h"

Icosahedron::Icosahedron(int numVertices, int numIndices, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList)
{
	CreateGeometry(numVertices, numIndices, d3DDevice, commandList);
}

Icosahedron::~Icosahedron()
{
}

void Icosahedron::CreateGeometry(int numVertices, int numIndices, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList)
{
	mGeometryData = std::make_unique<GeometryData>();

	const float X = 0.525731112119133606f;
	const float Z = 0.850650808352039932f;
	const float N = 0.0f;

	vertices = 
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

	indices =
	{
		0,4,1,
		0,9,4,
		9,5,4,
		4,5,8,
		4,8,1, 
		8,10,1,
		8,3,10,
		5,3,8, 
		5,2,3,
		2,7,3,
		7,10,3,
		7,6,10,
		7,11,6,
		11,0,6,
		0,1,6,
		6,1,10,
		9,0,11,
		9,11,2,
		9,2,5,
		7,2,11
	};
	
	for (int i = 0; i < indices.size(); i += 3)
	{
		triangles.push_back(Triangle{ indices[i],indices[i + 1],indices[i + 2] });
	}

	for (int i = 0; i < mRecursions; i++)
	{
		SubdivideIcosphere();
	}

	//indices.clear();

	for (int i = 0; i < triangles.size(); i++)
	{
		indices.push_back(triangles[i].Point[0]);
		indices.push_back(triangles[i].Point[1]);
		indices.push_back(triangles[i].Point[2]);
	}

	mGeometryData->vertices = vertices;
	mGeometryData->indices = indices;

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
	auto in = mVertexMap.insert({ vertexPair, vertices.size() });
	if (in.second)
	{
		auto& edge1 = vertices[first];
		auto& edge2 = vertices[second];
		auto point = AddVector(edge1.Pos,edge2.Pos);
		Normalize(&point.Pos);
		/*point.Color = edge1.Color;*/
		vertices.push_back(point);
	}
	return in.first->second;
}


void Icosahedron::SubdivideIcosphere()
{
	std::vector<Triangle> newTriangles;
	for (auto& triangle : triangles)
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
	triangles.swap(newTriangles);

	// Clear the vertex map for re-use
	mVertexMap.clear();
}