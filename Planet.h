#pragma once

#include "Utility.h"
#include "Mesh.h"
#include "Camera.h"
#include "TriangleChunk.h"
#include "Common.h"
#include "Graphics.h"

#include <DirectXColors.h>
#include <vector>
#include <memory>
#include <map>

#include "FastNoiseLite.h"

class Node
{
private:
public:
	Node* mParent;
	std::vector<Node*> mSubnodes;
	int mNumSubs = 0;
	Triangle mTriangle = {0,0,0};
	float mDistance = 0;
	int mLevel = 0;
	bool mCombine = false;
	TriangleChunk* mTriangleChunk = nullptr;
	Node() : mParent{ 0 } {};
	Node(Node* parent) : mParent{ parent } {};
	~Node()
	{
		if (mTriangleChunk) delete mTriangleChunk;
		for (auto& subNode : mSubnodes)
		{
			mParent = nullptr;
			delete subNode;
		}
		mSubnodes.clear();
	}
	Node* AddSub(Triangle triangle)
	{
		Node* newNode = new Node(this);
		newNode->mTriangle = triangle;
		mSubnodes.push_back(newNode);
		mNumSubs++;
		return newNode;
	};
};


class Planet
{
public:
	Planet(Graphics* graphics);
	~Planet();

	// Create a new planet
	void CreatePlanet(float frequency, int octaves, int lod, int scale, int seed);

	// Reset planet geometry to icosahedron
	void ResetGeometry();

	// Update planet
	bool Update(Camera* camera, ID3D12GraphicsCommandList* commandList);

	// Planet mesh
	Mesh* mMesh;

	// Max LOD to subdivide to
	int mMaxLOD = 0;

	// Scale of planet
	int mScale = 1;

	// Enable CLOD
	bool mCLOD = false;

	// List of chunks
	std::vector<TriangleChunk*> mTriangleChunks;
private:
	
	// Reference to the graphics class
	Graphics* mGraphics;
	ID3D12GraphicsCommandList* mCurrentCommandList;

	// Geometry
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	std::vector<Triangle> mTriangles;
	std::vector<XMFLOAT3> mNormals;
	std::map<std::pair<int, int>, int> mVertexMap;

	// Geometry Quadtree
	unique_ptr<Node> mTriangleTree;

	// Radius of planet
	float mRadius = 0.5f;
	float mMaxDistance = 0.0f;

	// Noise variables
	float mFrequency;
	int mOctaves;
	FastNoiseLite* mNoise;

	// Build indices to render the planet
	void BuildIndices();

	// Get triangles for indexing or chunks from maxLOD nodes
	void GetTriangles(Node* node);

	// Sort nodes by distance to the camera
	void SortBaseNodes(XMFLOAT3 cameraPos);

	// Check node distance to camera
	float CheckNodeDistance(Node* node, XMFLOAT3 cameraPos);

	// Subdivide node
	bool Subdivide(Node* node, int level = 0);

	// Get a vertex for triangle edge
	int GetVertexForEdge(int v1, int v2);
	
	// Subdivide triangle
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);

	// Check each node for updates
	bool CheckNodes(Camera* camera, Node* parentNode);

	// Combine nodes if flagged
	bool CombineNodes(Node* node);
	
	// Apply noise to the geometry
	void ApplyNoise(float frequency, int octaves, FastNoiseLite* noise, Vertex& vertex);

	// Get size of triangle on screen UNUSED
	float CheckNodeTriSize(Node* node, Camera* camera);
};

