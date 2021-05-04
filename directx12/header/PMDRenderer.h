#pragma once

#include <pmd.h>
#include <d3d12.h>
#include <wrl.h>

// pmd
#include <string>
#include <DirectXMath.h>

struct PMDMaterialForHlsl
{
	DirectX::XMFLOAT3 diffuse;
	float alpha;
	DirectX::XMFLOAT3 specular;
	float specularity;
	DirectX::XMFLOAT3 ambient;
};

struct AdditionalMaterial
{
	std::string texPath;
	int toonIdx;
	bool edgeFlg;
};

struct Material
{
	unsigned int indicesNum;
	PMDMaterialForHlsl pmdmat4hlsl;
	AdditionalMaterial additional;
};

struct SceneMatrix
{
	DirectX::XMMATRIX world;
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMFLOAT3 eye;
};

namespace ms = Microsoft::WRL;

class PMDRenderer
{
public:
	void Init(PMD& pmd, ms::ComPtr<ID3D12Device> device);
	void Render(ms::ComPtr<ID3D12Device> device, ms::ComPtr<ID3D12GraphicsCommandList> cmdList);

	const ms::ComPtr<ID3D12PipelineState> GetPipelineState() const { return pipelineState; }
private:
	// pmd
	ms::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	ms::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> basicDescHeap = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> materialDescHeap = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	D3D12_INDEX_BUFFER_VIEW ibView = {};

	std::vector<Material> materials;
};
