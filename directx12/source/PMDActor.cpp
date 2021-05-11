#include <PMDActor.h>
#include <Windows.h>
#pragma comment(lib, "winmm.lib")

#include <iostream>

namespace
{
	enum class BoneType
	{
		Rotation,      // 回転
		RotAndMove,    // 回転&移動
		IK,            // IK
		Undefined,     // 未定義
		IKChild,       // IK影響ボーン
		RotationChild, // 回転影響ボーン
		IKDestination, // IK接続先
		Invisible,     // 見えないボーン
	};
}

void* Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
}

void PMDActor::Init()
{
	static std::string strModelPath = "model/初音ミク.pmd";
	static std::string strMotionPath = "motion/swing2.vmd";
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
	boneNameArray.resize(pmd.bones.size());
	boneNodeAddressArray.resize(pmd.bones.size());
	for (int idx = 0; idx < pmd.bones.size(); ++idx)
	{
		auto& pb = pmd.bones[idx];
		bone_names[idx] = pb.boneName;
		auto& node = boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;

		boneNameArray[idx] = pb.boneName;
		boneNodeAddressArray[idx] = &node;
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

	// ikデバッグ表示
	auto get_name_from_idx = [&](uint16_t idx) -> std::string
	{
		auto it = std::find_if(boneNodeTable.begin(), boneNodeTable.end(), [idx](const std::pair<std::string, BoneNode>& obj) {
			return obj.second.boneIdx == idx;
		});
		if (it != boneNodeTable.end())
			return it->first;
		else
			return std::string();
	};
	for (auto& ik : pmd.iks)
	{
		std::cout << "IKボーン番号=" << ik.boneIdx << ":" << get_name_from_idx(ik.boneIdx) << std::endl;
		for (auto& node : ik.nodeIdxes)
		{
			std::cout << "\tノードボーン=" << node << ":" << get_name_from_idx(node) << std::endl;
		}
	}
}

void PMDActor::Update()
{
	MotionUpdate();
}

void PMDActor::PlayAnimation()
{
	startTime = timeGetTime();
}

void PMDActor::RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat)
{
	transform.boneMatrices[node->boneIdx] *= mat;
	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultiply(cnode, transform.boneMatrices[node->boneIdx]);
	}
}

void PMDActor::MotionUpdate()
{
	elapsedTime = timeGetTime() - startTime;
	unsigned int frame_no = 30 * (elapsedTime / 1000.0f);

	if (frame_no > vmd.durationFrame)
	{
		startTime = timeGetTime();
		frame_no = 0;
	}

	std::fill(transform.boneMatrices.begin(), transform.boneMatrices.end(), DirectX::XMMatrixIdentity());

	for (auto& key_val : vmd.keyFrames)
	{
		auto node = boneNodeTable[key_val.first];
		auto motions = key_val.second;
		auto rit = std::find_if(motions.rbegin(), motions.rend(), [frame_no](const KeyFrame& kf) {
			return kf.frameNo <= frame_no;
		});
		if (rit == motions.rend())
			continue;

		DirectX::XMMATRIX rotation;
		auto it = rit.base();
		if (it != motions.end())
		{
			auto t = static_cast<float>(frame_no - rit->frameNo) / static_cast<float>(it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);
			rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
		}
		else
		{
			rotation = DirectX::XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* rotation
			* DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
		transform.boneMatrices[node.boneIdx] = mat;
	}
	RecursiveMatrixMultiply(&boneNodeTable["センター"], DirectX::XMMatrixIdentity());
}

float PMDActor::GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y)
		return x;

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;
	const float k1 = 3 * b.x - 6 * a.x;
	const float k2 = 3 * a.x;

	constexpr float epsilon = 0.0005f;

	// tを近似
	for (int i = 0; i < n; ++i)
	{
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;
		if (ft <= epsilon && ft >= -epsilon)
			break;
		t -= ft / 2;
	}
	auto r = 1 - t;
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y;
}

void PMDActor::IKSolve()
{
	for (auto& ik : pmd.iks)
	{
		auto child_node_count = ik.nodeIdxes.size();

		switch (child_node_count)
		{
		case 0:
			// ありえない
			assert(0);
			continue;
		case 1:
			SolveLookAt(ik);
			break;
		case 2:
			SolveCosineIK(ik);
			break;
		default:
			SolveCCDIK(ik);
			break;
		}
	}
}

//! Z軸を特定の方向に向ける
DirectX::XMMATRIX LookAtMatrix(const DirectX::XMVECTOR& lookat, DirectX::XMFLOAT3& up, DirectX::XMFLOAT3& right)
{
	using namespace DirectX;
	XMVECTOR vz = lookat;
	// 仮のY軸
	XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));
	XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vy, vz));
	// zとxからy軸を計算
	vy = XMVector3Normalize(XMVector3Cross(vz, vx));

	// 同じ方向を向いてしまっていたらright基準で作り直し
	if (std::abs(XMVector3Dot(vy, vz).m128_f32[0]) == 1.0f)
	{
		vx = XMVector3Normalize(XMLoadFloat3(&right));
		vy = XMVector3Normalize(XMVector3Cross(vz, vx));
		vx = XMVector3Normalize(XMVector3Cross(vy, vz));
	}

	XMMATRIX ret = XMMatrixIdentity();
	ret.r[0] = vx;
	ret.r[1] = vy;
	ret.r[2] = vz;
	return ret;
}
//! 特定のベクトルを特定の方向に向ける
DirectX::XMMATRIX LookAtMatrix(const DirectX::XMVECTOR& origin, const DirectX::XMVECTOR& lookat, DirectX::XMFLOAT3& up, DirectX::XMFLOAT3& right)
{
	return DirectX::XMMatrixTranspose(LookAtMatrix(origin, up, right))
		* LookAtMatrix(lookat, up, right);
}

void PMDActor::SolveLookAt(const PMDIK& ik)
{
	auto root_node = boneNodeAddressArray[ik.nodeIdxes[0]];
	auto target_node = boneNodeAddressArray[ik.boneIdx];

	using namespace DirectX;
	auto rpos1 = XMLoadFloat3(&root_node->startPos);
	auto tpos1 = XMLoadFloat3(&target_node->startPos);

	auto rpos2 = XMVector3TransformCoord(rpos1, transform.boneMatrices[ik.nodeIdxes[0]]);
	auto tpos2 = XMVector3TransformCoord(tpos1, transform.boneMatrices[ik.boneIdx]);

	auto origin_vec = XMVectorSubtract(tpos1, rpos1);
	auto target_vec = XMVectorSubtract(tpos2, rpos2);

	origin_vec = XMVector3Normalize(origin_vec);
	target_vec = XMVector3Normalize(target_vec);
	auto up = XMFLOAT3(0, 1, 0);
	auto right = XMFLOAT3(1, 0, 0);
	transform.boneMatrices[ik.nodeIdxes[0]] = LookAtMatrix(origin_vec, target_vec, up, right);
}
void PMDActor::SolveCosineIK(const PMDIK& ik)
{
}
void PMDActor::SolveCCDIK(const PMDIK& ik)
{
}
