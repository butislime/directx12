#include <Application.h>

#include <tchar.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <d3dx12.h>

#include <iostream>
#include <map>
#include <algorithm>

// todo
static auto WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720;
static std::string strModelPath = "model/初音ミク.pmd";

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT2 uv;
};

struct TexRGBA
{
	unsigned char r, g, b, a;
};

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	// デバッグレイヤーの有効化
	debugLayer->EnableDebugLayer();
	// インターフェースの解放
	debugLayer->Release();
}

Application::Application()
{
}
Application::~Application()
{
}

Application& Application::Instance()
{
	static Application _instance;
	return _instance;
}

unsigned int Application::GetWindowWidth() const { return WINDOW_WIDTH; }
unsigned int Application::GetWindowHeight() const { return WINDOW_HEIGHT; }

ID3D12Resource* Application::LoadTextureFromFile(ms::ComPtr<ID3D12Device> device, std::string& texPath)
{
	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end())
	{
		// 読み込み済み
		return it->second;
	}

	auto wtexpath = GetWideStringFromString(texPath);
	auto ext = GetExtension(texPath);
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};
	auto hresult = loadLambdaTable[ext](wtexpath, &metadata, scratchImg);

	if (FAILED(hresult))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);

	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, metadata.height, metadata.arraySize, metadata.mipLevels);

	ID3D12Resource* texBuff = nullptr;
	hresult = device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);

	if (FAILED(hresult))
	{
		return nullptr;
	}

	hresult = texBuff->WriteToSubresource(
		0, nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch
	);

	if (FAILED(hresult))
	{
		return nullptr;
	}

	_resourceTable[texPath] = texBuff;

	return texBuff;
}

bool Application::Init()
{
	// window initialize
	window.cbSize = sizeof(WNDCLASSEX);
	window.lpfnWndProc = (WNDPROC)WindowProcedure;
	window.lpszClassName = _T("DX12Sample");
	window.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&window);

	RECT wrc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(window.lpszClassName,
		_T("Dx12テスト"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		window.hInstance,
		nullptr
	);

#ifdef _DEBUG
	// デバッグレイヤーの有効化
	EnableDebugLayer();
#endif

	loadLambdaTable["sph"] = loadLambdaTable["spa"] = loadLambdaTable["bmp"] = loadLambdaTable["png"] = loadLambdaTable["spg"] =
		[](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img) -> HRESULT
	{
		return DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
	};
	loadLambdaTable["tga"] = 
		[](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img) -> HRESULT
	{
		return DirectX::LoadFromTGAFile(path.c_str(), meta, img);
	};
	loadLambdaTable["dds"] = 
		[](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img) -> HRESULT
	{
		return DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, meta, img);
	};

	// directx12 initialize
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
	hresult = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.ReleaseAndGetAddressOf()));

	//ID3D12CommandQueue* cmdQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};

	// タイムアウトなし
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// キュー生成
	hresult = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(cmdQueue.ReleaseAndGetAddressOf()));
	std::cout << "CreateCommandQueue res=" << hresult << std::endl;

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};

	swapchainDesc.Width = WINDOW_WIDTH;
	swapchainDesc.Height = WINDOW_HEIGHT;
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
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // ダブルバッファ
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

	// 深度バッファ
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = WINDOW_WIDTH;
	depthResDesc.Height = WINDOW_HEIGHT;
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT; // 32bit深度値フォーマット
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// 深度バッファのクリア設定
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	ID3D12Resource* depthBuffer = nullptr;
	hresult = device->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値書き込み
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

	hresult = CoInitializeEx(0, COINIT_MULTITHREADED);
	ShowWindow(hwnd, SW_SHOW);

	// viewport設定
	// default
	viewport = CD3DX12_VIEWPORT(backBuffers[0]);

	// シザー矩形
	scissorRect.top = 0;
	scissorRect.left = 0;
	scissorRect.right = scissorRect.left + WINDOW_WIDTH;
	scissorRect.bottom = scissorRect.top + WINDOW_HEIGHT;

	// pmd renderer
	auto pmd = LoadPMD(strModelPath);
	pmdRenderer = new PMDRenderer();
	pmdRenderer->Init(pmd, device);

	return true;
}

void Application::Run()
{
	MSG msg = {};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		auto bbIdx = swapchain->GetCurrentBackBufferIndex();

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // present
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
		cmdList->ResourceBarrier(1, &BarrierDesc);

		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		auto dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvHandle);

		// clear
		float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f/*最大深度*/, 0, 0, nullptr);

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);

		// render
		auto pipeline_state = pmdRenderer->GetPipelineState();
		cmdList->SetPipelineState(pipeline_state.Get());
		pmdRenderer->Render(device, cmdList);

#if false
		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);
		cmdList->SetGraphicsRootSignature(rootSignature.Get());
		ID3D12DescriptorHeap* basic_heaps[] = { basicDescHeap.Get() };
		cmdList->SetDescriptorHeaps(1, basic_heaps);
		cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());

		ID3D12DescriptorHeap* material_heaps[] = { materialDescHeap.Get() };
		cmdList->SetDescriptorHeaps(1, material_heaps);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->IASetVertexBuffers(0, 1, &vbView);
		cmdList->IASetIndexBuffer(&ibView);

		auto materialHandle = materialDescHeap->GetGPUDescriptorHandleForHeapStart();
		auto cbvsrvIncSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5; // cbvとsrv4つ
		unsigned int idxOffset = 0;
		for (auto& m : materials)
		{
			cmdList->SetGraphicsRootDescriptorTable(1, materialHandle);
			cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
			materialHandle.ptr += cbvsrvIncSize;
			idxOffset += m.indicesNum;
		}
#endif

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // present
		cmdList->ResourceBarrier(1, &BarrierDesc);

		// 命令の終了
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
		cmdList->Reset(cmdAllocator.Get(), pipeline_state.Get());

		swapchain->Present(1, 0);
	}
}

void Application::Terminate()
{
	UnregisterClass(window.lpszClassName, window.hInstance);

	for (size_t i = 0; i < backBuffers.size(); ++i)
	{
		if (backBuffers[i] == nullptr) continue;
		backBuffers[i]->Release();
	}

	delete pmdRenderer;
}