#include <DirectXWrapper.h>
#include <d3dx12.h>

#include <iostream>

#include <Application.h>

bool DirectXWrapper::Init(HWND hwnd)
{
	ms::ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;

	auto hresult = CreateDXGIFactory1(IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}
	// nvidia gpu
	for (auto adapter : adapters)
	{
		DXGI_ADAPTER_DESC adapterDesc = {};
		adapter->GetDesc(&adapterDesc);
		std::wstring strDesc = adapterDesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adapter;
			break;
		}
	}

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	D3D_FEATURE_LEVEL featureLevel;
	for (auto level : levels)
	{
		if (SUCCEEDED(D3D12CreateDevice(tmpAdapter, level, IID_PPV_ARGS(device.ReleaseAndGetAddressOf()))))
		{
			featureLevel = level;
			break;
		}
	}

	hresult = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAllocator.ReleaseAndGetAddressOf()));
	std::cout << "CreateCommandAllocator res=" << hresult << std::endl;
	hresult = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.ReleaseAndGetAddressOf()));
	std::cout << "CreateCommandList res=" << hresult << std::endl;

	//ID3D12CommandQueue* cmdQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};

	// �^�C���A�E�g�Ȃ�
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// �L���[����
	hresult = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(cmdQueue.ReleaseAndGetAddressOf()));
	std::cout << "CreateCommandQueue res=" << hresult << std::endl;

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};

	swapchainDesc.Width = Application::Instance().GetWindowWidth();
	swapchainDesc.Height = Application::Instance().GetWindowHeight();
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	hresult = _dxgiFactory->CreateSwapChainForHwnd(
		cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)swapchain.ReleaseAndGetAddressOf());
	std::cout << "CreateSwapChainForHwnd res=" << hresult << std::endl;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // �����_�[�^�[�Q�b�g�r���[
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // �_�u���o�b�t�@
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hresult = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeaps.ReleaseAndGetAddressOf()));
	std::cout << "rtvHeap CreateDescriptorHeap res=" << hresult << std::endl;

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	hresult = swapchain->GetDesc(&swcDesc);
	backBuffers.resize(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// sRGB
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		hresult = swapchain->GetBuffer(idx, IID_PPV_ARGS(&backBuffers[idx]));
		device->CreateRenderTargetView(backBuffers[idx], &rtvDesc, handle);
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	hresult = device->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()));

	// �[�x�o�b�t�@
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = Application::Instance().GetWindowWidth();
	depthResDesc.Height = Application::Instance().GetWindowHeight();
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT; // 32bit�[�x�l�t�H�[�}�b�g
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// �[�x�o�b�t�@�̃N���A�ݒ�
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	ID3D12Resource* depthBuffer = nullptr;
	hresult = device->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // �[�x�l��������
		&depthClearValue,
		IID_PPV_ARGS(&depthBuffer)
	);
	std::cout << "depth buffer res=" << hresult << std::endl;

	// depth stencil view
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hresult = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap.ReleaseAndGetAddressOf()));
	std::cout << "dsvHeap CreateDescriptorHeap res=" << hresult << std::endl;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	device->CreateDepthStencilView(
		depthBuffer,
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	// viewport�ݒ�
	// default
	viewport = CD3DX12_VIEWPORT(backBuffers[0]);

	// �V�U�[��`
	scissorRect.top = 0;
	scissorRect.left = 0;
	scissorRect.right = scissorRect.left + Application::Instance().GetWindowWidth();
	scissorRect.bottom = scissorRect.top + Application::Instance().GetWindowHeight();

	return true;
}
void DirectXWrapper::BeginDraw()
{
	auto bbIdx = swapchain->GetCurrentBackBufferIndex();

	swapchainBarrier = {};
	swapchainBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	swapchainBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	swapchainBarrier.Transition.pResource = backBuffers[bbIdx];
	swapchainBarrier.Transition.Subresource = 0;
	swapchainBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // present
	swapchainBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
	cmdList->ResourceBarrier(1, &swapchainBarrier);

	auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
	cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvHandle);

	// clear
	float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f/*�ő�[�x*/, 0, 0, nullptr);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
}
void DirectXWrapper::EndDraw()
{
	swapchainBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
	swapchainBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // present
	cmdList->ResourceBarrier(1, &swapchainBarrier);

	// ���߂̏I��
	cmdList->Close();

	ID3D12CommandList* cmdlists[] = { cmdList.Get() };
	cmdQueue->ExecuteCommandLists(1, cmdlists);

	cmdQueue->Signal(fence.Get(), ++fenceVal);
	if (fence->GetCompletedValue() != fenceVal)
	{
		auto event = CreateEvent(nullptr, false, false, nullptr);
		fence->SetEventOnCompletion(fenceVal, event);
		// block
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	cmdAllocator->Reset();
	cmdList->Reset(cmdAllocator.Get(), nullptr);

	swapchain->Present(1, 0);
}
