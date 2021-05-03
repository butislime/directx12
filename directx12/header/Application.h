#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>

namespace ms = Microsoft::WRL;

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

class Application
{
public:
	static Application& Instance();

	bool Init();
	void Run();
	void Terminate();

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
	ms::ComPtr<ID3D12PipelineState> pipelineState = nullptr;
	ms::ComPtr<IDXGISwapChain4> swapchain = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> rtvHeaps = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> dsvHeap = nullptr;
	ms::ComPtr<ID3D12RootSignature> rootSignature = nullptr;

	D3D12_VIEWPORT viewport = {};
	D3D12_RECT scissorRect = {};

	std::vector<ID3D12Resource*> backBuffers;

	ms::ComPtr<ID3D12Fence> fence = nullptr;
	UINT64 fenceVal = 0;

	WNDCLASSEX window = {};

	// pmd
	ms::ComPtr<ID3D12DescriptorHeap> basicDescHeap = nullptr;
	ms::ComPtr<ID3D12DescriptorHeap> materialDescHeap = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	D3D12_INDEX_BUFFER_VIEW ibView = {};

	std::vector<Material> materials;
};