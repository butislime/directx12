#pragma once

#include <wrl.h>

// pmd
#include <PMDActor.h>
#include <string>
#include <memory>
#include <DirectXMath.h>

struct SceneMatrix
{
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMFLOAT3 eye;
};

namespace ms = Microsoft::WRL;

class PMDRenderer
{
public:
	void Init(ms::ComPtr<ID3D12Device> device, std::shared_ptr<PMDActor> actor);
	void Render(ms::ComPtr<ID3D12Device> device, ms::ComPtr<ID3D12GraphicsCommandList> cmdList);

	const ms::ComPtr<ID3D12PipelineState> GetPipelineState() const { return pipelineState; }

private:
	HRESULT CreateTransformView(ms::ComPtr<ID3D12Device> device);

private:
	// pmd
	ms::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	ms::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> basicDescHeap = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> transformDescHeap = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> materialDescHeap = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	D3D12_INDEX_BUFFER_VIEW ibView = {};

	ms::ComPtr<ID3D12Resource> transformBuff = nullptr;
	DirectX::XMMATRIX* mappedMatrices = nullptr;

	std::shared_ptr<PMDActor> actor;
};
