#include <PMDRenderer.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <map>
#include <iostream>

#include <Application.h>

size_t AlignmentedSize(size_t size, size_t alignment)
{
	return size + alignment - size % alignment;
}

void* Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
}

void PMDRenderer::Init(PMD& pmd, VMD& vmd, ms::ComPtr<ID3D12Device> device)
{
	// material convert
	materials.resize(pmd.materials.size());
	for (int i = 0; i < pmd.materials.size(); ++i)
	{
		materials[i].indicesNum = pmd.materials[i].indicesNum;
		materials[i].pmdmat4hlsl.diffuse = pmd.materials[i].diffuse;
		materials[i].pmdmat4hlsl.alpha = pmd.materials[i].alpha;
		materials[i].pmdmat4hlsl.specular = pmd.materials[i].specular;
		materials[i].pmdmat4hlsl.specularity = pmd.materials[i].specularity;
		materials[i].pmdmat4hlsl.ambient = pmd.materials[i].ambient;
	}
	// bone convert
	std::vector<std::string> bone_names(pmd.bones.size());
	for (int idx = 0; idx < pmd.bones.size(); ++idx)
	{
		auto& pb = pmd.bones[idx];
		bone_names[idx] = pb.boneName;
		auto& node = boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}
	for (auto& pb : pmd.bones)
	{
		if (pb.parentNo >= pmd.bones.size()) continue;

		auto parent_name = bone_names[pb.parentNo];
		boneNodeTable[parent_name].children.emplace_back(&boneNodeTable[pb.boneName]);
	}
	transform.boneMatrices.resize(BoneMax);
	std::fill(transform.boneMatrices.begin(), transform.boneMatrices.end(), DirectX::XMMatrixIdentity());

	this->vmd = vmd;

	// 頂点バッファの作成
	ID3D12Resource* vertBuff = nullptr;
	D3D12_HEAP_PROPERTIES heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC resdesc = CD3DX12_RESOURCE_DESC::Buffer(pmd.vertices.size());
	auto hresult = device->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	// 頂点情報をバッファにマップ
	unsigned char* vertMap = nullptr;
	hresult = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(pmd.vertices), std::end(pmd.vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = pmd.vertices.size(); // 全バイト数
	vbView.StrideInBytes = pmdvertex_size;

	// インデックスバッファ
	ID3D12Resource* idxBuff = nullptr;
	resdesc = CD3DX12_RESOURCE_DESC::Buffer(pmd.indices.size() * sizeof(pmd.indices[0]));
	hresult = device->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);

	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(pmd.indices), std::end(pmd.indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = pmd.indices.size() * sizeof(pmd.indices[0]);

	// texture
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};
	hresult = DirectX::LoadFromWICFile(
		L"texture/textest.png", DirectX::WIC_FLAGS_NONE,
		&metadata, scratchImg
	);
	auto img = scratchImg.GetImage(0, 0, 0);
	D3D12_HEAP_PROPERTIES uploadHeapProp = {};
	uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	// upload用なのでunknown
	uploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProp.CreationNodeMask = 0;
	uploadHeapProp.VisibleNodeMask = 0;

	resdesc = {};
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * img->height;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;

	// 中間バッファ
	ID3D12Resource* uploadBuff = nullptr;
	hresult = device->CreateCommittedResource(
		&uploadHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuff)
	);
	printf("uploadBuff result=%d\n", hresult);

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	resdesc.Format = metadata.format;
	resdesc.Width = metadata.width;
	resdesc.Height = metadata.height;
	resdesc.DepthOrArraySize = metadata.arraySize;
	resdesc.MipLevels = metadata.mipLevels;
	resdesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	// テクスチャバッファ
	ID3D12Resource* texBuff = nullptr;
	hresult = device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);
	printf("texBuff result=%d\n", hresult);

	uint8_t* mapForImg = nullptr;
	hresult = uploadBuff->Map(0, nullptr, (void**)&mapForImg);

	auto srcAddress = img->pixels;
	auto rowPitch = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	for (int y = 0; y < img->height; ++y)
	{
		std::copy_n(srcAddress, rowPitch, mapForImg);
		srcAddress += img->rowPitch;
		mapForImg += rowPitch;
	}

	uploadBuff->Unmap(0, nullptr);
	printf("map result=%d\n", hresult);

	D3D12_TEXTURE_COPY_LOCATION src = {};
	// コピー元
	src.pResource = uploadBuff;
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Offset = 0;
	src.PlacedFootprint.Footprint.Width = metadata.width;
	src.PlacedFootprint.Footprint.Height = metadata.height;
	src.PlacedFootprint.Footprint.Depth = metadata.depth;
	src.PlacedFootprint.Footprint.RowPitch = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	src.PlacedFootprint.Footprint.Format = img->format;

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	// コピー先
	dst.pResource = texBuff;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	std::vector<ID3D12Resource*> textureResources(pmd.materials.size());
	std::vector<ID3D12Resource*> sphResources(pmd.materials.size());
	std::vector<ID3D12Resource*> spaResources(pmd.materials.size());
	std::vector<ID3D12Resource*> toonResources(pmd.materials.size());
	for (int i = 0; i < pmd.materials.size(); ++i)
	{
		// LUT
		std::string toonFilePath = "toon/";
		char toonFileName[16];
		sprintf_s(toonFileName, "toon%02d.bmp", pmd.materials[i].toonIdx + 1);
		toonFilePath += toonFileName;
		toonResources[i] = Application::Instance().LoadTextureFromFile(device.Get(), toonFilePath);

		std::string fileName = pmd.materials[i].texFilePath;
		std::string texFileName = std::string();
		std::string sphFileName = std::string();
		std::string spaFileName = std::string();
		if (std::count(fileName.begin(), fileName.end(), '*') > 0)
		{
			// 分割する
			auto namepair = SplitFileName(fileName);
			if (GetExtension(namepair.first) == "sph" ||
				GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.second;
				if (GetExtension(namepair.first) == "sph")
					sphFileName = namepair.first;
				else if(GetExtension(namepair.first) == "spa")
					spaFileName = namepair.first;
			}
			else
			{
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph")
					sphFileName = namepair.second;
				else if(GetExtension(namepair.second) == "spa")
					spaFileName = namepair.second;
			}
		}
		else
		{
			auto ext = GetExtension(fileName);
			if (ext == "sph")
				sphFileName = fileName;
			else if (ext == "spa")
				spaFileName = fileName;
			else
				texFileName = fileName;
		}

		if (!texFileName.empty())
		{
			auto texFilePath = GetTexturePathFromModelAndTexPath(pmd.filePath, texFileName.c_str());
			std::cout << "tex path=" << texFilePath << std::endl;
			textureResources[i] = Application::Instance().LoadTextureFromFile(device.Get(), texFilePath);
		}
		else textureResources[i] = nullptr;

		if (!sphFileName.empty())
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(pmd.filePath, sphFileName.c_str());
			std::cout << "sph path=" << sphFilePath << std::endl;
			sphResources[i] = Application::Instance().LoadTextureFromFile(device.Get(), sphFilePath);
		}
		else sphResources[i] = nullptr;

		if (!spaFileName.empty())
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(pmd.filePath, spaFileName.c_str());
			std::cout << "spa path=" << spaFilePath << std::endl;
			spaResources[i] = Application::Instance().LoadTextureFromFile(device.Get(), spaFilePath);
		}
		else spaResources[i] = nullptr;
	}

	// matrix
	// view
	DirectX::XMFLOAT3 eye(0, 10, -15);
	DirectX::XMFLOAT3 target(0, 10, 0);
	DirectX::XMFLOAT3 up(0, 1, 0);
	DirectX::XMMATRIX viewMat = DirectX::XMMatrixLookAtLH(
		DirectX::XMLoadFloat3(&eye),
		DirectX::XMLoadFloat3(&target),
		DirectX::XMLoadFloat3(&up)
	);
	auto window_width = Application::Instance().GetWindowWidth();
	auto window_height = Application::Instance().GetWindowHeight();
	// projection
	DirectX::XMMATRIX projMat = DirectX::XMMatrixPerspectiveFovLH(
		DirectX::XM_PIDIV2,
		static_cast<float>(window_width) / static_cast<float>(window_height),
		1.0f, // near
		100.0f // far
	);
	// 定数バッファ作成
	{
		ID3D12Resource* scene_mat_buff = nullptr;
		auto matHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto matResourceDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff & ~0xff));
		device->CreateCommittedResource(
			&matHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&matResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&scene_mat_buff)
		);
		SceneMatrix* scene_matrix;
		hresult = scene_mat_buff->Map(0, nullptr, (void**)&scene_matrix);
		scene_matrix->view = viewMat;
		scene_matrix->proj = projMat;
		scene_matrix->eye = eye;

		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
		descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descHeapDesc.NodeMask = 0;
		descHeapDesc.NumDescriptors = 1; // cbv
		descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		hresult = device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(basicDescHeap.ReleaseAndGetAddressOf()));
		std::cout << "CreateDescriptorHeap res=" << hresult << std::endl;

		auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = scene_mat_buff->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = scene_mat_buff->GetDesc().Width;
		device->CreateConstantBufferView(&cbvDesc, basicHeapHandle);
	}

	hresult = CreateTransformView(device);
	std::cout << "CreateTransformView res=" << hresult << std::endl;

	// material
	auto materialBuffSize = sizeof(PMDMaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff; // 256byteアライメント
	ID3D12Resource* materialBuff = nullptr;
	heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resdesc = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materials.size());
	hresult = device->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&materialBuff)
	);

	char* mapMaterial = nullptr;
	hresult = materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	for (auto& m : materials)
	{
		*((PMDMaterialForHlsl*)mapMaterial) = m.pmdmat4hlsl;
		mapMaterial += materialBuffSize; // 次のアライメント位置
	}
	materialBuff->Unmap(0, nullptr);

	// 通常テクスチャビュー
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	//ID3D12DescriptorHeap* materialDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = materials.size() * 5;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hresult = device->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(materialDescHeap.ReleaseAndGetAddressOf()));
	std::cout << "materialDescHeap CreateDescriptorHeap res=" << hresult << std::endl;

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize; // 256byteアライメントされたサイズ

	auto matDescHeapHandle = materialDescHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	auto whiteTex = Application::Instance().CreateWhiteTexture(device);
	auto blackTex = Application::Instance().CreateBlackTexture(device);
	auto gradTex = Application::Instance().CreateGrayGradationTexture(device);
	for (int i = 0; i < materials.size(); ++i)
	{
		device->CreateConstantBufferView(&matCBVDesc, matDescHeapHandle);
		matDescHeapHandle.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (textureResources[i] == nullptr)
		{
			// テクスチャ指定なしは白テクスチャ
			srvDesc.Format = whiteTex->GetDesc().Format;
			device->CreateShaderResourceView(whiteTex, &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = textureResources[i]->GetDesc().Format;
			device->CreateShaderResourceView(textureResources[i], &srvDesc, matDescHeapHandle);
		}
		matDescHeapHandle.ptr += incSize;

		if (sphResources[i] == nullptr)
		{
			// 乗算スフィア指定なしは白テクスチャ
			srvDesc.Format = whiteTex->GetDesc().Format;
			device->CreateShaderResourceView(whiteTex, &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = sphResources[i]->GetDesc().Format;
			device->CreateShaderResourceView(sphResources[i], &srvDesc, matDescHeapHandle);
		}
		matDescHeapHandle.ptr += incSize;

		if (spaResources[i] == nullptr)
		{
			// 加算スフィア指定なしは黒テクスチャ
			srvDesc.Format = blackTex->GetDesc().Format;
			device->CreateShaderResourceView(blackTex, &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = spaResources[i]->GetDesc().Format;
			device->CreateShaderResourceView(spaResources[i], &srvDesc, matDescHeapHandle);
		}
		matDescHeapHandle.ptr += incSize;

		if (toonResources[i] == nullptr)
		{
			// トーンマップ指定なしはグラデーションテクスチャ
			srvDesc.Format = gradTex->GetDesc().Format;
			device->CreateShaderResourceView(gradTex, &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = toonResources[i]->GetDesc().Format;
			device->CreateShaderResourceView(toonResources[i], &srvDesc, matDescHeapHandle);
		}
		matDescHeapHandle.ptr += incSize;
	}

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
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"BONE_NO", 0, DXGI_FORMAT_R16G16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"EDGE_FLAG", 0, DXGI_FORMAT_R8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};

	CD3DX12_DESCRIPTOR_RANGE descTblRange[4] = {}; // 定数バッファとテクスチャ
	// matrix
	descTblRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1/*num*/, 0/*register*/);
	// transform
	descTblRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1/*num*/, 1/*register*/);
	// material
	descTblRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1/*num*/, 2/*register*/);
	// texture
	// 通常テクスチャとsphとspaとtoon
	descTblRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4/*num*/, 0/*register*/);

	CD3DX12_ROOT_PARAMETER rootparams[3] = {};
	rootparams[0].InitAsDescriptorTable(1, &descTblRange[0]);
	rootparams[1].InitAsDescriptorTable(1, &descTblRange[1]);
	rootparams[2].InitAsDescriptorTable(2, &descTblRange[2]);

	CD3DX12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	samplerDescs[0].Init(0/*register*/);
	samplerDescs[1].Init(1/*register*/, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	// ルートシグネチャの作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootparams;
	rootSignatureDesc.NumParameters = 3;
	rootSignatureDesc.pStaticSamplers = samplerDescs;
	rootSignatureDesc.NumStaticSamplers = 2;

	ID3DBlob* rootSigBlob = nullptr;
	hresult = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);
	printf("SerializeRootSignature res=%d\n", hresult);

	hresult = device->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf())
	);
	rootSigBlob->Release();
	std::cout << "CreateRootSignature res=" << hresult << std::endl;

	// グラフィックパイプラインの作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = rootSignature.Get();
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob);
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob);
	// デフォルトのサンプルマスク
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // mmdに合わせてカリングオフ

	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

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

	// depth stencil
	gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DepthStencilState.StencilEnable = false;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	hresult = device->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()));
	std::cout << "CreateGraphicsPipelineState res=" << hresult << std::endl;
}

