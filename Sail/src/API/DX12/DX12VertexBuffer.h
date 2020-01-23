#pragma once
#include "Sail/api/VertexBuffer.h"
#include "DX12API.h"

class DX12VertexBuffer : public VertexBuffer {
public:
	DX12VertexBuffer(const InputLayout& inputLayout, Mesh::Data& modelData);
	~DX12VertexBuffer();

	virtual void bind(void* cmdList) const override;

private:
	wComPtr<ID3D12Resource> m_vertexBuffer;

};