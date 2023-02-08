#include "Utility.h"
#include <memory>
#include <vector>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include "Mesh.h"
#include <map>
#include <utility>

class Node;

using namespace DirectX;
using namespace std;
class Icosahedron
{


public:
	Icosahedron(float frequency, int recursions, int octaves, XMFLOAT3 eyePos, bool tesselation);
	~Icosahedron();

	XMFLOAT3 mEyePos;

	unique_ptr<Mesh> mMesh;
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	std::vector<Triangle> mTriangles;
	std::vector<Triangle> mNewTriangles;
	std::vector<XMFLOAT3> mNormals;
	int mRecursions = 2;
	int mMaxRecursions = 10;
	std::map<std::pair<int, int>, int> mVertexMap;
	int mOctaves = 8;
	float mFrequency = 1;
	std::vector<float> mCullAnglePerLevel;
	std::vector<float> mTriSizePerLevel;
	std::vector<float> mTriAnglePerLevel;
	int mScreenWidth = 800;
	std::vector<XMFLOAT2> mUVs;
	bool mTesselation = false;
	unique_ptr<Node> mTriangleTree;
	float mMaxScreenPercent;
	float mMaxPixelsPerTriangle = 5.0f;

	void CreateGeometry();
	void ResetGeometry(XMFLOAT3 eyePos, float frequency, int recursions, int octaves, bool tesselation);
private:
	int VertexForEdge(int first, int second);
	void SubdivideIcosphere(int level);
	float FractalBrownianMotion(FastNoiseLite fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency);
	void CalculateNormals();
	std::vector<Triangle> SubdivideTriangle(Triangle triangle);
	void CalculateUVs();
};