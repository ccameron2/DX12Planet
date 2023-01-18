#include "Planet.h"

Vertex AddFloat32(XMFLOAT3 a, XMFLOAT3 b)
{
	Vertex result;

	result.Pos.x = a.x + b.x;
	result.Pos.y = a.y + b.y;
	result.Pos.z = a.z + b.z;

	return result;
}

void Normalize2(XMFLOAT3* p)
{
	float w = sqrt(p->x * p->x + p->y * p->y + p->z * p->z);
	p->x /= w;
	p->y /= w;
	p->z /= w;
}

Planet::Planet(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	CreateIcosahedron();
	mGeometryData->CalculateBufferData(device, commandList);
}

Planet::~Planet()
{
}

void Planet::CreateIcosahedron()
{
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

	for (auto& node : mTriangleTree->mChildren)
	{
		Subdivide(node);
	}

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

	//CalculateNormals();

	mGeometryData = make_unique<GeometryData>();

	mGeometryData->mVertices = mVertices;
	mGeometryData->mIndices = mIndices;
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
		auto newPoint = AddFloat32(edge1.Pos, edge2.Pos);
		Normalize2(&newPoint.Pos);

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

void Planet::CalculateNormals()
{
	// Map of vertex to triangles in Triangles array
	int numVerts = mVertices.size();
	std::vector<std::array<int32_t, 8>> VertToTriMap;
	for (int i = 0; i < numVerts; i++)
	{
		std::array<int32_t, 8> array{ -1,-1,-1,-1,-1,-1,-1,-1 };
		VertToTriMap.push_back(array);
	}

	// For each triangle for each vertex add triangle to vertex array entry
	for (int i = 0; i < mIndices.size(); i++)
	{
		for (int j = 0; j < 8; j++)
		{
			if (VertToTriMap[mIndices[i]][j] < 0)
			{
				VertToTriMap[mIndices[i]][j] = i / 3;
				break;
			}
		}
	}

	std::vector<XMFLOAT3> NTriangles;

	for (int i = 0; i < mIndices.size() / 3; i++)
	{
		XMFLOAT3 normal = {};
		NTriangles.push_back(normal);
	}

	int index = 0;
	for (int i = 0; i < NTriangles.size(); i++)
	{
		NTriangles[i].x = mIndices[index];
		NTriangles[i].y = mIndices[index + 1];
		NTriangles[i].z = mIndices[index + 2];
		index += 3;
	}

	for (int i = 0; i < mVertices.size(); i++)
	{
		mNormals.push_back({ 0,0,0 });
	}

	// For each vertex collect the triangles that share it and calculate the face normal
	for (int i = 0; i < mVertices.size(); i++)
	{
		for (auto& triangle : VertToTriMap[i])
		{
			// This shouldnt happen
			if (triangle < 0)
			{
				continue;
			}

			// Get vertices from triangle index
			auto A = mVertices[NTriangles[triangle].x];
			auto B = mVertices[NTriangles[triangle].y];
			auto C = mVertices[NTriangles[triangle].z];

			// Calculate edges
			auto a = XMLoadFloat3(&A.Pos);
			auto b = XMLoadFloat3(&B.Pos);
			auto c = XMLoadFloat3(&C.Pos);

			auto E1 = XMVectorSubtract(a, b);
			auto E2 = XMVectorSubtract(c, b);

			// Calculate normal with cross product and normalise
			XMFLOAT3 Normal; XMStoreFloat3(&Normal, XMVector3Normalize(XMVector3Cross(E1, E2)));

			mNormals[i].x += Normal.x;
			mNormals[i].y += Normal.y;
			mNormals[i].z += Normal.z;
		}
	}

	// Average the face normals
	for (auto& normal : mNormals)
	{
		XMFLOAT3 normalizedNormal;
		XMStoreFloat3(&normalizedNormal, XMVector3Normalize(XMLoadFloat3(&normal)));
		normal = normalizedNormal;
	}

	for (int i = 0; i < mVertices.size(); i++)
	{
		mVertices[i].Normal = mNormals[i];
	}

}