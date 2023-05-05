#include "Planet.h"

Planet::Planet(Graphics* graphics)
{
	mGraphics = graphics;

	// Create new noise object with random seed
	mNoise = new FastNoiseLite();
	mNoise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	mNoise->SetSeed(std::rand());
}

Planet::~Planet()
{
	mGraphics = nullptr;
}

void Planet::CreatePlanet(float frequency, int octaves, int lod, int scale, int seed)
{
	// Set data from params
	mNoise->SetSeed(seed);
	mMaxLOD = lod;
	mFrequency = frequency;
	mOctaves = octaves;
	mScale = scale;
	mRadius = 0.5 * scale;
	mMaxDistance = mRadius * (mMaxLOD + 1);

	// Clear old mesh
	if (mMesh) delete mMesh;

	// Reserve memory
	mVertices.reserve(sizeof(Vertex) * MAX_PLANET_VERTS);
	mTriangles.reserve(sizeof(Triangle) * MAX_PLANET_VERTS);
	mIndices.reserve(sizeof(int) * MAX_PLANET_VERTS * 3);

	// Reset geometry to icosahedron
	ResetGeometry();

	BuildIndices();

	// Calculate normals
	auto normals = CalculateNormals(mVertices,mIndices);
	for (int i = 0; i < mVertices.size(); i++)
	{
		mVertices[i].Normal = normals[i];
	}

	// Make a new mesh and calculate dynamic buffer data
	mMesh = new Mesh();
	mMesh->mVertices = mVertices;
	mMesh->mIndices = mIndices;
	mMesh->CalculateDynamicBufferData();
}

void Planet::ResetGeometry()
{
	// Clear old data
	mVertices.clear();
	mIndices.clear();
	mTriangles.clear();
	mVertexMap.clear();
	mTriangleTree.reset();
	mTriangleChunks.clear();

	// Base Icosahedron
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

	// Make quadtree and add triangles to it
	mTriangleTree = make_unique<Node>();
	for (auto& triangle : mTriangles)
	{
		mTriangleTree->AddSub(triangle);
	}

	// Set subdivision distances to max on base nodes
	for (auto& sub : mTriangleTree->mSubnodes)
	{
		sub->mDistance = mMaxDistance;
	}

	// Apply noise to the vertices
	for (auto& vertex : mVertices)
	{
		ApplyNoise(mFrequency, mOctaves, mNoise, vertex);
	}

	// Clear the triangles list
	mTriangles.clear();

}

void Planet::GetTriangles(Node* node)
{
	// If the node has subnodes
	if (node->mNumSubs > 0)
	{
		// Call this function on each subnode
		for (auto& subNode : node->mSubnodes)
		{
			GetTriangles(subNode);
		}
	}
	else 
	{
		// Push triangle indices
		if (node->mLevel < mMaxLOD) mTriangles.push_back(node->mTriangle);
		else
		{
			// If the node has a triangle chunk, push it onto the chunks list
			if (node->mTriangleChunk) 
				mTriangleChunks.push_back(node->mTriangleChunk);
			else // Push back the triangle indices instead
				mTriangles.push_back(node->mTriangle);			
		}
	}
}

bool Planet::CheckNodes(Camera* camera, Node* parentNode)
{
	bool ret = false;
	bool combine = false;

	// For each subnode in this node
	for (auto node : parentNode->mSubnodes)
	{
		// If the node has no subnodes
		if (node->mNumSubs == 0)
		{
			// If CLOD is enabled
			if (mCLOD)
			{
				// Check distance from the camera to the centre of the triangle
				auto distance = CheckNodeDistance(node, camera->mPos);

				// If distance is less than the nodes subdivide distance
				if (distance < node->mDistance)
				{
					// Subdivide the node
					ret = Subdivide(node, node->mLevel);
				}
				else if (distance > node->mDistance + mScale)
				{
					// Combine the node
					if (node->mLevel > 0) { node->mCombine = true; ret = true; }
				}
			}
			else
			{
				// Subdivide the nodes to the max LOD
				ret = Subdivide(node, node->mLevel);
			}		
		}
		else
		{
			// Check the next node
			ret = ret || CheckNodes(camera, node);
		}
	}

	return ret;
}

bool Planet::CombineNodes(Node* node)
{
	// If node should be combined
	if (node->mCombine)
	{
		// Delete this node and its neighbours 
		for (auto& subNode : node->mParent->mSubnodes)
		{
			if (subNode->mTriangleChunk)
			{
				subNode->mTriangleChunk->mCombine = true;
			}
		}

		// Clear the subnodes list
		node->mParent->mSubnodes.clear();
		node->mParent->mNumSubs = 0;
		return true;
	}
	else
	{
		// Check each subnode
		for (auto& subNode : node->mSubnodes)
		{
			CombineNodes(subNode);
		}
	}
	return false;
}

