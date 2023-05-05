#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include "FastNoiseLite.h"
#include <assimp/scene.h>
#include <vector>
#include <array>
//#include "FrameResource.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

// Make identity matrix
static XMFLOAT4X4 MakeIdentity4x4()
{
	XMFLOAT4X4 I(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	return I;
}

struct Triangle
{
	std::uint32_t Point[3];
};

// Light data
const static int mMaxLights = 16;
struct mLight
{
	XMFLOAT3 Colour = { 1.0f,1.0f,1.0f };
	float padding1 = 0.0f;
	XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;
	XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
	float FalloffEnd = 10.0f;
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
	float SpotPower = 64.0f;
};

// Vertex struct
struct Vertex
{
	XMFLOAT3 Pos = XMFLOAT3{ 0,0,0 };
	XMFLOAT4 Colour = XMFLOAT4{ 0,0,0,0 };
	XMFLOAT3 Normal = XMFLOAT3{ 0,0,0 };
	XMFLOAT2 UV = XMFLOAT2{ 0,0 };
	XMFLOAT3 Tangent = XMFLOAT3{ 0,0,0 };
};

// Buffer structs
struct PerMaterialConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	float Metallic = 0.0f;
	XMFLOAT3 padding;
	XMFLOAT4X4 MatTransform = MakeIdentity4x4();
};
struct PerObjectConstants
{
	XMFLOAT4X4 WorldMatrix;
	bool parallax;
	XMFLOAT3 padding;
};
struct PerFrameConstants
{
	XMFLOAT4X4 ViewMatrix = MakeIdentity4x4();
	XMFLOAT4X4 ProjMatrix = MakeIdentity4x4();
	XMFLOAT4X4 ViewProjMatrix = MakeIdentity4x4();
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float padding1 = 0.0f;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	float TexDebugIndex = 0.0f;
	float padding3 = 0.0f;
	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	mLight Lights[mMaxLights];
};

// Material struct
struct Material
{
	aiString AiName;
	wstring Name;

	int CBIndex = -1;
	int DiffuseSRVIndex = -1;
	int NumFramesDirty = 3;

	// Material constant buffer data used for shading.
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = .25f;
	float Metalness = 0.0f;
	XMFLOAT4X4 MatTransform = MakeIdentity4x4();
};

// Texture struct
struct Texture
{
	int ID;
	string Type;
	aiString AIPath;
	wstring Path;
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

// Helper function to create a bufferf
static ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer, 
													ComPtr<ID3D12Device> d3DDevice, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	ComPtr<ID3D12Resource> defaultBuffer;

	// Create the default buffer resource
	d3DDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf()));

	// Create intermediate upload heap to copy CPU data to buffer
	d3DDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf()));


	// Describe the data to copy
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	UpdateSubresources<1>(commandList.Get(), defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));


	return defaultBuffer;
}

static UINT CalculateConstantBufferSize(UINT size)
{
    // Round to nearest 256
	return (size + 255) & ~255;
}

static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi)
{
	return DirectX::XMVectorSet(
		radius * sinf(phi) * cosf(theta),
		radius * cosf(phi),
		radius * sinf(phi) * sinf(theta),
		1.0f);
}

// Distance between two points
static float Distance(XMFLOAT3 p1, XMFLOAT3 p2)
{
	auto x = (p1.x - p2.x) * (p1.x - p2.x);
	auto y = (p1.y - p2.y) * (p1.y - p2.y);
	auto z = (p1.z - p2.z) * (p1.z - p2.z);
	return std::sqrt(x + y + z);
}

// Add two float3 variables
static Vertex AddFloat3(XMFLOAT3 a, XMFLOAT3 b)
{
	Vertex result;

	result.Pos.x = a.x + b.x;
	result.Pos.y = a.y + b.y;
	result.Pos.z = a.z + b.z;

	return result;
}

// Sub two float3 variables
static XMFLOAT3 SubFloat3(XMFLOAT3 a, XMFLOAT3 b)
{
	XMFLOAT3 result;

	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;

	return result;
}

// Multiply two float3 variables
static XMFLOAT3 MulFloat3(XMFLOAT3 a, XMFLOAT3 b)
{
	XMFLOAT3 result;

	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;

	return result;
}

// Dot product between two float3s
static float DotProduct(XMFLOAT3 v1, XMFLOAT3 v2)
{
	auto x = v1.x * v2.x;
	auto y = v1.y * v2.y;
	auto z = v1.z * v2.z;
	auto result = x + y + z;
	return result;
}

// Normalise float3
static void Normalize(XMFLOAT3* p)
{
	float w = sqrt(p->x * p->x + p->y * p->y + p->z * p->z);
	p->x /= w;
	p->y /= w;
	p->z /= w;
}

