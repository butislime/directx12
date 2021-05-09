#include <PMDActor.h>

void* Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
}

void PMDActor::Init()
{
	static std::string strModelPath = "model/初音ミク.pmd";
	static std::string strMotionPath = "motion/pose.vmd";
	pmd = LoadPMD(strModelPath);
	vmd = LoadVMD(strMotionPath);

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

	transform.world = DirectX::XMMatrixIdentity();
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

	// pose
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
}

void PMDActor::Update()
{
#if false
	transform.world = DirectX::XMMatrixIdentity();

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
#endif

}

void PMDActor::RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat)
{
	transform.boneMatrices[node->boneIdx] *= mat;
	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultiply(cnode, transform.boneMatrices[node->boneIdx]);
	}
}
