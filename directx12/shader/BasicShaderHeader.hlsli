struct Output
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1; // ビュー変換
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
};

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);
SamplerState smp : register(s0);
SamplerState smpToon : register(s1);
cbuffer cbuff0 : register(b0)
{
	matrix world; // ワールド変換行列
	matrix view; // ビュー行列
	matrix proj; // プロジェクション行列
	float3 eye; // カメラ座標
};
cbuffer Material : register(b1)
{
	float4 diffuse; // a=alpha
	float4 specular; // a=specularity
	float3 ambient;
};