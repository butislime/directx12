#include "../header/pmd.h"
#include <stdio.h>
#include <iostream>
#include <Windows.h>
#include <algorithm>

//! テクスチャパスの変換
std::string GetTexturePathFromModelAndTexPath(
	const std::string& modelPath,
	const char* texPath
)
{
	int pathIndex1 = modelPath.rfind('/');
	int pathIndex2 = modelPath.rfind('\\');
	auto pathIndex = max(pathIndex1, pathIndex2);
	auto folderPath = modelPath.substr(0, pathIndex+1);
	return folderPath + texPath;
}

//! std::stringからstd::wstringへの変換
std::wstring GetWideStringFromString(const std::string& str)
{
	auto num1 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
	std::wstring wstr;
	wstr.resize(num1);
	auto num2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, &wstr[0], num1);
	assert(num1 == num2);
	return wstr;
}

//! ファイル名から拡張子を取得
std::string GetExtension(const std::string& path)
{
	int idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

//! テクスチャのパスをセパレータで分離
std::pair<std::string, std::string> SplitFileName(const std::string& path, const char splitter)
{
	int idx = path.find(splitter);
	std::pair<std::string, std::string> ret;
	ret.first = path.substr(0, idx);
	ret.second = path.substr(idx + 1, path.length() - idx - 1);
	return ret;
}

PMDHeader LoadPMDHeader(const std::string& path)
{
	char signature[3] = {}; // シグネチャ"pmd"

	FILE* fp;
	fopen_s(&fp, path.c_str(), "rb");

	auto header = PMDHeader();
	fread(signature, sizeof(signature), 1, fp);
	fread(&header, sizeof(header), 1, fp);

	fclose(fp);

	return header;
}

PMD LoadPMD(const std::string& path)
{
	char signature[3] = {}; // シグネチャ"pmd"

	FILE* fp;
	fopen_s(&fp, path.c_str(), "rb");

	PMD pmd;
	pmd.filePath = path;

	// header
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmd.header, sizeof(pmd.header), 1, fp);
	std::cout << "version:" << pmd.header.version << std::endl;
	std::cout << "model_name:" << pmd.header.model_name << std::endl;
	std::cout << "comment:" << pmd.header.comment << std::endl;

	fread(&pmd.vertNum, sizeof(pmd.vertNum), 1, fp);
	std::cout << "vertex num:" << pmd.vertNum << std::endl;
	size_t read_size = pmd.vertNum * pmdvertex_size;
	pmd.vertices.resize(read_size);
	fread(pmd.vertices.data(), read_size, 1, fp);

	unsigned int indices_num;
	fread(&indices_num, sizeof(indices_num), 1, fp);
	std::cout << "indices num:" << indices_num << std::endl;
	pmd.indices.resize(indices_num);
	fread(pmd.indices.data(), pmd.indices.size() * sizeof(pmd.indices[0]), 1, fp);

	unsigned int material_num;
	fread(&material_num, sizeof(material_num), 1, fp);
	std::cout << "material num:" << material_num << std::endl;
	pmd.materials.resize(material_num);
	fread(pmd.materials.data(), pmd.materials.size() * sizeof(PMDMaterial), 1, fp);

	unsigned short bone_num = 0;
	fread(&bone_num, sizeof(bone_num), 1, fp);
	std::cout << "bone num:" << bone_num << std::endl;
	pmd.bones.resize(bone_num);
	fread(pmd.bones.data(), sizeof(PMDBone), bone_num, fp);

	uint16_t ik_num = 0;
	fread(&ik_num, sizeof(ik_num), 1, fp);
	std::cout << "ik num:" << ik_num << std::endl;
	pmd.iks.resize(ik_num);
	for (auto& ik : pmd.iks)
	{
		fread(&ik.boneIdx, sizeof(ik.boneIdx), 1, fp);
		fread(&ik.targetIdx, sizeof(ik.targetIdx), 1, fp);

		uint8_t chain_len = 0;
		fread(&chain_len, sizeof(chain_len), 1, fp);
		ik.nodeIdxes.resize(chain_len);
		fread(&ik.iterations, sizeof(ik.iterations), 1, fp);
		fread(&ik.limit, sizeof(ik.limit), 1, fp);
		
		if (chain_len == 0)
			continue;
		fread(ik.nodeIdxes.data(), sizeof(ik.nodeIdxes[0]), chain_len, fp);
	}

	fclose(fp);

	return pmd;
}

