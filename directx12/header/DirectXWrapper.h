#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <vector>

namespace ms = Microsoft::WRL;

class DirectXWrapper
{
public:
	bool Init(HWND hwnd);
	void BeginDraw();
	void EndDraw();

	ms::ComPtr<ID3D12Device> GetDevice() const { return device; }
	ms::ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return cmdList; }

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

	D3D12_RESOURCE_BARRIER swapchainBarrier;

	ms::ComPtr<ID3D12Fence> fence = nullptr;
	UINT64 fenceVal = 0;
};
