struct Output
{
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);
cbuffer cbuff0 : register(b0)
{
	matrix world; // ワールド変換行列
	matrix viewProj; // ビュープロジェクション行列
};
cbuffer Material : register(b1)
{
	float4 diffuse; // a=alpha
	float4 specular; // a=specularity
	float3 ambient;
};