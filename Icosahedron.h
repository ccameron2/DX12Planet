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

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
	std::vector<Triangle> triangles;
	int mRecursions = 5;
	std::map<std::pair<int, int>, int> mVertexMap;
private:
	void CreateGeometry(int numVertices, int numIndices, ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList);
	int VertexForEdge(int first, int second);
	void CreateIcosphere();
	void SubdivideIcosphere();
};