#pragma once

#include "Utility.h"
#include "Mesh.h"

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
	std::vector<Node*> mChildren;
	Triangle mTriangle = {0,0,0};
	float mDistance = 0;
	int mLevel = 0;
	Node() : mParent{ 0 } {};
	Node(Node* parent) : mParent{ parent } {};
	~Node()
	{
		for (auto& node : mChildren)
		{
			mParent = nullptr;
			delete node;
		}
	}
	Node* AddChild(Triangle triangle)
	{
		Node* newNode = new Node(this);
		newNode->mTriangle = triangle;
		mChildren.push_back(newNode);
		return newNode;
	};
};


class Planet
{
public:
	Planet();
	~Planet();
	void CreatePlanet(float frequency, int octaves, int lod);
	void ResetGeometry();
	void Update(XMFLOAT3 cameraPos);

	Mesh* mMesh;	
	int mMaxLOD = 0;
	int mStartLOD = 2;
private:
	std::vector<Triangle> mTriangles;
	std::vector<XMFLOAT3> mNormals;
	unique_ptr<Node> mTriangleTree;
	std::map<std::pair<int, int>, int> mVertexMap;

	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;

	float mRadius = 1.0f;
	bool mFirstGen = true;
	void BuildIndices();

	float FractalBrownianMotion(FastNoiseLite fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency);

	void CalculateNormals();
	void Subdivide(Node* node, int level = 0);
	int GetVertexForEdge(int v1, int v2);
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);
	void GetTriangles(Node* node);
	bool CheckNodes(XMFLOAT3 cameraPos, Node* parentNode);
	void ApplyNoise(float frequency, int octaves);
};

