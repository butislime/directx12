#include "../header/pmd.h"
#include <stdio.h>
#include <iostream>

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

	fclose(fp);

	return pmd;
}