HRESULT PMDRenderer::CreateTransformView(ms::ComPtr<ID3D12Device> device)
{
	auto matHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto matResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Transform::CalcAlignmentedSize(transform));
	auto hresult = device->CreateCommittedResource(
		&matHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&matResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(transformBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(hresult))
	{
		return hresult;
	}

	hresult = transformBuff->Map(0, nullptr, (void**)&mappedMatrices);
	std::cout << "CreateTransformView::Map res=" << hresult << std::endl;
	if (FAILED(hresult))
	{
		return hresult;
	}

	// world
	transform.world = DirectX::XMMatrixIdentity();
	mappedMatrices[0] = transform.world;
	std::copy(transform.boneMatrices.begin(), transform.boneMatrices.end(), &mappedMatrices[1]);

	// test
#if false
	auto arm_node = boneNodeTable["左腕"];
	auto& arm_pos = arm_node.startPos;
	auto arm_mat = DirectX::XMMatrixTranslation(-arm_pos.x, -arm_pos.y, -arm_pos.z) // 原点へ移動
		* DirectX::XMMatrixRotationZ(DirectX::XM_PIDIV2) // 回転
		* DirectX::XMMatrixTranslation(arm_pos.x, arm_pos.y, arm_pos.z); // 元の位置へ
	transform.boneMatrices[arm_node.boneIdx] = arm_mat;

	auto elbow_node = boneNodeTable["左ひじ"];
	auto& elbow_pos = elbow_node.startPos;
	auto elbow_mat = DirectX::XMMatrixTranslation(-elbow_pos.x, -elbow_pos.y, -elbow_pos.z)
		* DirectX::XMMatrixRotationZ(-DirectX::XM_PIDIV2)
		* DirectX::XMMatrixTranslation(elbow_pos.x, elbow_pos.y, elbow_pos.z);
	transform.boneMatrices[elbow_node.boneIdx] = elbow_mat;

	RecursiveMatrixMultiply(&boneNodeTable["センター"], DirectX::XMMatrixIdentity());

	mappedMatrices[0] = transform.world;
	std::copy(transform.boneMatrices.begin(), transform.boneMatrices.end(), &mappedMatrices[1]);
#else
	for (auto& key_frame : vmd.keyFrames)
	{
		auto node = boneNodeTable[key_frame.first];
		auto& pos = node.startPos;
		auto mat = DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* DirectX::XMMatrixRotationQuaternion(key_frame.second[0].quaternion)
			* DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		transform.boneMatrices[node.boneIdx] = mat;
	}
	RecursiveMatrixMultiply(&boneNodeTable["センター"], DirectX::XMMatrixIdentity());

	mappedMatrices[0] = transform.world;
	std::copy(transform.boneMatrices.begin(), transform.boneMatrices.end(), &mappedMatrices[1]);
#endif

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1; // cbv
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hresult = device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(transformDescHeap.ReleaseAndGetAddressOf()));
	std::cout << "CreateTransformView::CreateDescriptorHeap res=" << hresult << std::endl;
	if (FAILED(hresult))
	{
		return hresult;
	}

	auto basicHeapHandle = transformDescHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = transformBuff->GetDesc().Width;
	device->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	return S_OK;
}

