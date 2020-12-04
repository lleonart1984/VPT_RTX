
/// Common header to any library that will use the scene retained as follow.
/// A buffer with all vertices at VERTICES_REG
/// A buffer with all materials at MATERIALS_REG
/// A texture array with all textures available at TEXTURES_REG
/// A local constant buffer referring for each geometry to start triangle and material index at OBJECT_CB_REG

/// Provides functions to get intersection information
/// Augment material with texture mapping
/// Augment material and surfel with texture mapping (including bump mapping)

#include "../CommonGI/Definitions.h"

StructuredBuffer<Vertex> vertices				: register(t0, space1);
StructuredBuffer<float4x3> transforms			: register(t1, space1);
StructuredBuffer<Material> materials			: register(t2, space1);
StructuredBuffer<VolumeMaterial> volMaterials	: register(t3, space1);
// Textures													   
Texture2D<float4> Textures[500]					: register(t4, space1);

RWTexture2D<float3> Output						: register(u0, space1); // Final Output image from the ray-trace

// Used for texture mapping
SamplerState gSmp : register(s0, space1);

// Given a surfel will modify the normal with texture maps, using
// Bump mapping and masking textures.
// Material info is updated as well.
void AugmentHitInfoWithTextureMapping(inout Vertex surfel, inout Material material, float ddx, float ddy) {
	float4 DiffTex = material.Texture_Index.x >= 0 ? Textures[material.Texture_Index.x].SampleGrad(gSmp, surfel.C, ddx, ddy) : float4(1, 1, 1, 1);
	float3 SpecularTex = material.Texture_Index.y >= 0 ? Textures[material.Texture_Index.y].SampleGrad(gSmp, surfel.C, ddx, ddy) : material.Specular;
	float3 BumpTex = material.Texture_Index.z >= 0 ? Textures[material.Texture_Index.z].SampleGrad(gSmp, surfel.C, ddx, ddy) : float3(0.5, 0.5, 1);
	float3 MaskTex = material.Texture_Index.w >= 0 ? Textures[material.Texture_Index.w].SampleGrad(gSmp, surfel.C, ddx, ddy) : 1;

	float3x3 TangentToWorld = { surfel.T, surfel.B, surfel.N };
	// Change normal according to bump map
	surfel.N = normalize(mul(BumpTex * 2 - 1, TangentToWorld));

	material.Diffuse *= DiffTex * MaskTex.x; // set transparent if necessary.
	material.Specular.xyz = max(material.Specular.xyz, SpecularTex);
}

// Given a surfel will modify the material using texture mapping.
void AugmentMaterialWithTextureMapping(inout Vertex surfel, inout Material material, float ddx, float ddy) {
	float4 DiffTex = material.Texture_Index.x >= 0 ? Textures[material.Texture_Index.x].SampleGrad(gSmp, surfel.C, ddx, ddy) : float4(1, 1, 1, 1);
	float3 SpecularTex = material.Texture_Index.y >= 0 ? Textures[material.Texture_Index.y].SampleGrad(gSmp, surfel.C, ddx, ddy).xyz : material.Specular;
	float3 BumpTex = material.Texture_Index.z >= 0 ? Textures[material.Texture_Index.z].SampleGrad(gSmp, surfel.C, ddx, ddy).xyz : float3(0.5, 0.5, 1);
	float3 MaskTex = material.Texture_Index.w >= 0 ? Textures[material.Texture_Index.w].SampleGrad(gSmp, surfel.C, ddx, ddy).xyz : 1;

	material.Diffuse *= DiffTex * MaskTex.x; // set transparent if necessary.
	material.Specular.xyz = max(material.Specular.xyz, SpecularTex);
}

void GetHitInfo(float3 barycentrics, uint triangleIndex, int transformIndex, int materialIndex, out Vertex surfel, out Material material, out VolumeMaterial volMaterial, float ddx, float ddy)
{
	Vertex v1 = vertices[triangleIndex * 3 + 0];
	Vertex v2 = vertices[triangleIndex * 3 + 1];
	Vertex v3 = vertices[triangleIndex * 3 + 2];
	Vertex s = {
		v1.P * barycentrics.x + v2.P * barycentrics.y + v3.P * barycentrics.z,
		v1.N * barycentrics.x + v2.N * barycentrics.y + v3.N * barycentrics.z,
		v1.C * barycentrics.x + v2.C * barycentrics.y + v3.C * barycentrics.z,
		v1.T * barycentrics.x + v2.T * barycentrics.y + v3.T * barycentrics.z,
		v1.B * barycentrics.x + v2.B * barycentrics.y + v3.B * barycentrics.z
	};

	surfel = Transform(s, transforms[transformIndex]);

	material = materials[materialIndex];

	volMaterial = volMaterials[materialIndex];

	AugmentHitInfoWithTextureMapping(surfel, material, ddx, ddy);
}
