#include "BasicShaderHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
	float4 color = tex.Sample(smp, input.uv);

	float3 light = normalize(float3(1, -1, 1));
	float brightness = dot(-light, input.normal.xyz);

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);

	return max(diffuse * brightness * color
		* sph.Sample(smp, sphereMapUV)
		+ spa.Sample(smp, sphereMapUV) * color
		+ float4(specularB * specular.rgb, 1)
		, float4(ambient * color, 1));
}
