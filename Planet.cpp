#include "Planet.h"

Planet::Planet(ID3D12GraphicsCommandList* commandList)
{
	mCommandList = commandList;
	mNoise = new FastNoiseLite();
	mNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
}

Planet::~Planet()
{

}

void Planet::CreatePlanet(float frequency, int octaves, int lod, int scale)
{
	mMaxLOD = lod;
	if (mMesh) delete mMesh;
	mFrequency = frequency;
	mOctaves = octaves;
	mScale = scale;
	mRadius = 1.0f * scale;
	mMaxDistance = mRadius * (mMaxLOD + 1);

	ResetGeometry();

	BuildIndices();

	auto normals = CalculateNormals(mVertices,mIndices);

	for (int i = 0; i < mVertices.size(); i++)
	{
		mVertices[i].Normal = normals[i];
	}

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

//	for (auto triangleChunk : mTriangleChunks) { delete triangleChunk; }
	mTriangleChunks.clear();

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
		if(node->mLevel < mMaxLOD) mTriangles.push_back(node->mTriangle);
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
			auto distance = CheckNodeDistance(node, cameraPos);
			if (distance < node->mDistance)
			{
				ret = Subdivide(node, node->mLevel);	
				//ret = true;
			}
			else if (distance > node->mDistance + 2)
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
	if (node->mLevel != 0)
	{
		if (node->mCombine)
		{
			if (node->mLevel < mCombineLOD) mCombineLOD = node->mLevel;
			return true;
		}
		else
		{
			for (auto& subNode : node->mSubnodes)
			{
				CombineNodes(subNode);
			}
		}
	}	
	return false;
}

bool Planet::Update(Camera* camera)
{
	SortBaseNodes(camera->mPos);

	if (CheckNodes(camera->mPos, mTriangleTree.get()))
	{
/*		bool combine = false;
		for (auto& subNode : mTriangleTree->mSubnodes) { if (CombineNodes(subNode)) combine = true; }
		if (combine)
		{
			CreatePlanet(mFrequency, mOctaves, mCombineLOD);
		}
		else {*/ BuildIndices(); /*}*/
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
	if (divLevel > mMaxLOD) 
	{
		//for (int i = 0; i < 3; i++) mVertices[node->mTriangle.Point[i]].Colour = XMFLOAT4{ 0,0,1,0 };
		return false;
	}
	if (divLevel == mMaxLOD && node->mTriangleChunk == nullptr)
	{
		node->mTriangleChunk = new TriangleChunk(mVertices[node->mTriangle.Point[0]],
												 mVertices[node->mTriangle.Point[1]],
												 mVertices[node->mTriangle.Point[2]],
												 mFrequency, mOctaves, mNoise, mCommandList);
		mTriangleChunks.push_back(node->mTriangleChunk);
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

float Planet::CheckNodeDistance(Node* node, XMFLOAT3 cameraPos)
{
	auto A = mVertices[node->mTriangle.Point[0]].Pos;
	auto B = mVertices[node->mTriangle.Point[1]].Pos;
	auto C = mVertices[node->mTriangle.Point[2]].Pos;

	auto checkPoint = Center(A, B, C);
	auto checkVector = XMFLOAT3{ checkPoint.x,checkPoint.y, checkPoint.z };
	auto distance = Distance(cameraPos, checkVector);

	return distance;
}

void Planet::SortBaseNodes(XMFLOAT3 cameraPos)
{
	float distArray[20];
	for (int i = 0; i < 20; i++)
	{
		distArray[i] = CheckNodeDistance(mTriangleTree->mSubnodes[i], cameraPos);
	}

	// First, we need to create an array to hold the indices of mSubnodes
	int subnodeIndices[20];
	for (int i = 0; i < 20; i++)
	{
		subnodeIndices[i] = i;
	}

	// Next, we'll use a simple selection sort algorithm to sort the subnodeIndices array based on the distances in distArray
	for (int i = 0; i < 19; i++)
	{
		int minIndex = i;
		for (int j = i + 1; j < 20; j++)
		{
			if (distArray[subnodeIndices[j]] < distArray[subnodeIndices[minIndex]])
			{
				minIndex = j;
			}
		}

		// Swap the elements at minIndex and i
		int temp = subnodeIndices[minIndex];
		subnodeIndices[minIndex] = subnodeIndices[i];
		subnodeIndices[i] = temp;
	}

	// Finally, we can create a new array of subnodes in the sorted order
	std::vector<Node*> sortedSubnodes;
	for (int i = 0; i < 20; i++)
	{
		sortedSubnodes.push_back(mTriangleTree->mSubnodes[subnodeIndices[i]]);
	}

	// Replace the mSubnodes array in mTriangleTree with the sorted array
	mTriangleTree->mSubnodes = sortedSubnodes;
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

