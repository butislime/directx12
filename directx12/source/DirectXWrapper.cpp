#include <DirectXWrapper.h>
#include <d3dx12.h>
#include <d3dcompiler.h>

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

	// タイムアウトなし
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// キュー生成
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

	{
		// 作成済みのヒープからもう1枚作成
		auto heap_desc = rtvHeaps->GetDesc();
		// バックバッファの情報を利用
		auto& back_buff = backBuffers[0];
		auto res_desc = back_buff->GetDesc();

		auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		float clear_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		auto clear_value = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clear_color);

		hresult = device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &res_desc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // D3D12_RESOURCE_STATE_RENDER_TARGETではない
			&clear_value, IID_PPV_ARGS(peraResource.ReleaseAndGetAddressOf()));
		std::cout << "Quad CreateQuadResource res=" << hresult << std::endl;

		// rtv用ヒープ
		heap_desc.NumDescriptors = 1;
		hresult = device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(peraRTVHeap.ReleaseAndGetAddressOf()));
		std::cout << "Quad CreateRTVHeap res=" << hresult << std::endl;

		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		// rtv作成
		device->CreateRenderTargetView(peraResource.Get(), &rtvDesc, peraRTVHeap->GetCPUDescriptorHandleForHeapStart());

		// srv用ヒープ
		heap_desc.NumDescriptors = 1;
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heap_desc.NodeMask = 0;
		hresult = device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(peraSRVHeap.ReleaseAndGetAddressOf()));
		std::cout << "Quad CreateSRVHeap res=" << hresult << std::endl;

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Format = rtv_desc.Format;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		// srv作成
		device->CreateShaderResourceView(peraResource.Get(), &srv_desc, peraSRVHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// 深度バッファ
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = Application::Instance().GetWindowWidth();
	depthResDesc.Height = Application::Instance().GetWindowHeight();
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

	// viewport設定
	// default
	viewport = CD3DX12_VIEWPORT(backBuffers[0]);

	// シザー矩形
	scissorRect.top = 0;
	scissorRect.left = 0;
	scissorRect.right = scissorRect.left + Application::Instance().GetWindowWidth();
	scissorRect.bottom = scissorRect.top + Application::Instance().GetWindowHeight();

	{
		struct Vertex
		{
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT2 uv;
		};
		Vertex vert[4] =
		{
			{{-1, -1, 0.1f}, {0, 1}},
			{{-1,  1, 0.1f}, {0, 0}},
			{{ 1, -1, 0.1f}, {1, 1}},
			{{ 1,  1, 0.1f}, {1, 0}}
		};
		auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vert));
		hresult = device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &resource_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(quadVB.ReleaseAndGetAddressOf()));
		std::cout << "Quad CreateCommittedResource res=" << hresult << std::endl;

		quadVBView.BufferLocation = quadVB->GetGPUVirtualAddress();
		quadVBView.SizeInBytes = sizeof(vert);
		quadVBView.StrideInBytes = sizeof(Vertex);

		Vertex* mapped_quad = nullptr;
		quadVB->Map(0, nullptr, (void**)&mapped_quad);
		std::copy(std::begin(vert), std::end(vert), mapped_quad);
		quadVB->Unmap(0, nullptr);

		D3D12_INPUT_ELEMENT_DESC layout[2] =
		{
			{
				"POSITION",
				0,
				DXGI_FORMAT_R32G32B32_FLOAT,
				0,
				D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			},
			{
				"TEXCOORD",
				0,
				DXGI_FORMAT_R32G32_FLOAT,
				0,
				D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			},
		};

		D3D12_DESCRIPTOR_RANGE range = {};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // t
		range.BaseShaderRegister = 0; // 0
		range.NumDescriptors = 1;

		D3D12_ROOT_PARAMETER root_param = {};
		root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		root_param.DescriptorTable.pDescriptorRanges = &range;
		root_param.DescriptorTable.NumDescriptorRanges = 1;

		D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC gps_desc = {};
		gps_desc.InputLayout.NumElements = _countof(layout);
		gps_desc.InputLayout.pInputElementDescs = layout;

		ms::ComPtr<ID3DBlob> vs;
		ms::ComPtr<ID3DBlob> ps;
		ms::ComPtr<ID3DBlob> error_blob;

		hresult = D3DCompileFromFile(
			L"shader/QuadVertexShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main", "vs_5_0", 0, 0,
			vs.ReleaseAndGetAddressOf(), error_blob.ReleaseAndGetAddressOf()
		);
		std::cout << "shader compile QuadVertexShader.hlsl res=" << hresult << " not found=" << HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) << std::endl;
		hresult = D3DCompileFromFile(
			L"shader/QuadPixelShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main", "ps_5_0", 0, 0,
			ps.ReleaseAndGetAddressOf(), error_blob.ReleaseAndGetAddressOf()
		);
		std::cout << "shader compile QuadPixelShader.hlsl res=" << hresult << std::endl;
		gps_desc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		gps_desc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());

		gps_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		gps_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		gps_desc.NumRenderTargets = 1;
		gps_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		gps_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		gps_desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		gps_desc.SampleDesc.Count = 1;
		gps_desc.SampleDesc.Quality = 0;
		gps_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		D3D12_ROOT_SIGNATURE_DESC root_sign_desc = {};
		root_sign_desc.NumParameters = 1;
		root_sign_desc.pParameters = &root_param;
		root_sign_desc.NumStaticSamplers = 1;
		root_sign_desc.pStaticSamplers = &sampler;
		root_sign_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ms::ComPtr<ID3DBlob> root_sign_blob;
		hresult = D3D12SerializeRootSignature(
			&root_sign_desc, D3D_ROOT_SIGNATURE_VERSION_1,
			root_sign_blob.ReleaseAndGetAddressOf(), error_blob.ReleaseAndGetAddressOf()
		);
		std::cout << "Quad SerializeRootSignature res=" << hresult << std::endl;
		hresult = device->CreateRootSignature(0,
			root_sign_blob->GetBufferPointer(), root_sign_blob->GetBufferSize(), IID_PPV_ARGS(quadRootSignature.ReleaseAndGetAddressOf()));
		std::cout << "Quad CreateRootSignature res=" << hresult << std::endl;

		gps_desc.pRootSignature = quadRootSignature.Get();
		hresult = device->CreateGraphicsPipelineState(&gps_desc, IID_PPV_ARGS(quadPipeline.ReleaseAndGetAddressOf()));
		std::cout << "Quad CreateGraphicsPipelineState res=" << hresult << std::endl;
	}

	return true;
}
void DirectXWrapper::BeginPeraDraw()
{
	// 1パス目
	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(peraResource.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ResourceBarrier(1, &transition);

	auto rtvH = peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
	cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvHandle);

	// clear
	float clear_color[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	cmdList->ClearRenderTargetView(rtvH, clear_color, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f/*最大深度*/, 0, 0, nullptr);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
}
void DirectXWrapper::EndPeraDraw()
{
	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(peraResource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmdList->ResourceBarrier(1, &transition);
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
	cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvHandle);

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	// clear
	float clear_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmdList->ClearRenderTargetView(rtvH, clear_color, 0, nullptr);
	cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f/*最大深度*/, 0, 0, nullptr);

	cmdList->SetGraphicsRootSignature(quadRootSignature.Get());
	cmdList->SetDescriptorHeaps(1, peraSRVHeap.GetAddressOf());
	auto handle = peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	cmdList->SetGraphicsRootDescriptorTable(0, handle);

	cmdList->SetPipelineState(quadPipeline.Get());
	cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmdList->IASetVertexBuffers(0, 1, &quadVBView);
	cmdList->DrawInstanced(4, 1, 0, 0);
}
void DirectXWrapper::EndDraw()
{
	swapchainBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // render target
	swapchainBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT; // present
	cmdList->ResourceBarrier(1, &swapchainBarrier);

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
	cmdList->Reset(cmdAllocator.Get(), nullptr);

	swapchain->Present(1, 0);
}
