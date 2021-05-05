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

ID3D12Resource* Application::CreateWhiteTexture(ms::ComPtr<ID3D12Device> device)
{
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4/*width*/, 4/*height*/);

	ID3D12Resource* whiteBuff = nullptr;
	auto hresult = device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&whiteBuff)
	);

	if (FAILED(hresult))
	{
		return nullptr;
	}

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0xff);

	hresult = whiteBuff->WriteToSubresource(
		0, nullptr,
		data.data(),
		4 * 4,
		data.size()
	);

	return whiteBuff;
}

ID3D12Resource* Application::CreateBlackTexture(ms::ComPtr<ID3D12Device> device)
{
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4/*width*/, 4/*height*/);

	ID3D12Resource* blackBuff = nullptr;
	auto hresult = device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&blackBuff)
	);

	if (FAILED(hresult))
	{
		return nullptr;
	}

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0x00);

	hresult = blackBuff->WriteToSubresource(
		0, nullptr,
		data.data(),
		4 * 4,
		data.size()
	);

	return blackBuff;
}

ID3D12Resource* Application::CreateGrayGradationTexture(ms::ComPtr<ID3D12Device> device)
{
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4/*width*/, 256/*height*/);

	ID3D12Resource* gradBuff = nullptr;
	auto hresult = device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&gradBuff)
	);

	if (FAILED(hresult))
	{
		return nullptr;
	}

	std::vector<unsigned int> data(4 * 256);
	auto it = data.begin();
	unsigned int c = 0xff;
	for (; it != data.end(); it += 4)
	{
		auto col = (0xff << 24) | RGB(c,c,c);
		std::fill(it, it + 4, col);
		--c;
	}

	hresult = gradBuff->WriteToSubresource(
		0, nullptr,
		data.data(),
		4 * sizeof(data[0]),
		sizeof(data[0]) * data.size()
	);

	return gradBuff;
}
bool Application::Init()
{
	auto hresult = CoInitializeEx(0, COINIT_MULTITHREADED);
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
	dxWrapper.reset(new DirectXWrapper());
	dxWrapper->Init(hwnd);

	// pmd renderer
	auto pmd = LoadPMD(strModelPath);
	pmdRenderer.reset(new PMDRenderer());
	pmdRenderer->Init(pmd, dxWrapper->GetDevice());

	ShowWindow(hwnd, SW_SHOW);

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

		dxWrapper->BeginDraw();

		auto command_list = dxWrapper->GetCommandList();
		// render
		auto pipeline_state = pmdRenderer->GetPipelineState();
		command_list->SetPipelineState(pipeline_state.Get());
		pmdRenderer->Render(dxWrapper->GetDevice().Get(), command_list);

		dxWrapper->EndDraw();
	}
}

void Application::Terminate()
{
	UnregisterClass(window.lpszClassName, window.hInstance);
}