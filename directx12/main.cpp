#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>

#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

#ifdef _DEBUG
#include <iostream>
#endif

using namespace std;

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

static auto WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720;

#ifdef _DEBUG
int main()
{
#else
#include <Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	//DebugOutputFormatString("Show Window Test.");
	//getchar();

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

#ifdef _DEBUG
	// デバッグレイヤーの有効化
	EnableDebugLayer();
#endif

	// directx12 initialize
	ID3D12Device* _dev = nullptr;
	IDXGIFactory6* _dxgiFactory = nullptr;
	IDXGISwapChain4* _swapchain = nullptr;

//#ifdef _DEBUG
	//hresult = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
//#else
	auto hresult = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
//#endif
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
		if (SUCCEEDED(D3D12CreateDevice(tmpAdapter, level, IID_PPV_ARGS(&_dev))))
		{
			featureLevel = level;
			break;
		}
	}

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
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// sRGB
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		hresult = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		_dev->CreateRenderTargetView(_backBuffers[idx], &rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	hresult = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	hresult = CoInitializeEx(0, COINIT_MULTITHREADED);

	ShowWindow(hwnd, SW_SHOW);

	// 頂点バッファの作成
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	Vertex vertices[] =
	{
		{{-0.4f, -0.7f, 0.0f}, {0.0f, 1.0f}}, // 左下
		{{-0.4f,  0.7f, 0.0f}, {0.0f, 0.0f}}, // 左上
		{{ 0.4f, -0.7f, 0.0f}, {1.0f, 1.0f}}, // 右下
		{{ 0.4f,  0.7f, 0.0f}, {1.0f, 0.0f}}, // 右上
	};

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
	Vertex* vertMap = nullptr;
	hresult = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(vertices); // 全バイト数
	vbView.StrideInBytes = sizeof(vertices[0]); // 1頂点あたりのバイト数

	unsigned int indices[] = {
		0, 1, 2,
		2, 1, 3,
	};

	ID3D12Resource* idxBuff = nullptr;
	resdesc.Width = sizeof(indices);
	hresult = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);

	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

	// texture
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};
	hresult = DirectX::LoadFromWICFile(
		L"texture/textest.png", DirectX::WIC_FLAGS_NONE,
		&metadata, scratchImg
	);
	auto img = scratchImg.GetImage(0, 0, 0);

	heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	resdesc = {};
	resdesc.Format = metadata.format;
	resdesc.Width = metadata.width;
	resdesc.Height = metadata.height;
	resdesc.DepthOrArraySize = metadata.arraySize;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.MipLevels = metadata.mipLevels;
	resdesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texbuff = nullptr;
	hresult = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);
	hresult = texbuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch
	);

	ID3D12DescriptorHeap* texDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hresult = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&texDescHeap));
	printf("CreateDescriptorHeap res=%d\n", hresult);

	// shader resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	_dev->CreateShaderResourceView(
		texbuff,
		&srvDesc,
		texDescHeap->GetCPUDescriptorHandleForHeapStart()
	);

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

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
			0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};

	D3D12_DESCRIPTOR_RANGE descTblRange = {};
	descTblRange.NumDescriptors = 1;
	descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange.BaseShaderRegister = 0;
	descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam = {};
	rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootparam.DescriptorTable.pDescriptorRanges = &descTblRange;
	rootparam.DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // u方向繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // v方向繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // w方向繰り返し
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	// ルートシグネチャの作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = &rootparam;
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	hresult = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);

	ID3D12RootSignature* rootsignature = nullptr;
	hresult = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootsignature)
	);
	rootSigBlob->Release();

	// グラフィックパイプラインの作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = rootsignature;
	gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();
	// デフォルトのサンプルマスク
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState.MultisampleEnable = false;
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpipeline.RasterizerState.DepthClipEnable = true;

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	ID3D12PipelineState* _pipelinestate = nullptr;
	hresult = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));
	printf("res=%d E_INVALIDARG=%d", hresult, E_INVALIDARG);

	// viewport設定
	D3D12_VIEWPORT viewport = {};
	viewport.Width = WINDOW_WIDTH;
	viewport.Height = WINDOW_HEIGHT;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	// シザー矩形
	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;
	scissorrect.left = 0;
	scissorrect.right = scissorrect.left + WINDOW_WIDTH;
	scissorrect.bottom = scissorrect.top + WINDOW_HEIGHT;

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

		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // present
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		_cmdList->SetPipelineState(_pipelinestate);

		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

		float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		_cmdList->SetGraphicsRootSignature(rootsignature);
		_cmdList->SetDescriptorHeaps(1, &texDescHeap);
		_cmdList->SetGraphicsRootDescriptorTable(0, texDescHeap->GetGPUDescriptorHandleForHeapStart());

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);
		_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // present
		_cmdList->ResourceBarrier(1, &BarrierDesc);

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

		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator, _pipelinestate);

		_swapchain->Present(1, 0);
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}