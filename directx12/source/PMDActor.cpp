#include <PMDActor.h>
#include <Windows.h>
#pragma comment(lib, "winmm.lib")

#include <iostream>
#include <array>

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
	static std::string strMotionPath = "motion/squat.vmd";
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
	kneeIdxes.clear();
	for (int idx = 0; idx < pmd.bones.size(); ++idx)
	{
		auto& pb = pmd.bones[idx];
		bone_names[idx] = pb.boneName;
		auto& node = boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;

		boneNameArray[idx] = pb.boneName;
		boneNodeAddressArray[idx] = &node;

		std::string bone_name = pb.boneName;
		if (bone_name.find("ひざ") != std::string::npos)
		{
			kneeIdxes.emplace_back(idx);
		}
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

	std::cout << "frame=" << frame_no << " duration=" << vmd.durationFrame << std::endl;
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

	// ik
	IKSolve();
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
	using namespace DirectX;
	std::vector<XMVECTOR> positions; // ik点
	std::array<float, 2> edge_lens; // ボーン間距離

	auto& target_node = boneNodeAddressArray[ik.boneIdx];
	auto target_pos = XMVector3Transform(XMLoadFloat3(&target_node->startPos), transform.boneMatrices[ik.boneIdx]);

	// 末端
	auto end_node = boneNodeAddressArray[ik.targetIdx];
	positions.emplace_back(XMLoadFloat3(&end_node->startPos));
	for (auto& chain_bone_idx : ik.nodeIdxes)
	{
		auto bone_node = boneNodeAddressArray[chain_bone_idx];
		positions.emplace_back(XMLoadFloat3(&bone_node->startPos));
	}
	// ルートからの昇順にする
	std::reverse(positions.begin(), positions.end());

	edge_lens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edge_lens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	positions[0] = XMVector3Transform(positions[0], transform.boneMatrices[ik.nodeIdxes[1]]);
	positions[2] = XMVector3Transform(positions[2], transform.boneMatrices[ik.boneIdx]);

	auto linear_vec = XMVectorSubtract(positions[2], positions[0]);
	float A = XMVector3Length(linear_vec).m128_f32[0];
	float B = edge_lens[0];
	float C = edge_lens[1];

	linear_vec = XMVector3Normalize(linear_vec);

	// ルートから中間への角度計算
	float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));
	// 中間からターゲットへの角度計算
	float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	// 軸を求める
	XMVECTOR axis;
	if (std::find(kneeIdxes.begin(), kneeIdxes.end(), ik.nodeIdxes[0]) == kneeIdxes.end())
	{
		auto vm = XMVector3Normalize(XMVectorSubtract(positions[2], positions[0]));
		auto vt = XMVector3Normalize(XMVectorSubtract(target_pos, positions[0]));
		axis = XMVector3Cross(vt, vm);
	}
	else
	{
		// 膝の場合はx軸
		auto right = XMFLOAT3(1, 0, 0);
		axis = XMLoadFloat3(&right);
	}

	auto mat1 = XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= XMMatrixRotationAxis(axis, theta1);
	mat1 *= XMMatrixTranslationFromVector(positions[0]);

	auto mat2 = XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= XMMatrixRotationAxis(axis, theta2 - XM_PI);
	mat2 *= XMMatrixTranslationFromVector(positions[1]);

	transform.boneMatrices[ik.nodeIdxes[1]] *= mat1;
	transform.boneMatrices[ik.nodeIdxes[0]] = mat2 * transform.boneMatrices[ik.nodeIdxes[1]];
	transform.boneMatrices[ik.targetIdx] = transform.boneMatrices[ik.nodeIdxes[0]];
}
void PMDActor::SolveCCDIK(const PMDIK& ik)
{
	using namespace DirectX;
	auto target_bone_node = boneNodeAddressArray[ik.boneIdx];
	auto target_origin_pos = XMLoadFloat3(&target_bone_node->startPos);

	auto parent_mat = transform.boneMatrices[boneNodeAddressArray[ik.boneIdx]->ikParentBone];
	XMVECTOR det;
	auto inv_parent_mat = XMMatrixInverse(&det, parent_mat);
	auto target_next_pos = XMVector3Transform(target_origin_pos, transform.boneMatrices[ik.boneIdx] * inv_parent_mat);

	// 末端位置
	auto end_pos = XMLoadFloat3(&boneNodeAddressArray[ik.targetIdx]->startPos);
	// 中間
	std::vector<XMVECTOR> bone_positions;
	for (auto& cidx : ik.nodeIdxes)
	{
		bone_positions.push_back(XMLoadFloat3(&boneNodeAddressArray[cidx]->startPos));
	}
	std::vector<XMMATRIX> mats(bone_positions.size());
	std:fill(mats.begin(), mats.end(), XMMatrixIdentity());

	// pmd特有
	const auto ik_limit = ik.limit * XM_PI;
	constexpr float epsilon = 0.0005f;
	// 試行回数分実行
	for (int c = 0; c < ik.iterations; ++c)
	{
		// ターゲットと末端がほぼ一致したら終わり
		if (XMVector3Length(XMVectorSubtract(end_pos, target_next_pos)).m128_f32[0] <= epsilon)
		{
			break;
		}

		// 角度制限以内で近づけていく
		for (int bidx = 0; bidx < bone_positions.size(); ++bidx)
		{
			const auto& pos = bone_positions[bidx];
			auto vec_to_end = XMVectorSubtract(end_pos, pos); // 末端へ
			auto vec_to_target = XMVectorSubtract(target_next_pos, pos); // ターゲットへ
			vec_to_end = XMVector3Normalize(vec_to_end);
			vec_to_target = XMVector3Normalize(vec_to_target);

			// 外積できないので次へ
			if (XMVector3Length(XMVectorSubtract(vec_to_end, vec_to_target)).m128_f32[0] < epsilon)
			{
				continue;
			}

			auto cross = XMVector3Normalize(XMVector3Cross(vec_to_end, vec_to_target));
			auto angle = XMVector3AngleBetweenVectors(vec_to_end, vec_to_target).m128_f32[0];
			angle = std::min<float>(angle, ik_limit); // 制限
			XMMATRIX rot = XMMatrixRotationAxis(cross, angle);

			auto mat = XMMatrixTranslationFromVector(-pos)
				* rot
				* XMMatrixTranslationFromVector(pos);
			mats[bidx] *= mat;
			// 現在のボーン以下を回転
			for (auto idx = bidx - 1; idx >= 0; --idx)
			{
				bone_positions[idx] = XMVector3Transform(bone_positions[idx], mat);
			}

			end_pos = XMVector2Transform(end_pos, mat);

			// ターゲットと末端がほぼ一致したら終わり
			if (XMVector3Length(XMVectorSubtract(end_pos, target_next_pos)).m128_f32[0] <= epsilon)
			{
				break;
			}
		}
	}

	int idx = 0;
	for (auto& cidx : ik.nodeIdxes)
	{
		transform.boneMatrices[cidx] = mats[idx];
		++idx;
	}

	// 適用
	auto root_node = boneNodeAddressArray[ik.nodeIdxes.back()];
	RecursiveMatrixMultiply(root_node, parent_mat);
}