void PMDRenderer::RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat)
{
	transform.boneMatrices[node->boneIdx] *= mat;
	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultiply(cnode, transform.boneMatrices[node->boneIdx]);
	}
}

void PMDRenderer::SetMotion(VMD& vmd)
{
	this->vmd = vmd;
	// todo
	UpdateBones();
}

void PMDRenderer::UpdateBones()
{
#if false
	auto arm_node = boneNodeTable["左腕"];
	auto& arm_pos = arm_node.startPos;
	auto arm_mat = DirectX::XMMatrixTranslation(-arm_pos.x, -arm_pos.y, -arm_pos.z) // 原点へ移動
		* DirectX::XMMatrixRotationZ(DirectX::XM_PIDIV2) // 回転
		* DirectX::XMMatrixTranslation(arm_pos.x, arm_pos.y, arm_pos.z); // 元の位置へ
	transform.boneMatrices[arm_node.boneIdx] = arm_mat;

	auto elbow_node = boneNodeTable["左ひじ"];
	auto& elbow_pos = elbow_node.startPos;
	auto elbow_mat = DirectX::XMMatrixTranslation(-elbow_pos.x, -elbow_pos.y, -elbow_pos.z)
		* DirectX::XMMatrixRotationZ(-DirectX::XM_PIDIV2)
		* DirectX::XMMatrixTranslation(elbow_pos.x, elbow_pos.y, elbow_pos.z);
	transform.boneMatrices[elbow_node.boneIdx] = elbow_mat;

	RecursiveMatrixMultiply(&boneNodeTable["センター"], DirectX::XMMatrixIdentity());

	mappedMatrices[0] = transform.world;
	std::copy(transform.boneMatrices.begin(), transform.boneMatrices.end(), &mappedMatrices[1]);
#else
	for (auto& key_frame : vmd.keyFrames)
	{
		auto node = boneNodeTable[key_frame.first];
		auto& pos = node.startPos;
		auto mat = DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* DirectX::XMMatrixRotationQuaternion(key_frame.second[0].quaternion)
			* DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		transform.boneMatrices[node.boneIdx] = mat;
	}
	RecursiveMatrixMultiply(&boneNodeTable["センター"], DirectX::XMMatrixIdentity());

	mappedMatrices[0] = transform.world;
	std::copy(transform.boneMatrices.begin(), transform.boneMatrices.end(), &mappedMatrices[1]);
#endif
}

