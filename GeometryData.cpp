#include "GeometryData.h"

D3D12_VERTEX_BUFFER_VIEW GeometryData::GetVertexBufferView()
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = GPUVertexBuffer->GetGPUVirtualAddress();
	vbv.StrideInBytes = vertexByteStride;
	vbv.SizeInBytes = vertexBufferByteSize;
	return vbv;
}
D3D12_INDEX_BUFFER_VIEW GeometryData::GetIndexBufferView()
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = GPUIndexBuffer->GetGPUVirtualAddress();
	ibv.Format = indexFormat;
	ibv.SizeInBytes = indexBufferByteSize;
	return ibv;
}
void GeometryData::EmptyUploaders()
{
	vertexBufferUploader = nullptr;
	indexBufferUploader = nullptr;
}

void GeometryData::CalculateBufferData(ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList)
{
	indicesCount = indices.size();
	const UINT vbSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibSize = (UINT)indices.size() * sizeof(std::uint16_t);


	D3DCreateBlob(vbSize, &CPUVertexBuffer);
	CopyMemory(CPUVertexBuffer->GetBufferPointer(), vertices.data(), vbSize);

	D3DCreateBlob(ibSize, &CPUIndexBuffer);
	CopyMemory(CPUIndexBuffer->GetBufferPointer(), indices.data(), ibSize);

	GPUVertexBuffer = CreateDefaultBuffer(vertices.data(), vbSize, vertexBufferUploader, d3DDevice, commandList);

	GPUIndexBuffer = CreateDefaultBuffer(indices.data(), ibSize, indexBufferUploader, d3DDevice, commandList);

	vertexByteStride = sizeof(Vertex);
	vertexBufferByteSize = vbSize;
	indexFormat = DXGI_FORMAT_R16_UINT;
	indexBufferByteSize = ibSize;
}
