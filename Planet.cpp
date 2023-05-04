#include "Planet.h"

Planet::Planet(Graphics* graphics)
{
	mGraphics = graphics;
	mNoise = new FastNoiseLite();
	mNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	mNoise->SetSeed(std::rand());
}

Planet::~Planet()
{
	mGraphics = nullptr;
}

void Planet::CreatePlanet(float frequency, int octaves, int lod, int scale)
{
	mMaxLOD = lod;
	if (mMesh) delete mMesh;
	mFrequency = frequency;
	mOctaves = octaves;
	mScale = scale;
	mRadius = 0.5 * scale;
	mMaxDistance = mRadius * (mMaxLOD + 1);
	mVertices.reserve(sizeof(Vertex) * MAX_PLANET_VERTS);
	mTriangles.reserve(sizeof(Triangle) * MAX_PLANET_VERTS);
	mIndices.reserve(sizeof(int) * MAX_PLANET_VERTS * 3);

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
		sub->mDistance = mMaxDistance;
	}

	for (auto& vertex : mVertices)
	{
		ApplyNoise(mFrequency, mOctaves, mNoise, vertex);
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
		if (node->mLevel < mMaxLOD) mTriangles.push_back(node->mTriangle);
		else
		{
			if (node->mTriangleChunk) mTriangleChunks.push_back(node->mTriangleChunk);
			else mTriangles.push_back(node->mTriangle);			
		}
	}
}

bool Planet::CheckNodes(Camera* camera, Node* parentNode)
{
	bool ret = false;
	bool combine = false;
	for (auto node : parentNode->mSubnodes)
	{
		if (node->mNumSubs == 0)
		{
			if (mCLOD)
			{
				auto distance = CheckNodeDistance(node, camera->mPos);
				if (distance < node->mDistance)
				{
					ret = Subdivide(node, node->mLevel);
					//ret = true;
				}
				else if (distance > node->mDistance + mScale)
				{
					if (node->mLevel > 0) { node->mCombine = true; ret = true; }
				}
			}
			else
			{
				ret = Subdivide(node, node->mLevel);
			}		
		}
		else
		{
			ret = ret || CheckNodes(camera, node);
		}
	}

	return ret;
}

bool Planet::CombineNodes(Node* node)
{
	if (node->mCombine)
	{
		for (auto& subNode : node->mParent->mSubnodes)
		{
			if (subNode->mTriangleChunk)
			{
				subNode->mTriangleChunk->mCombine = true;
			}
		}
		node->mParent->mSubnodes.clear();
		node->mParent->mNumSubs = 0;
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

bool Planet::Update(Camera* camera, ID3D12GraphicsCommandList* commandList)
{
	SortBaseNodes(camera->mPos);

	mCurrentCommandList = commandList;
	if (CheckNodes(camera, mTriangleTree.get()))
	{
		//log2(distance) > something 
		//; log2(distance) > MaxLOD
		bool combine = false;
		for (auto& subNode : mTriangleTree->mSubnodes) { CombineNodes(subNode); }

		for (auto& chunk : mTriangleChunks)
		{
			if (chunk->mCombine)
				combine = true;
		}

		if (combine)
		{
			mGraphics->EmptyCommandQueue();			
		}

		for (auto& chunk : mTriangleChunks)
		{
			if (chunk->mCombine) delete chunk;
		}

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
	if (divLevel > mMaxLOD) return false;
	if (divLevel == mMaxLOD)
	{
		if (node->mTriangleChunk == nullptr)
		{
			node->mTriangleChunk = new TriangleChunk(mVertices[node->mTriangle.Point[0]],
				mVertices[node->mTriangle.Point[1]],
				mVertices[node->mTriangle.Point[2]],
				mFrequency, mOctaves, mNoise, mCurrentCommandList);
			mTriangleChunks.push_back(node->mTriangleChunk);
			return true;
		}
		return false;		
	}
	std::vector<Triangle> newTriangles = SubdivideTriangle(node->mTriangle);
	divLevel++;
	for (auto& triangle : newTriangles)
	{
		node->AddSub(triangle);
		//dont use pushback
		mTriangles.push_back(triangle);
		node->mNumSubs++;
	}
	for (auto& sub : node->mSubnodes)
	{
		sub->mLevel = divLevel;
		if(sub->mLevel >= mMaxLOD - 1) sub->mDistance = mRadius / mMaxLOD + 0.3;
		else sub->mDistance = mMaxDistance / divLevel;
	}
	return true;
}

int Planet::GetVertexForEdge(int v1, int v2)
{
	//mTriangles.reserve(num pushbacks)
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
		
		ApplyNoise(mFrequency, mOctaves, mNoise, newPoint);

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

	int subnodeIndices[20];
	for (int i = 0; i < 20; i++)
	{
		subnodeIndices[i] = i;
	}

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

		int temp = subnodeIndices[minIndex];
		subnodeIndices[minIndex] = subnodeIndices[i];
		subnodeIndices[i] = temp;
	}

	std::vector<Node*> sortedSubnodes;
	for (int i = 0; i < 20; i++)
	{
		sortedSubnodes.push_back(mTriangleTree->mSubnodes[subnodeIndices[i]]);
	}

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
	std::vector<TriangleChunk*> triangleChunks;
	mTriangleChunks = triangleChunks;
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

void Planet::ApplyNoise(float frequency, int octaves, FastNoiseLite* noise, Vertex& vertex)
{
	XMVECTOR pos = XMLoadFloat3(&vertex.Pos);
	pos = XMVectorMultiply(pos, { 200,200,200 });
	XMFLOAT3 position; XMStoreFloat3(&position, pos);
	auto elevationValue = FractalBrownianMotion(noise, position, octaves, frequency);
	//auto ElevationValue = 1 + noise.GetNoise(0.5 * vertex.Pos.x * 100, 0.5 * vertex.Pos.y * 100, 0.5 * vertex.Pos.z * 100);
	elevationValue *= 0.3;
	auto Radius = Distance(vertex.Pos, XMFLOAT3{ 0,0,0 });
	vertex.Pos.x *= 1 + (elevationValue / Radius);
	vertex.Pos.y *= 1 + (elevationValue / Radius);
	vertex.Pos.z *= 1 + (elevationValue / Radius);
}

float Planet::CheckNodeTriSize(Node* node, Camera* camera)
{
	auto A = mVertices[node->mTriangle.Point[0]].Pos;
	auto B = mVertices[node->mTriangle.Point[1]].Pos;
	auto C = mVertices[node->mTriangle.Point[2]].Pos;

	auto vA = XMLoadFloat3(&A);
	auto vB = XMLoadFloat3(&B);
	auto vC = XMLoadFloat3(&C);
	auto mV = XMLoadFloat4x4(&camera->mViewMatrix);
	auto mP = XMLoadFloat4x4(&camera->mProjectionMatrix);

	auto viewA = XMVector3Transform(vA, mV);
	auto viewB = XMVector3Transform(vB, mV);
	auto viewC = XMVector3Transform(vC, mV);

	auto viewProjA = XMVector3Transform(viewA, mP);
	auto viewProjB = XMVector3Transform(viewB, mP);
	auto viewProjC = XMVector3Transform(viewC, mP);

	XMStoreFloat3(&A, viewProjA);
	XMStoreFloat3(&B, viewProjB);
	XMStoreFloat3(&C, viewProjC);

	auto distance = Distance(A, B);

	return distance;
}