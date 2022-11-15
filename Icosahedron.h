#include "Utility.h"
#include <memory>
#include <vector>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include "GeometryData.h"
#include <map>
#include <utility>

using namespace DirectX;
using namespace std;
class Icosahedron
{
	struct Triangle
	{
		int Point[3];
	};

public:
	Icosahedron(int numVertices, int numIndices, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList);
	~Icosahedron();
	unique_ptr<GeometryData> mGeometryData;

	std::vector<Vertex> mVertices;
	std::vector<uint16_t> mIndices;
	std::vector<Triangle> mTriangles;
	int mRecursions = 6;
	std::map<std::pair<int, int>, int> mVertexMap;
	int mOctaves = 8;
	int mFrequency = 0.8;
private:
	void CreateGeometry(int numVertices, int numIndices, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList);
	int VertexForEdge(int first, int second);
	void SubdivideIcosphere();
	float FractalBrownianMotion(XMFLOAT3 fractalInput, float octaves, float frequency);
};