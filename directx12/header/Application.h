#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXTex.h>

#include <vector>
#include <functional>
#include <map>

#include <PMDRenderer.h>

namespace ms = Microsoft::WRL;

class Application
{
public:
	static Application& Instance();

	bool Init();
	void Run();
	void Terminate();

	unsigned int GetWindowWidth() const;
	unsigned int GetWindowHeight() const;

	ID3D12Resource* LoadTextureFromFile(ms::ComPtr<ID3D12Device> device, std::string& texPath);

	~Application();

private:

	// singleton
	Application();
	Application(const Application&) = delete;
	void operator= (const Application&) = delete;

private:
	ms::ComPtr<ID3D12Device> device = nullptr;
	ms::ComPtr<ID3D12CommandAllocator> cmdAllocator = nullptr;
	ms::ComPtr<ID3D12GraphicsCommandList> cmdList = nullptr;
	ms::ComPtr<ID3D12CommandQueue> cmdQueue = nullptr;
	ms::ComPtr<IDXGISwapChain4> swapchain = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> rtvHeaps = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> dsvHeap = nullptr;

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT scissorRect = {};

	std::vector<ID3D12Resource*> backBuffers;

	ms::ComPtr<ID3D12Fence> fence = nullptr;
	UINT64 fenceVal = 0;

	WNDCLASSEX window = {};

	PMDRenderer* pmdRenderer = nullptr;

	// resource cache
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> loadLambdaTable;
	std::map<std::string, ID3D12Resource*> _resourceTable;
};