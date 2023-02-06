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
	Triangle mTriangle;
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
	void Update();
	void Subdivide(Node* node, int level = 0);
	int GetVertexForEdge(int v1, int v2);
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);
	void GetTriangles(Node* node);
	unique_ptr<Mesh> mGeometryData;
	void ApplyNoise(float frequency, int octaves);
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	int mMaxLOD = 0;
private:
	std::vector<Triangle> mTriangles;
	std::vector<XMFLOAT3> mNormals;
	unique_ptr<Node> mTriangleTree;
	std::map<std::pair<int, int>, int> mVertexMap;

	void BuildIndices();

	float FractalBrownianMotion(FastNoiseLite fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency);

	void CalculateNormals();

};