// Cross product of two float3s
static XMFLOAT3 CrossProduct(XMFLOAT3 v1, XMFLOAT3 v2)
{
	XMFLOAT3 product = XMFLOAT3{ 0,0,0 };
	product.x = v1.y * v2.z - v1.z * v2.y;
	product.y = -(v1.x * v2.z - v1.z * v2.x);
	product.z = v1.x * v2.y - v1.y * v2.x;
	return product;
}

// Are two float3s the same
static bool Float3IsSame(XMFLOAT3 a, XMFLOAT3 b)
{
	if (a.x == b.x && a.y == b.y && a.z == b.z)
	{
		return true;
	}
	else
	{
		return false;
	}
}

// Midpoint between two float3s
static XMFLOAT3 Midpoint(XMFLOAT3 p1, XMFLOAT3 p2)
{
	XMFLOAT3 mid;
	mid.x = (p1.x + p2.x) / 2.0;
	mid.y = (p1.y + p2.y) / 2.0;
	mid.z = (p1.z + p2.z) / 2.0;
	return mid;
}

// Return centre of triangle
static XMFLOAT3 Center(XMFLOAT3 A, XMFLOAT3 B, XMFLOAT3 C)
{
	XMFLOAT3 center;
	center.x = (A.x + B.x + C.x) / 3.0;
	center.y = (A.y + B.y + C.y) / 3.0;
	center.z = (A.z + B.z + C.z) / 3.0;
	return center;
}

// Fractal brownian motion noise function
static float FractalBrownianMotion(FastNoiseLite* fastNoise, XMFLOAT3 fractalInput, float octaves, float frequency)
{
	float result = 0;
	float amplitude = 0.5;
	float lacunarity = 2.0;
	float gain = 0.5;

	// Add iterations of noise at different frequencies to get more detail from perlin noise
	for (int i = 0; i < octaves; i++)
	{
		result += amplitude * fastNoise->GetNoise(frequency * fractalInput.x, frequency * fractalInput.y, frequency * fractalInput.z);
		frequency *= lacunarity;
		amplitude *= gain;
	}

	return result;
}

// Calculate normals on an array of vertices and indices
static std::vector<XMFLOAT3> CalculateNormals(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
{
	std::vector<XMFLOAT3> normals;

	// Map of vertex to triangles in Triangles array
	int numVerts = vertices.size();
	std::vector<std::array<int32_t, 8>> VertToTriMap;
	for (int i = 0; i < numVerts; i++)
	{
		std::array<int32_t, 8> array{ -1,-1,-1,-1,-1,-1,-1,-1 };
		VertToTriMap.push_back(array);
	}

	// For each triangle for each vertex add triangle to vertex array entry
	for (int i = 0; i < indices.size(); i++)
	{
		for (int j = 0; j < 8; j++)
		{
			if (VertToTriMap[indices[i]][j] < 0)
			{
				VertToTriMap[indices[i]][j] = i / 3;
				break;
			}
		}
	}

	std::vector<XMFLOAT3> NTriangles;

	for (int i = 0; i < indices.size() / 3; i++)
	{
		XMFLOAT3 normal = {};
		NTriangles.push_back(normal);
	}

	int index = 0;
	for (int i = 0; i < NTriangles.size(); i++)
	{
		NTriangles[i].x = indices[index];
		NTriangles[i].y = indices[index + 1];
		NTriangles[i].z = indices[index + 2];
		index += 3;
	}

	for (int i = 0; i < vertices.size(); i++)
	{
		normals.push_back({ 0,0,0 });
	}

	// For each vertex collect the triangles that share it and calculate the face normal
	for (int i = 0; i < vertices.size(); i++)
	{
		for (auto& triangle : VertToTriMap[i])
		{
			// This shouldnt happen
			if (triangle < 0)
			{
				continue;
			}

			// Get vertices from triangle index
			auto A = vertices[NTriangles[triangle].x];
			auto B = vertices[NTriangles[triangle].y];
			auto C = vertices[NTriangles[triangle].z];

			// Calculate edges
			auto a = XMLoadFloat3(&A.Pos);
			auto b = XMLoadFloat3(&B.Pos);
			auto c = XMLoadFloat3(&C.Pos);

			auto E1 = XMVectorSubtract(a, c);
			auto E2 = XMVectorSubtract(b, c);

			// Calculate normal with cross product and normalise
			XMFLOAT3 Normal; XMStoreFloat3(&Normal, XMVector3Normalize(XMVector3Cross(E1, E2)));

			normals[i].x += Normal.x;
			normals[i].y += Normal.y;
			normals[i].z += Normal.z;
		}
	}

	// Average the face normals
	for (auto& normal : normals)
	{
		XMFLOAT3 normalizedNormal;
		XMStoreFloat3(&normalizedNormal, XMVector3Normalize(XMLoadFloat3(&normal)));
		normal = normalizedNormal;
	}
	return normals;
}