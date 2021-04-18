#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#ifdef _DEBUG
#include <iostream>
#endif

using namespace std;

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

static auto WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720;

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	DebugOutputFormatString("Show Window Test.");
	getchar();

	// window initialize
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	w.lpszClassName = _T("DX12Sample");
	w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w);

	RECT wrc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(w.lpszClassName,
		_T("Dx12テスト"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr
	);
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	// デバッグレイヤーの有効化
	EnableDebugLayer();
#endif

	// directx12 initialize
	ID3D12Device* _dev = nullptr;
	IDXGIFactory6* _dxgiFactory = nullptr;
	IDXGISwapChain4* _swapchain = nullptr;

	auto hresult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_dev));
#ifdef _DEBUG
	hresult = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	hresult = CreateDXGIFactory(IID_PPV_ARGS(&_dxgiFactory));
#endif

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	hresult = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	ID3D12CommandAllocator* _cmdAllocator = nullptr;
	ID3D12GraphicsCommandList* _cmdList = nullptr;

	hresult = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	hresult = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

	ID3D12CommandQueue* _cmdQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};

	// タイムアウトなし
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	cmdQueueDesc.NodeMask = 0;

	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	// キュー生成
	hresult = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

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
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // ダブルバッファ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	hresult = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	hresult = _swapchain->GetDesc(&swcDesc);
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		hresult = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_dev->CreateRenderTargetView(_backBuffers[idx], nullptr, handle);
	}

	hresult = _cmdAllocator->Reset();
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

	auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // present
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
	_cmdList->ResourceBarrier(1, &BarrierDesc);

	_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

	float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	DirectX::XMFLOAT3 vertices[] =
	{
		{-1.0f, -1.0f, 0.0f},
		{-1.0f,  1.0f, 0.0f},
		{ 1.0f, -1.0f, 0.0f},
	};

	// 頂点バッファの作成
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(vertices);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* vertBuff = nullptr;
	hresult = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	// 頂点情報をバッファにマップ
	DirectX::XMFLOAT3* vertMap = nullptr;
	hresult = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(vertices); // 全バイト数
	vbView.StrideInBytes = sizeof(vertices[0]); // 1頂点あたりのバイト数

	// シェーダー読み込み
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	hresult = D3DCompileFromFile(
		L"shader/BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &vsBlob, &errorBlob);
	if (FAILED(hresult))
	{
		printf("vs res=%d not_found=%d\n", hresult, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
	}
	hresult = D3DCompileFromFile(
		L"shader/BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &psBlob, &errorBlob);
	if (FAILED(hresult))
	{
		printf("ps res=%d not_found=%d\n", hresult, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
	}

	//_cmdList->IASetVertexBuffers(0, 1, &vbView);

	// 命令の終了
	_cmdList->Close();

	ID3D12CommandList* cmdlists[] = { _cmdList };
	_cmdQueue->ExecuteCommandLists(1, cmdlists);

	_cmdQueue->Signal(_fence, ++_fenceVal);
	if (_fence->GetCompletedValue() != _fenceVal)
	{
		auto event = CreateEvent(nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion(_fenceVal, event);
		// block
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // present
	_cmdList->ResourceBarrier(1, &BarrierDesc);

	_cmdAllocator->Reset();
	_cmdList->Reset(_cmdAllocator, nullptr);

	_swapchain->Present(1, 0);

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
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}