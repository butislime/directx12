#pragma once

#include <DirectXTex.h>

#include <vector>
#include <functional>
#include <map>
#include <memory>

#include <DirectXWrapper.h>
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
	ID3D12Resource* CreateWhiteTexture(ms::ComPtr<ID3D12Device> device);
	ID3D12Resource* CreateBlackTexture(ms::ComPtr<ID3D12Device> device);
	ID3D12Resource* CreateGrayGradationTexture(ms::ComPtr<ID3D12Device> device);

	~Application();

private:

	// singleton
	Application();
	Application(const Application&) = delete;
	void operator= (const Application&) = delete;

private:
	WNDCLASSEX window = {};

	std::shared_ptr<DirectXWrapper> dxWrapper = nullptr;
	std::shared_ptr<PMDRenderer> pmdRenderer = nullptr;

	// resource cache
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> loadLambdaTable;
	std::map<std::string, ID3D12Resource*> _resourceTable;
};