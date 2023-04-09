#pragma once

#include "Utility.h"
#include "Mesh.h"
#include "Camera.h"
#include "TriangleChunk.h"

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
	Planet(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	~Planet();
	void CreatePlanet(float frequency, int octaves, int lod);
	void ResetGeometry();
	bool Update(Camera* camera);

	Mesh* mMesh;	
	int mMaxLOD = 0;
	std::vector<TriangleChunk*> mTriangleChunks;
private:
	ID3D12Device* mD3DDevice;
	ID3D12GraphicsCommandList* mCommandList;

	std::vector<Triangle> mTriangles;
	std::vector<XMFLOAT3> mNormals;
	unique_ptr<Node> mTriangleTree;
	std::map<std::pair<int, int>, int> mVertexMap;
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	float mRadius = 1.0f;
	float mMaxDistance = mRadius * 8;
	float mFrequency;
	int mOctaves;
	FastNoiseLite* mNoise;
	void BuildIndices();

	float FractalBrownianMotion(FastNoiseLite fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency);

	void CalculateNormals();
	bool Subdivide(Node* node, int level = 0);
	int GetVertexForEdge(int v1, int v2);
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);
	void GetTriangles(Node* node);
	bool CheckNodes(XMFLOAT3 cameraPos, Node* parentNode);
	bool CombineNodes(Node* node);
	void ApplyNoise(float frequency, int octaves);
};

