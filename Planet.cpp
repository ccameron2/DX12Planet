#include "Planet.h"

Planet::Planet(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	mD3DDevice = device;
	mCommandList = commandList;
	mNoise = new FastNoiseLite();
	mNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
}

Planet::~Planet()
{

}

void Planet::CreatePlanet(float frequency, int octaves, int lod)
{
	mMaxLOD = lod;
	if (mMesh) delete mMesh;
	mFrequency = frequency;
	mOctaves = octaves;
	ResetGeometry();

	//for (auto& node : mTriangleTree->mSubnodes)
	//{
	//	Subdivide(node);
	//}

	BuildIndices();

	//ApplyNoise(frequency, octaves);

	CalculateNormals();

	mMesh = new Mesh();

	mMesh->mVertices = mVertices;
	mMesh->mIndices = mIndices;

	mMesh->CalculateDynamicBufferData();
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
		mTriangleTree->AddSub(triangle);
	}

	for (auto& sub : mTriangleTree->mSubnodes)
	{
		sub->mDistance = (mMaxDistance);
	}

	mTriangles.clear();

}

void Planet::GetTriangles(Node* node)
{
	if (node->mNumSubs > 0)
	{
		for (auto& subNode : node->mSubnodes)
		{
			GetTriangles(subNode);
		}
	}
	else 
	{
		mTriangles.push_back(node->mTriangle);
	}
}

bool Planet::CheckNodes(XMFLOAT3 cameraPos, Node* parentNode)
{
	bool ret = false;
	bool combine = false;
	for (auto node : parentNode->mSubnodes)
	{
		if (node->mNumSubs == 0)
		{
			auto A = mVertices[node->mTriangle.Point[0]].Pos;
			auto B = mVertices[node->mTriangle.Point[1]].Pos;
			auto C = mVertices[node->mTriangle.Point[2]].Pos;
		
			auto checkPoint = Center(A, B, C);		
			auto checkVector = XMFLOAT3{ checkPoint.x,checkPoint.y, checkPoint.z };		
			auto distance = Distance(cameraPos, checkVector);

			if (distance < node->mDistance)
			{
				Subdivide(node, node->mLevel);	
				ret = true;
			}
			else if (distance > node->mDistance + 5)
			{
				node->mCombine = true;
				ret = true;
			}
		}
		else
		{
			ret = ret || CheckNodes(cameraPos, node);
		}
	}

	return ret;
}

bool Planet::CombineNodes(Node* node)
{
	if (node->mCombine)
	{
		for(auto& subNode : node->mParent->mSubnodes) delete subNode;
		return true;
	}
	else
	{
		for (auto& subNode : node->mSubnodes)
		{
			CombineNodes(subNode);
		}
	}
	return false;
}

bool Planet::Update(Camera* camera)
{
	if (CheckNodes(camera->mPos, mTriangleTree.get()))
	{
		//for (auto& subNode : mTriangleTree->mSubnodes) { CombineNodes(subNode); }
		BuildIndices();
		mMesh->mVertices = mVertices;
		mMesh->mIndices = mIndices;
		mMesh->CalculateDynamicBufferData();
		return true;
	}
	return false;
}

bool Planet::Subdivide(Node* node, int level)
{
	auto divLevel = level;
	divLevel++;
	if (divLevel > mMaxLOD) return false;
	if (divLevel == mMaxLOD)
	{
		for (int i = 0; i < 3; i++)
		{
			//mVertices[node->mTriangle.Point[i]].Colour = XMFLOAT4({ 1,0,0,0 });

			node->mTriangleChunk = new TriangleChunk( mVertices[node->mTriangle.Point[0]],
													  mVertices[node->mTriangle.Point[1]],
													  mVertices[node->mTriangle.Point[2]],
													  mFrequency, mOctaves, mNoise, mD3DDevice, mCommandList);

			mTriangleChunks.push_back(node->mTriangleChunk);
		}
	}
	std::vector<Triangle> newTriangles = SubdivideTriangle(node->mTriangle);
	for (auto& triangle : newTriangles)
	{
		node->AddSub(triangle);
	}
	for (auto& sub : node->mSubnodes)
	{
		sub->mLevel = divLevel;
		sub->mDistance = mMaxDistance / divLevel;
	}
	return true;
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

void Planet::BuildIndices()
{
	mTriangles.clear();

	for (auto& node : mTriangleTree->mSubnodes)
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

			auto E1 = XMVectorSubtract(a, c);
			auto E2 = XMVectorSubtract(b, c);

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