#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include "FastNoiseLite.h"
#include <assimp/scene.h>
//#include "FrameResource.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

//extern std::vector<std::unique_ptr<FrameResource>> mFrameResources;

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

struct Vertex
{
	XMFLOAT3 Pos = XMFLOAT3{ 0,0,0 };
	XMFLOAT4 Colour = XMFLOAT4{ 0,0,0,0 };
	XMFLOAT3 Normal = XMFLOAT3{ 0,0,0 };
	XMFLOAT2 UV = XMFLOAT2{ 0,0 };
	XMFLOAT3 Tangent = XMFLOAT3{ 0,0,0 };
};

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
	float padding2 = 0.0f;
	float padding3 = 0.0f;
	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	mLight Lights[mMaxLights];
};

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

struct Texture
{
	int ID;
	string Type;
	aiString AIPath;
	wstring Path;
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

static ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer, 
													ComPtr<ID3D12Device> d3DDevice, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	ComPtr<ID3D12Resource> defaultBuffer;

	// Create the default buffer resource.
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

	// Schedule to copy the data to the default buffer resource.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	UpdateSubresources<1>(commandList.Get(), defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.

	return defaultBuffer;
}

static UINT CalculateConstantBufferSize(UINT size)
{
	// Constant buffers must be a multiple of 256.  
	// So round up to nearest multiple of 256.
	// We do this by adding 255 and then masking off
	// the lower 2 bytes which store all bits < 256.
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

static float Distance(XMFLOAT3 p1, XMFLOAT3 p2)
{
	auto x = (p1.x - p2.x) * (p1.x - p2.x);
	auto y = (p1.y - p2.y) * (p1.y - p2.y);
	auto z = (p1.z - p2.z) * (p1.z - p2.z);
	return std::sqrt(x + y + z);
}

static Vertex AddFloat3(XMFLOAT3 a, XMFLOAT3 b)
{
	Vertex result;

	result.Pos.x = a.x + b.x;
	result.Pos.y = a.y + b.y;
	result.Pos.z = a.z + b.z;

	return result;
}

static XMFLOAT3 SubFloat3(XMFLOAT3 a, XMFLOAT3 b)
{
	XMFLOAT3 result;

	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;

	return result;
}

static float DotProduct(XMFLOAT3 v1, XMFLOAT3 v2)
{
	auto x = v1.x * v2.x;
	auto y = v1.y * v2.y;
	auto z = v1.z * v2.z;
	auto result = x + y + z;
	return result;
}

static void Normalize(XMFLOAT3* p)
{
	float w = sqrt(p->x * p->x + p->y * p->y + p->z * p->z);
	p->x /= w;
	p->y /= w;
	p->z /= w;
}

static XMFLOAT3 CrossProduct(XMFLOAT3 v1, XMFLOAT3 v2)
{
	XMFLOAT3 product = XMFLOAT3{ 0,0,0 };
	product.x = v1.y * v2.z - v1.z * v2.y;
	product.y = -(v1.x * v2.z - v1.z * v2.x);
	product.z = v1.x * v2.y - v1.y * v2.x;
	return product;
}

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

