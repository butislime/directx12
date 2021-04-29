#include <string>
#include <vector>
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

struct PMD
{
	PMDHeader header;
	//std::vector<PMDVertex> vertices;
	unsigned int vertNum;
	std::vector<unsigned char> vertices;
	std::vector<unsigned short> indices;
};

PMDHeader LoadPMDHeader(const std::string& path);
PMD LoadPMD(const std::string& path);