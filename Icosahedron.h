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
		std::uint32_t Point[3];
	};

public:
	Icosahedron(float frequency, int recursions, int octaves, XMFLOAT3 eyePos);
	~Icosahedron();

	XMFLOAT3 mEyePos;

	unique_ptr<GeometryData> mGeometryData;
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	std::vector<Triangle> mTriangles;
	std::vector<Triangle> mNewTriangles;
	std::vector<XMFLOAT3> mNormals;
	int mRecursions = 6;
	int mMaxRecursions = 20;
	std::map<std::pair<int, int>, int> mVertexMap;
	int mOctaves = 8;
	float mFrequency = 1;
	std::vector<float> mCullAnglePerLevel;
	std::vector<float> mTriSizePerLevel;
	std::vector<float> mTriAnglePerLevel;
	int mScreenWidth = 800;

	float mMaxScreenPercent;
	float mMaxPixelsPerTriangle = 5.0f;

	void CreateGeometry();
	void ResetGeometry(XMFLOAT3 eyePos, float frequency, int recursions, int octaves);
private:
	int VertexForEdge(int first, int second);
	void SubdivideIcosphere(int level);
	float FractalBrownianMotion(FastNoiseLite fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency);
	void CalculateNormals();
	void SubdivideTriangle(Triangle triangle);
};