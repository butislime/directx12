#pragma once 

#include <pmd.h>
#include <d3d12.h>
#include <map>

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

struct BoneNode
{
	int boneIdx;
	DirectX::XMFLOAT3 startPos;
	DirectX::XMFLOAT3 endPos;
	std::vector<BoneNode*> children;
};

struct Transform
{
	DirectX::XMMATRIX world;
	std::vector<DirectX::XMMATRIX> boneMatrices;
	static unsigned int CalcAlignmentedSize(const Transform& transform)
	{
		unsigned int size = sizeof(DirectX::XMMATRIX) + sizeof(DirectX::XMMATRIX) * transform.boneMatrices.size();
		return size + 0xff & ~0xff;
	}

	void* operator new(size_t size);
};

class PMDActor
{
public:
	void Init();
	void Update();

	const PMD& GetPMD() const { return pmd; }
	const VMD& GetVMD() const { return vmd; }
	const Transform& GetTransform() const { return transform; }
	const std::vector<Material>& GetMaterials() const { return materials; }
	const std::map<std::string, BoneNode>& const GetBoneNodeTable() { return boneNodeTable; }

private:
	void RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat);

private:
	PMD pmd;
	VMD vmd;

	Transform transform;

	std::vector<Material> materials;
	std::map<std::string, BoneNode> boneNodeTable;

	static const unsigned short BoneMax = 256;
};
