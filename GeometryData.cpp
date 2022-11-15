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
	indicesCount = mIndices.size();
	const UINT vBSize = (UINT)mVertices.size() * sizeof(Vertex);
	const UINT iBSize = (UINT)mIndices.size() * sizeof(std::uint16_t);

	D3DCreateBlob(vBSize, &CPUVertexBuffer);
	CopyMemory(CPUVertexBuffer->GetBufferPointer(), mVertices.data(), vBSize);

	D3DCreateBlob(iBSize, &CPUIndexBuffer);
	CopyMemory(CPUIndexBuffer->GetBufferPointer(), mIndices.data(), iBSize);

	GPUVertexBuffer = CreateDefaultBuffer(mVertices.data(), vBSize, vertexBufferUploader, d3DDevice, commandList);

	GPUIndexBuffer = CreateDefaultBuffer(mIndices.data(), iBSize, indexBufferUploader, d3DDevice, commandList);

	vertexByteStride = sizeof(Vertex);
	vertexBufferByteSize = vBSize;
	indexFormat = DXGI_FORMAT_R16_UINT;
	indexBufferByteSize = iBSize;
}
