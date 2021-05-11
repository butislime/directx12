#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>

struct PMDHeader
{
	float version;
	char model_name[20];
	char comment[256];
};

struct PMDVertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
	unsigned short boneNo[2];
	unsigned char boneWeight;
	unsigned char edgeFlag;
};
constexpr size_t pmdvertex_size = 38;

#pragma pack(1)
struct PMDMaterial
{
	DirectX::XMFLOAT3 diffuse;
	float alpha;
	float specularity;
	DirectX::XMFLOAT3 specular;
	DirectX::XMFLOAT3 ambient;
	unsigned char toonIdx;
	unsigned char edgeFlag;
	unsigned int indicesNum;
	char texFilePath[20];
};
#pragma pack()

#pragma pack(1)
struct PMDBone
{
	char boneName[20];
	unsigned short parentNo;
	unsigned short nextNo;
	unsigned char type;
	unsigned short ikBoneNo;
	DirectX::XMFLOAT3 pos;
};
#pragma pack()

struct PMDIK
{
	uint16_t boneIdx;
	uint16_t targetIdx;
	uint16_t iterations;
	float limit;
	std::vector<uint16_t> nodeIdxes;
};

struct PMD
{
	std::string filePath;
	PMDHeader header;
	//std::vector<PMDVertex> vertices;
	unsigned int vertNum;
	std::vector<unsigned char> vertices;
	std::vector<unsigned short> indices;
	std::vector<PMDMaterial> materials;
	std::vector<PMDBone> bones;
	std::vector<PMDIK> iks;
};

struct VMDMotion
{
	char boneName[15];
	unsigned int frameNo;
	DirectX::XMFLOAT3 location;
	DirectX::XMFLOAT4 quaternion;
	unsigned char bezier[64];
};

struct KeyFrame
{
	unsigned int frameNo;
	DirectX::XMVECTOR quaternion;
	DirectX::XMFLOAT3 offset;
	DirectX::XMFLOAT2 p1, p2;

	KeyFrame(unsigned int fno, DirectX::XMVECTOR& q, DirectX::XMFLOAT3 ofst, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2)
		: frameNo(fno), quaternion(q), offset(ofst), p1(ip1), p2(ip2) {}
};

struct VMD
{
	std::vector<VMDMotion> motionData;
	std::unordered_map<std::string, std::vector<KeyFrame>> keyFrames;
	unsigned int durationFrame = 0;
};

std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath);
std::wstring GetWideStringFromString(const std::string& str);
std::string GetExtension(const std::string& path);
std::pair<std::string, std::string> SplitFileName(const std::string& path, const char splitter = '*');

PMDHeader LoadPMDHeader(const std::string& path);
PMD LoadPMD(const std::string& path);
VMD LoadVMD(const std::string& path);