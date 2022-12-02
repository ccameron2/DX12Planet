#include "GeometryData.h"

D3D12_VERTEX_BUFFER_VIEW GeometryData::GetVertexBufferView()
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = mGPUVertexBuffer->GetGPUVirtualAddress();
	vbv.StrideInBytes = mVertexByteStride;
	vbv.SizeInBytes = mVertexBufferByteSize;
	return vbv;
}
D3D12_INDEX_BUFFER_VIEW GeometryData::GetIndexBufferView()
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = mGPUIndexBuffer->GetGPUVirtualAddress();
	ibv.Format = mIndexFormat;
	ibv.SizeInBytes = mIndexBufferByteSize;
	return ibv;
}
void GeometryData::EmptyUploaders()
{
	mVertexBufferUploader = nullptr;
	mIndexBufferUploader = nullptr;
}

void GeometryData::CalculateDynamicBufferData()
{
	UINT vbByteSize = mVertices.size() * sizeof(Vertex);
	UINT ibByteSize = (UINT)mIndices.size() * sizeof(std::uint32_t);

	mCPUVertexBuffer = nullptr;
	mGPUVertexBuffer = nullptr;

	mVertexByteStride = sizeof(Vertex);
	mVertexBufferByteSize = vbByteSize;
	mIndexBufferByteSize = ibByteSize;

}

void GeometryData::CalculateBufferData(ID3D12Device* d3DDevice, ID3D12GraphicsCommandList* commandList)
{
	mIndicesCount = mIndices.size();
	const UINT vBSize = (UINT)mVertices.size() * sizeof(Vertex);
	const UINT iBSize = (UINT)mIndices.size() * sizeof(std::uint32_t);

	D3DCreateBlob(vBSize, &mCPUVertexBuffer);
	CopyMemory(mCPUVertexBuffer->GetBufferPointer(), mVertices.data(), vBSize);

	D3DCreateBlob(iBSize, &mCPUIndexBuffer);
	CopyMemory(mCPUIndexBuffer->GetBufferPointer(), mIndices.data(), iBSize);

	mGPUVertexBuffer = CreateDefaultBuffer(mVertices.data(), vBSize, mVertexBufferUploader, d3DDevice, commandList);

	mGPUIndexBuffer = CreateDefaultBuffer(mIndices.data(), iBSize, mIndexBufferUploader, d3DDevice, commandList);

	mVertexByteStride = sizeof(Vertex);
	mVertexBufferByteSize = vBSize;
	mIndexBufferByteSize = iBSize;
}