bool Planet::Update(Camera* camera, ID3D12GraphicsCommandList* commandList)
{
	// Sort nodes based on distance to camera
	SortBaseNodes(camera->mPos);
	mCurrentCommandList = commandList;

	// If a node has been updated
	if (CheckNodes(camera, mTriangleTree.get()))
	{
		// Check if any should be combined
		bool combine = false;
		for (auto& subNode : mTriangleTree->mSubnodes) { CombineNodes(subNode); }

		// If a chunk has been combined
		for (auto& chunk : mTriangleChunks)
		{
			if (chunk->mCombine)
				combine = true;
		}

		// Empty the command queue
		if (combine)
		{
			mGraphics->EmptyCommandQueue();			
		}

		// Delete the chunk that should be combined
		for (auto& chunk : mTriangleChunks)
		{
			if (chunk->mCombine) delete chunk;
		}

		// Build the indices for the planet to render
		BuildIndices();

		// Calculate dynamic buffer data and set mesh geometry
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

	// If greater than max LOD, do nothing and return false
	if (divLevel > mMaxLOD) return false;

	// If at max LOD, spawn a triangle chunk
	if (divLevel == mMaxLOD)
	{
		if (node->mTriangleChunk == nullptr)
		{
			node->mTriangleChunk = new TriangleChunk(
				mVertices[node->mTriangle.Point[0]],
				mVertices[node->mTriangle.Point[1]],
				mVertices[node->mTriangle.Point[2]],
				mFrequency, mOctaves, mNoise, mCurrentCommandList);
			mTriangleChunks.push_back(node->mTriangleChunk);
			return true;
		}
		return false;		
	}

	// Subdivide the node's triangle
	std::vector<Triangle> newTriangles = SubdivideTriangle(node->mTriangle);

	// Increment division level
	divLevel++;

	// Add triangles to quadtree
	for (auto& triangle : newTriangles)
	{
		node->AddSub(triangle);
		mTriangles.push_back(triangle);
		node->mNumSubs++;
	}

	// Set subdivision distances
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
	// Get list of distances
	float distArray[20];
	for (int i = 0; i < 20; i++)
	{
		distArray[i] = CheckNodeDistance(mTriangleTree->mSubnodes[i], cameraPos);
	}

	// Get list of indices to sort
	int subnodeIndices[20];
	for (int i = 0; i < 20; i++)
	{
		subnodeIndices[i] = i;
	}

	// Sort nodes
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

	// Get list of sorted nodes
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

	// Get indices from trinagles
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
	// Sample noise at position
	auto position = MulFloat3(vertex.Pos, { 200,200,200 });
	auto elevationValue = FractalBrownianMotion(noise, position, octaves, frequency);

	// Modify elevation slightly
	elevationValue *= 0.3;

	// Get distance from vertex to planet centre
	auto Radius = Distance(vertex.Pos, XMFLOAT3{ 0,0,0 });

	// Apply noise
	vertex.Pos.x *= 1 + (elevationValue / Radius);
	vertex.Pos.y *= 1 + (elevationValue / Radius);
	vertex.Pos.z *= 1 + (elevationValue / Radius);
}

float Planet::CheckNodeTriSize(Node* node, Camera* camera)
{
	// Get triangle positions
	auto A = mVertices[node->mTriangle.Point[0]].Pos;
	auto B = mVertices[node->mTriangle.Point[1]].Pos;
	auto C = mVertices[node->mTriangle.Point[2]].Pos;

	// Set up DirectX math
	auto vA = XMLoadFloat3(&A);
	auto vB = XMLoadFloat3(&B);
	auto vC = XMLoadFloat3(&C);
	auto mV = XMLoadFloat4x4(&camera->mViewMatrix);
	auto mP = XMLoadFloat4x4(&camera->mProjectionMatrix);

	// Multiply each point by the view matrix
	auto viewA = XMVector3Transform(vA, mV);
	auto viewB = XMVector3Transform(vB, mV);
	auto viewC = XMVector3Transform(vC, mV);

	// Multiply each point by the projection matrix
	auto viewProjA = XMVector3Transform(viewA, mP);
	auto viewProjB = XMVector3Transform(viewB, mP);
	auto viewProjC = XMVector3Transform(viewC, mP);

	// Store the new positions
	XMStoreFloat3(&A, viewProjA);
	XMStoreFloat3(&B, viewProjB);
	XMStoreFloat3(&C, viewProjC);

	// Measure the distance of a side and return
	auto distance = Distance(A, B);
	return distance;
}