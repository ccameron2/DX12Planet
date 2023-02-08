#include "Icosahedron.h"
#include "FastNoiseLite.h"
#include <execution>
#include <cmath>

class Node
{
private:
public:
	Node* mParent;
	std::vector<Node*> mChildren;
	Triangle mTriangle;
	Node() : mParent{ 0 } {};
	Node(Node* parent) : mParent{ parent }{};
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




XMVECTOR ComputeNormal(XMVECTOR p0, XMVECTOR p1, XMVECTOR p2)
{
	XMVECTOR u = p1 - p0;
	XMVECTOR v = p2 - p0;
	return XMVector3Normalize(XMVector3Cross(u, v));
}


Icosahedron::Icosahedron(float frequency, int recursions, int octaves, XMFLOAT3 eyePos, bool tesselation)
{
	mMesh = std::make_unique<Mesh>();

	ResetGeometry(eyePos,frequency,recursions, octaves, tesselation);

	// Precalculate angle to stop subdivision
	mCullAnglePerLevel.push_back(0.5);
	float angle = std::acos(mCullAnglePerLevel[0]);
	for (int i = 0; i < mMaxRecursions; i++)
	{
		angle = angle / 2;
		mCullAnglePerLevel.push_back(sin(angle));
	}

	// Precalculate size per level
	mTriSizePerLevel.clear();
	mTriSizePerLevel.push_back(Distance(mVertices[3].Pos, mVertices[1].Pos));
	for (int i = 1; i < mMaxRecursions; i++)
	{
		mTriSizePerLevel.push_back(mTriSizePerLevel[i - 1] / 2);
	}

	// Precalculate max screen percentage
	mMaxScreenPercent = mMaxPixelsPerTriangle / mScreenWidth;

	//for (int i = 1; i < mMaxRecursions; i++) 
	//{
	//	mTriAnglePerLevel.push_back(atan(mTriSizePerLevel[i] / (Distance(XMFLOAT3{0,0,0}, mEyePos))));
	//}

	CreateGeometry();
}

Icosahedron::~Icosahedron()
{
}

void Icosahedron::CreateGeometry()
{
	for (int i = 0; i < mRecursions; i++)
	{
		SubdivideIcosphere(i);
	}

	FastNoiseLite noise;
	noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	for (auto & vertex : mVertices)
	{
		XMVECTOR pos = XMLoadFloat3(&vertex.Pos);
		pos = XMVectorMultiply(pos, { 100,100,100 });
		XMFLOAT3 position; XMStoreFloat3(&position, pos);
		auto ElevationValue = 1 + FractalBrownianMotion(noise, position, mOctaves, mFrequency);
		//auto ElevationValue = 1 + noise.GetNoise(0.5 * vertex.Pos.x * 100, 0.5 * vertex.Pos.y * 100, 0.5 * vertex.Pos.z * 100);
		ElevationValue *= 1.5;
		auto Radius = Distance(vertex.Pos, XMFLOAT3{ 0,0,0 });
		vertex.Pos.x *= 1 + (ElevationValue / Radius);
		vertex.Pos.y *= 1 + (ElevationValue / Radius);
		vertex.Pos.z *= 1 + (ElevationValue / Radius);
	}

	mIndices.clear();

	for (int i = 0; i < mTriangles.size(); i++)
	{
		mIndices.push_back(mTriangles[i].Point[0]);
		mIndices.push_back(mTriangles[i].Point[1]);
		mIndices.push_back(mTriangles[i].Point[2]);
	}

	mNormals.clear();
	CalculateNormals();

	CalculateUVs();

	mMesh->mVertices = mVertices;
	mMesh->mIndices = mIndices;
	
	//mGeometryData->CalculateBufferData(d3DDevice,commandList);
	mMesh->CalculateDynamicBufferData();
}

void Icosahedron::CalculateUVs()
{
	mUVs.resize(mVertices.size());
	for (int i = 0; i < mVertices.size() / 3; i++)
	{
		mUVs[i].x = atan2(mVertices[i].Pos.z,mVertices[i].Pos.x) / (2.0f * XM_PI);
		mUVs[i].y = asin(mVertices[i].Pos.y / (XM_PI) + 0.5f);
	}
}

void Icosahedron::ResetGeometry(XMFLOAT3 eyePos, float frequency, int recursions, int octaves, bool tesselation)
{
	mEyePos = eyePos;
	mVertices.clear();
	mIndices.clear();
	mTriangles.clear();
	mNormals.clear();
	mRecursions = recursions;
	mFrequency = frequency;
	mOctaves = octaves;
	mTesselation = tesselation;

	const float X = 0.525731112119133606f;
	const float Z = 0.850650808352039932f;
	const float N = 0.0f;

	mVertices =
	{
		Vertex({ XMFLOAT3(-X,N,Z), XMFLOAT4(Colors::Red)}),
		Vertex({ XMFLOAT3(X,N,Z), XMFLOAT4(Colors::Orange)}),
		Vertex({ XMFLOAT3(-X,N,-Z), XMFLOAT4(Colors::Yellow)}),
		Vertex({ XMFLOAT3(X,N,-Z), XMFLOAT4(Colors::Green)}),
		Vertex({ XMFLOAT3(N,Z,X), XMFLOAT4(Colors::Blue)}),
		Vertex({ XMFLOAT3(N,Z,-X), XMFLOAT4(Colors::Indigo)}),
		Vertex({ XMFLOAT3(N,-Z,X), XMFLOAT4(Colors::Violet)}),
		Vertex({ XMFLOAT3(N,-Z,-X), XMFLOAT4(Colors::Magenta)}),
		Vertex({ XMFLOAT3(Z,X,N), XMFLOAT4(Colors::Cyan)}),
		Vertex({ XMFLOAT3(-Z,X,N), XMFLOAT4(Colors::Gold)}),
		Vertex({ XMFLOAT3(Z,-X,N), XMFLOAT4(Colors::Pink)}),
		Vertex({ XMFLOAT3(-Z,-X,N), XMFLOAT4(Colors::Silver)})
	};

	mIndices =
	{
		1,4,0,	4,9,0,	4,5,9,
		8,5,4,	1,8,4,	1,10,8,
		10,3,8, 8,3,5,	3,2,5,
		3,7,2,	3,10,7,	10,6,7,
		6,11,7,	6,0,11,	6,1,0,
		10,1,6,	11,0,9,	2,11,9,
		5,2,9,	11,2,7
	};

	// Build triangles
	for (int i = 0; i < mIndices.size(); i += 3)
	{
		mTriangles.push_back(Triangle{ mIndices[i],mIndices[i + 1],mIndices[i + 2] });
	}

}

int Icosahedron::VertexForEdge(int p1, int p2)
{
	// Create pair to use as key in map and normalise edge direction to prevent duplication
	std::pair<int, int> vertexPair(p1, p2);
	if (vertexPair.first > vertexPair.second) { std::swap(vertexPair.first, vertexPair.second); }

	// Either create or reuse vertices
	auto in = mVertexMap.insert({ vertexPair, mVertices.size() });
	if (in.second)
	{
		auto& edge1 = mVertices[p2];
		auto& edge2 = mVertices[p1];
		auto point = AddFloat3(edge1.Pos,edge2.Pos);
		Normalize(&point.Pos);
		point.Colour.x = std::lerp(edge1.Colour.x, edge2.Colour.x, 0.5);
		point.Colour.y = std::lerp(edge1.Colour.y, edge2.Colour.y, 0.5);
		point.Colour.z = std::lerp(edge1.Colour.z, edge2.Colour.z, 0.5);
		mVertices.push_back(point);
	}
	return in.first->second;
}

std::vector<Triangle> Icosahedron::SubdivideTriangle(Triangle triangle)
{
	std::vector<Triangle> newTriangles;
	// For each edge
	std::uint32_t mid[3];
	for (int e = 0; e < 3; e++)
	{
		mid[e] = VertexForEdge(triangle.Point[e], triangle.Point[(e + 1) % 3]);
	}

	// Add triangles to new array
	mNewTriangles.push_back({ triangle.Point[0], mid[0], mid[2] });
	mNewTriangles.push_back({ triangle.Point[1], mid[1], mid[0] });
	mNewTriangles.push_back({ triangle.Point[2], mid[2], mid[1] });
	mNewTriangles.push_back({ mid[0], mid[1], mid[2] });

	newTriangles.push_back({ triangle.Point[0], mid[0], mid[2] });
	newTriangles.push_back({ triangle.Point[1], mid[1], mid[0] });
	newTriangles.push_back({ triangle.Point[2], mid[2], mid[1] });
	newTriangles.push_back({ mid[0], mid[1], mid[2] });

	return newTriangles;
}

void Icosahedron::SubdivideIcosphere(int level)
{
	for (auto& triangle : mTriangles)
	{
		// Dot product between camera and triangle face normal
		XMFLOAT3 a = mVertices[triangle.Point[0]].Pos;
		XMFLOAT3 b = mVertices[triangle.Point[1]].Pos;
		XMFLOAT3 c = mVertices[triangle.Point[2]].Pos;
		XMFLOAT3 centre = { (a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3 };
		
		Normalize(&centre);
		auto directionToCamera = SubFloat3(centre, mEyePos);
		Normalize(&directionToCamera);

		auto dot = DotProduct(centre,directionToCamera);
		
		if (mTesselation)
		{
			// Dont subdivide rear facing triangles
			if (dot < mCullAnglePerLevel[level])
			{
				//SubdivideTriangle(triangle);
				auto angleSize = atan(mTriSizePerLevel[level] / (Distance(centre, mEyePos) * 2));//= mTriAnglePerLevel[level];
				if (angleSize / (0.25f * XM_PI) > mMaxScreenPercent)
				{
					SubdivideTriangle(triangle);
				}
				else
				{
					mNewTriangles.push_back(triangle);
				}
			}
			else
			{
				mNewTriangles.push_back(triangle);
			}
		}
		else
		{
			SubdivideTriangle(triangle);
		}	
	}

	// Swap old triangles with new ones
	mTriangles.swap(mNewTriangles);

	mNewTriangles.clear();
	
	// Clear the vertex map for re-use
	mVertexMap.clear();
}

float Icosahedron::FractalBrownianMotion(FastNoiseLite fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency)
{
	float result = 0;
	float amplitude = 0.5;
	float lacunarity = 2.0;
	float gain = 0.5;

	// Add iterations of noise at different frequencies to get more detail from perlin noise
	for (int i = 0; i < octaves; i++)
	{
		result += amplitude * fastNoise.GetNoise(frequency * fractalInput.x, frequency * fractalInput.y, frequency * fractalInput.z);
		frequency *= lacunarity;
		amplitude *= gain;
	}

	return result;
}

void Icosahedron::CalculateNormals()
{
	// Map of vertex to triangles in Triangles array
	int numVerts = mVertices.size();
	std::vector<std::array<int32_t,8>> VertToTriMap;
	for (int i = 0; i < numVerts; i++)
	{
		std::array<int32_t, 8> array{-1,-1,-1,-1,-1,-1,-1,-1};
		VertToTriMap.push_back(array);
	}

	// For each triangle for each vertex add triangle to vertex array entry
	for (int i = 0; i < mIndices.size(); i++)
	{
		for (int j = 0; j < 8; j++)
		{
			if (VertToTriMap[mIndices[i]][j] < 0)
			{
				VertToTriMap[mIndices[i]][j] = i / 3;
				break;
			}
		}
	}

	std::vector<XMFLOAT3> NTriangles;

	for (int i = 0; i < mIndices.size() / 3; i++)
	{
		XMFLOAT3 normal = {};
		NTriangles.push_back(normal);
	}

	int index = 0;
	for (int i = 0; i < NTriangles.size(); i++)
	{
		NTriangles[i].x = mIndices[index];
		NTriangles[i].y = mIndices[index + 1];
		NTriangles[i].z = mIndices[index + 2];
		index += 3;
	}

	for (int i = 0; i < mVertices.size(); i++)
	{
		mNormals.push_back({ 0,0,0 });
	}

	// For each vertex collect the triangles that share it and calculate the face normal
	for (int i = 0; i < mVertices.size(); i++)
	{
		for (auto& triangle : VertToTriMap[i])
		{
			// This shouldnt happen
			if (triangle < 0)
			{
				continue;
			}

			// Get vertices from triangle index
			auto A = mVertices[NTriangles[triangle].x];
			auto B = mVertices[NTriangles[triangle].y];
			auto C = mVertices[NTriangles[triangle].z];

			// Calculate edges
			auto a = XMLoadFloat3(&A.Pos);
			auto b = XMLoadFloat3(&B.Pos);
			auto c = XMLoadFloat3(&C.Pos);

			auto E1 = XMVectorSubtract(a, c);
			auto E2 = XMVectorSubtract(b, c);
			
			// Calculate normal with cross product and normalise
			XMFLOAT3 Normal; XMStoreFloat3(&Normal, XMVector3Normalize(XMVector3Cross(E1, E2)));

			mNormals[i].x += Normal.x;
			mNormals[i].y += Normal.y;
			mNormals[i].z += Normal.z;
		}
	}

	// Average the face normals
	for (auto& normal : mNormals)
	{
		XMFLOAT3 normalizedNormal;
		XMStoreFloat3(&normalizedNormal, XMVector3Normalize(XMLoadFloat3(&normal)));
		normal = normalizedNormal;
	}

	for (int i = 0; i < mVertices.size(); i++)
	{
		mVertices[i].Normal = mNormals[i];
	}

}
