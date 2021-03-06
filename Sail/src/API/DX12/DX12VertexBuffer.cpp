#include "pch.h"
#include "DX12VertexBuffer.h"
#include "Sail/Application.h"
#include "DX12Utils.h"

VertexBuffer* VertexBuffer::Create(const InputLayout& inputLayout, const Mesh::Data& modelData, bool allowUpdates) {
	return SAIL_NEW DX12VertexBuffer(inputLayout, modelData, allowUpdates);
}

// When allowUpdates is enabled, there will exist one vertex buffer for each GPUBuffer,
// otherwise there will only be one buffer since it is not allowed to change between frames
DX12VertexBuffer::DX12VertexBuffer(const InputLayout& inputLayout, const Mesh::Data& modelData, bool allowUpdates)
	: VertexBuffer(inputLayout, modelData.numVertices)
	, m_allowUpdates(allowUpdates)
{
	SAIL_PROFILE_API_SPECIFIC_FUNCTION();

	void* vertices = getVertexData(modelData);
	
	m_context = Application::getInstance()->getAPI<DX12API>();
	auto numSwapBuffers = (m_allowUpdates) ? m_context->getNumGPUBuffers() : 1;

	m_initFrameCount = 0;
	m_hasBeenUpdated.resize(numSwapBuffers, false);
	m_hasBeenInitialized.resize(numSwapBuffers, false);
	m_uploadVertexBuffers.resize(numSwapBuffers);
	m_defaultVertexBuffers.resize(numSwapBuffers);

	m_byteSize = getVertexDataSize();

	for (unsigned int i = 0; i < numSwapBuffers; i++) {
		m_uploadVertexBuffers[i].Attach(DX12Utils::CreateBuffer(m_context->getDevice(), m_byteSize,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ, DX12Utils::sUploadHeapProperties));
		m_uploadVertexBuffers[i]->SetName(L"Vertex buffer upload");

		// Place vertices in the buffer
		void* pData;
		D3D12_RANGE readRange{ 0, 0 };
		ThrowIfFailed(m_uploadVertexBuffers[i]->Map(0, &readRange, &pData));
		memcpy(pData, vertices, m_byteSize);
		m_uploadVertexBuffers[i]->Unmap(0, nullptr);

		// Create the default buffers that the data will be copied to during init(cmdList)
		m_defaultVertexBuffers[i].Attach(DX12Utils::CreateBuffer(m_context->getDevice(), m_byteSize,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, // TODO: only allow UAV on animated vertex buffers
			D3D12_RESOURCE_STATE_COPY_DEST, DX12Utils::sDefaultHeapProps));
		m_defaultVertexBuffers[i]->SetName(L"Vertex buffer default");
	}
	// Delete vertices from cpu memory
	free(vertices);
}

DX12VertexBuffer::~DX12VertexBuffer() {

}

void DX12VertexBuffer::bind(void* cmdList) {
	SAIL_PROFILE_API_SPECIFIC_FUNCTION();

	auto frameIndex = (m_allowUpdates) ? m_context->getSwapIndex() : 0;
	auto* dxCmdList = static_cast<ID3D12GraphicsCommandList4*>(cmdList);
	// Make sure it has been initialized
	init(dxCmdList);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = m_defaultVertexBuffers[frameIndex]->GetGPUVirtualAddress();
	vbView.SizeInBytes = static_cast<UINT>(getVertexDataSize());
	vbView.StrideInBytes = static_cast<UINT>(getVertexDataStride());
	
	dxCmdList->IASetVertexBuffers(0, 1, &vbView);
}

void DX12VertexBuffer::update(Mesh::Data& data) {
	SAIL_PROFILE_API_SPECIFIC_FUNCTION();

	if (!m_allowUpdates) {
		assert(false);
		Logger::Error("update() was called on a VertexBuffer what was created without the allowUpdate flag!");
		return;
	}

	auto frameIndex = m_context->getSwapIndex();
	void* vertices = getVertexData(data);
	// Place vertices in the buffer
	void* pData;
	D3D12_RANGE readRange{ 0, 0 };
	ThrowIfFailed(m_uploadVertexBuffers[frameIndex]->Map(0, &readRange, &pData));
	memcpy(pData, vertices, getVertexDataSize());
	m_uploadVertexBuffers[frameIndex]->Unmap(0, nullptr);
	free(vertices);

	m_hasBeenUpdated[frameIndex] = true;
	m_hasBeenInitialized[frameIndex] = false;
}

void DX12VertexBuffer::setAsUpdated() {
	auto frameIndex = (m_allowUpdates) ? m_context->getSwapIndex() : 0;
	m_hasBeenUpdated[frameIndex] = true;
}

bool DX12VertexBuffer::hasBeenUpdated() const {
	auto frameIndex = (m_allowUpdates) ? m_context->getSwapIndex() : 0;
	return m_hasBeenUpdated[frameIndex];
}

void DX12VertexBuffer::resetHasBeenUpdated() { 
	auto frameIndex = (m_allowUpdates) ? m_context->getSwapIndex() : 0;
	m_hasBeenUpdated[frameIndex] = false;
}

bool DX12VertexBuffer::init(ID3D12GraphicsCommandList4* cmdList) {
	SAIL_PROFILE_API_SPECIFIC_FUNCTION();

	// This method is called at least once every frame that this vertex buffer is used

	auto numSwapBuffers = (m_allowUpdates) ? m_context->getNumGPUBuffers() : 1;
	for (unsigned int i = 0; i < numSwapBuffers; i++) {
		if (m_hasBeenInitialized[i]) {
			// Release the upload heap as soon as the texture has been uploaded to the GPU, but make sure it doesn't happen on the same frame as the upload
			if (!m_allowUpdates && m_uploadVertexBuffers[i] && m_initFrameCount != m_context->getFrameCount() && m_queueUsedForUpload->getCompletedFenceValue() > m_initFenceVal) {
				m_uploadVertexBuffers[i].ReleaseAndGetAddressOf();
			}
			continue;
		}

		// Copy the data from the uploadBuffer to the defaultBuffer
		cmdList->CopyBufferRegion(m_defaultVertexBuffers[i].Get(), 0, m_uploadVertexBuffers[i].Get(), 0, m_byteSize);
		// Transition to usage state
		DX12Utils::SetResourceTransitionBarrier(cmdList, m_defaultVertexBuffers[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		DX12Utils::SetResourceUAVBarrier(cmdList, m_defaultVertexBuffers[i].Get());

		// Signal is only required for the last buffer, since both will be uploaded when the last one is
		if (!m_allowUpdates && i == numSwapBuffers - 1) {
			auto type = cmdList->GetType();
			m_queueUsedForUpload = (type == D3D12_COMMAND_LIST_TYPE_DIRECT) ? m_context->getDirectQueue() : nullptr; // TODO: add support for other command list types
			// Schedule a signal to be called directly after this command list has executed
			// This allows us to know when the data has been uploaded to the GPU
			m_queueUsedForUpload->scheduleSignal([this](UINT64 signaledValue) {
				m_initFenceVal = signaledValue;
			});
			m_initFrameCount = m_context->getFrameCount();
		}

		m_hasBeenInitialized[i] = true;
	}

	return true;
}

