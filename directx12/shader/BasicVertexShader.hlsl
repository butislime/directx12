#include "BasicShaderHeader.hlsli"

Output BasicVS(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONE_NO,
	min16uint weight : WEIGHT)
{
	Output output;
	pos = mul(bones[boneno[0]], pos);
	output.pos = mul(world, pos);
	output.svpos = mul(mul(proj, view), output.pos);
	output.normal.w = 0; // ïΩçsà⁄ìÆÇ»Çµ
	output.normal = mul(world, normal);
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	output.ray = normalize(output.pos.xyz - eye);
	return output;
}