void PMDRenderer::Update()
{
#if false
	for (auto& key_frame : vmd.keyFrames)
	{
		auto node = boneNodeTable[key_frame.first];
		auto& pos = node.startPos;
		auto mat = DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* DirectX::XMMatrixRotationQuaternion(key_frame.second[0].quaternion)
			* DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		transform.boneMatrices[node.boneIdx] = mat;
	}
	RecursiveMatrixMultiply(&boneNodeTable["センター"], DirectX::XMMatrixIdentity());

	mappedMatrices[0] = transform.world;
	std::copy(transform.boneMatrices.begin(), transform.boneMatrices.end(), &mappedMatrices[1]);
#endif
}

void PMDRenderer::Render(ms::ComPtr<ID3D12Device> device, ms::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	cmdList->SetGraphicsRootSignature(rootSignature.Get());
	ID3D12DescriptorHeap* basic_heaps[] = { basicDescHeap.Get() };
	cmdList->SetDescriptorHeaps(1, basic_heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* transform_heaps[] = { transformDescHeap.Get() };
	cmdList->SetDescriptorHeaps(1, transform_heaps);
	cmdList->SetGraphicsRootDescriptorTable(1, transformDescHeap->GetGPUDescriptorHandleForHeapStart());

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
		cmdList->SetGraphicsRootDescriptorTable(2, materialHandle);
		cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialHandle.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}
