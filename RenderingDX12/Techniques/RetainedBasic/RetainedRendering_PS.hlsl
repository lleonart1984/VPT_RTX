struct PSInput
{
	float4 projected : SV_POSITION;
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
	int objectId : OBJECTID;
};

cbuffer Lighting : register(b0)
{
	float3 LightPosition;
	float3 LightIntensity;
};

struct Material
{
	float3 Diffuse;
	float RefractionIndex;
	float3 Specular;
	float SpecularSharpness;
	int4 Texture_Index;
	float3 Emissive;
	float4 Roulette;
};

StructuredBuffer<int> MaterialIndexBuffer : register(t0);
StructuredBuffer<Material> Materials : register(t1);
Texture2D<float4> Textures[500] : register(t2);

SamplerState Sampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{

	//return float4(input.normal, 1);
	float3 L = LightPosition - input.position;
	float d = length(L);
	L /= d;

	int materialIndex = MaterialIndexBuffer[input.objectId];
	Material material = Materials[materialIndex];

	float3 Lin = LightIntensity / (4 * 3.141596 * 3.14159*6*max(0.1, d * d));

	float4 DiffTex = material.Texture_Index.x >= 0 ? Textures[material.Texture_Index.x].Sample(Sampler, input.uv) : float4(1,1,1,1);
	float3 SpecularTex = material.Texture_Index.y >= 0 ? Textures[material.Texture_Index.y].Sample(Sampler, input.uv) : material.Specular;
	float3 BumpTex = material.Texture_Index.z >= 0 ? Textures[material.Texture_Index.z].Sample(Sampler, input.uv) : float3(0.5, 0.5, 1);
	float3 MaskTex = material.Texture_Index.w >= 0 ? Textures[material.Texture_Index.w].Sample(Sampler, input.uv) : 1;

	if (MaskTex.x < 0.5 || DiffTex.a < 0.5)
		clip(-1);

	float3x3 worldToTangent = { input.tangent, input.binormal, input.normal };

	float3 normal = normalize(mul(BumpTex * 2 - 1, worldToTangent));

	return float4(normal,1);
	return float4(exp(-length(cross(input.tangent, input.binormal)))*float3(1,1,1), 1);

	float3 V = normalize(-input.position);
	float3 H = normalize(V + L);

	float NdotL = max(0, dot(normal, L));
	float NdotH = NdotL < 0 ? 0 : max(0, dot(normal, H));

	float3 diff = material.Diffuse * DiffTex / 3.14159;
	float3 spec = max(material.Specular, SpecularTex) * pow(NdotH, material.SpecularSharpness)*(2 + material.SpecularSharpness)/(6.28);

	float3 light = (
		material.Roulette.x * diff * NdotL +
		material.Roulette.y * spec) * Lin;
	return float4(light, 1);
}