VMD LoadVMD(const std::string& path)
{
	FILE* fp;
	fopen_s(&fp, path.c_str(), "rb");
	fseek(fp, 50, SEEK_SET); // 最初の50バイトは未使用

	VMD vmd;

	unsigned int motion_data_num = 0;
	fread(&motion_data_num, sizeof(motion_data_num), 1, fp);
	std::cout << "motion data num:" << motion_data_num << std::endl;

	vmd.motionData.resize(motion_data_num);
	for (auto& motion : vmd.motionData)
	{
		fread(motion.boneName, sizeof(motion.boneName), 1, fp);
		fread(&motion.frameNo,
			sizeof(motion.frameNo) +
			sizeof(motion.location) +
			sizeof(motion.quaternion) +
			sizeof(motion.bezier),
			1, fp);
	}

	for (auto& motion : vmd.motionData)
	{
		auto q = DirectX::XMLoadFloat4(&motion.quaternion);
		vmd.keyFrames[motion.boneName].emplace_back(KeyFrame(motion.frameNo, q, motion.location,
			DirectX::XMFLOAT2((float)motion.bezier[3] / 127.0f, (float)motion.bezier[7] / 127.0f),
			DirectX::XMFLOAT2((float)motion.bezier[11] / 127.0f, (float)motion.bezier[15] / 127.0f)));

		vmd.durationFrame = std::max<unsigned int>(vmd.durationFrame, motion.frameNo);
	}

	for (auto& kv : vmd.keyFrames)
	{
		std::sort(kv.second.begin(), kv.second.end(), [](const KeyFrame& lv, const KeyFrame& rv) {
			return lv.frameNo <= rv.frameNo;
		});
	}

	uint32_t morph_num = 0;
	fread(&morph_num, sizeof(morph_num), 1, fp);
	vmd.morphs.resize(morph_num);
	fread(vmd.morphs.data(), sizeof(VMDMorph), morph_num, fp);

	uint32_t camera_num = 0;
	fread(&camera_num, sizeof(camera_num), 1, fp);
	vmd.cameras.resize(camera_num);
	fread(vmd.cameras.data(), sizeof(VMDCamera), camera_num, fp);

	uint32_t light_num = 0;
	fread(&light_num, sizeof(light_num), 1, fp);
	vmd.lights.resize(light_num);
	fread(vmd.lights.data(), sizeof(VMDLight), light_num, fp);

	uint32_t self_shadow_num = 0;
	fread(&self_shadow_num, sizeof(self_shadow_num), 1, fp);
	vmd.selfShadows.resize(self_shadow_num);
	fread(vmd.selfShadows.data(), sizeof(VMDSelfShadow), self_shadow_num, fp);

	uint32_t ik_switch_num = 0;
	fread(&ik_switch_num, sizeof(ik_switch_num), 1, fp);
	vmd.ikEnables.resize(ik_switch_num);
	for (auto& ik_enable : vmd.ikEnables)
	{
		fread(&ik_enable.frameNo, sizeof(ik_enable.frameNo), 1, fp);

		uint8_t visible_flag = 0;
		fread(&visible_flag, sizeof(visible_flag), 1, fp);

		uint32_t ik_bone_count = 0;
		fread(&ik_bone_count, sizeof(ik_bone_count), 1, fp);

		for (int i = 0; i < ik_bone_count; ++i)
		{
			char ik_bone_name[20];
			fread(ik_bone_name, _countof(ik_bone_name), 1, fp);

			uint8_t flag = 0;
			fread(&flag, sizeof(flag), 1, fp);
			ik_enable.ikEnableTable[ik_bone_name] = flag;
		}
	}

	fclose(fp);

	return vmd;
}