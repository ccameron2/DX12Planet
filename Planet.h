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
	void CreatePlanet(float frequency, int octaves, int lod, int scale);
	void ResetGeometry();
	bool Update(Camera* camera, ID3D12GraphicsCommandList* commandList);

	Mesh* mMesh;	
	int mMaxLOD = 0;
	int mScale = 1;
	bool mCLOD = false;
	std::vector<TriangleChunk*> mTriangleChunks;
private:
	Graphics* mGraphics;
	ID3D12GraphicsCommandList* mCurrentCommandList;
	std::vector<Triangle> mTriangles;
	std::vector<XMFLOAT3> mNormals;
	unique_ptr<Node> mTriangleTree;
	std::map<std::pair<int, int>, int> mVertexMap;
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	float mRadius = 0.5f;
	float mMaxDistance = 0.0f;
	float mFrequency;
	int mOctaves;
	int mCombineLOD = mMaxLOD;
	FastNoiseLite* mNoise;
	void BuildIndices();
	void SortBaseNodes(XMFLOAT3 cameraPos);
	float CheckNodeDistance(Node* node, XMFLOAT3 cameraPos);
	bool Subdivide(Node* node, int level = 0);
	int GetVertexForEdge(int v1, int v2);
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);
	void GetTriangles(Node* node);
	bool CheckNodes(Camera* camera, Node* parentNode);
	bool CombineNodes(Node* node);
	void ApplyNoise(float frequency, int octaves, FastNoiseLite* noise, Vertex& vertex);
	float CheckNodeTriSize(Node* node, Camera* camera);

	// Threading




};

