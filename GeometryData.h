#pragma once
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include "Utility.h"
#include <vector>
#include <array>
#include <D3DCompiler.h>

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;
class GeometryData
{
public:
	// Vertex and index buffers on CPU side
	ComPtr<ID3DBlob> CPUVertexBuffer = nullptr;
	ComPtr<ID3DBlob> CPUIndexBuffer = nullptr;

	// Vertex and index buffers on GPU side
	ComPtr<ID3D12Resource> GPUVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> GPUIndexBuffer = nullptr;

	// Vertex and index buffer uploaders
	ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

	// Data about buffers.
	UINT vertexByteStride = 0;
	UINT vertexBufferByteSize = 0;
	DXGI_FORMAT indexFormat = DXGI_FORMAT_R16_UINT;
	UINT indexBufferByteSize = 0;
	int  indicesCount = 0;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	void EmptyUploaders();
	void CalculateBufferData(ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList);
};
