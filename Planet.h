#pragma once

#include "Utility.h"
#include "GeometryData.h"

#include <DirectXColors.h>
#include <vector>
#include <memory>
#include <map>

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
	void AddChild(Triangle triangle)
	{
		Node* newNode = new Node(this);
		newNode->mTriangle = triangle;
		mChildren.push_back(newNode);
	};
};


class Planet
{
public:
	Planet(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	~Planet();
	void CreateIcosahedron();
	void Update();
	void Subdivide(Node* node, int level = 0);
	int GetVertexForEdge(int v1, int v2);
	int mMaxLOD = 8;
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);
	void GetTriangles(Node* node);
	unique_ptr<GeometryData> mGeometryData;
private:
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	std::vector<Triangle> mTriangles;
	unique_ptr<Node> mTriangleTree;
	std::map<std::pair<int, int>, int> mVertexMap;

};

