
// Top level structure with the scene
RaytracingAccelerationStructure Scene : register(t0, space0);

cbuffer Lighting : register(b0) {
	float3 LightPosition;
	float3 LightIntensity;
	float3 LightDirection;
}

cbuffer Transforming : register(b1) {
	row_major matrix FromProjectionToWorld; // Matrix used to convert rays from projection space to world.
}

struct ObjInfo {
	int TriangleOffset;
	int TransformIndex;
	int MaterialIndex;
};
// Locals for hit groups (fresnel and lambert)
ConstantBuffer<ObjInfo> objectInfo		: register(b2);

struct RayPayload // Only used for raycasting
{
	int TriangleIndex;
	int TransformIndex;
	int MaterialIndex;
	float3 Barycentric;
};

// This includes vertex info and materials in space1, as well as acummulation buffers for a path-tracer
#include "..\Common\CommonPTScene.h" 

// Random using is HybridTaus
#include "..\Randoms\RandomUsed.h"

// Includes some functions for surface scattering and texture mapping
#include "..\Common\ScatteringTools.h"

float3 SampleSkybox(float3 L) {

	//return float3(0, 0, 1);
	float3 BG_COLORS[5] =
	{
		float3(0.00f, 0.0f, 0.1f), // GROUND DARKER BLUE
		float3(0.01f, 0.05f, 0.2f), // HORIZON GROUND DARK BLUE
		float3(0.7f, 0.9f, 1.0f), // HORIZON SKY WHITE
		float3(0.1f, 0.3f, 1.0f),  // SKY LIGHT BLUE
		float3(0.01f, 0.1f, 0.7f)  // SKY BLUE
	};

	float BG_DISTS[5] =
	{
		-1.0f,
		-0.04f,
		0.0f,
		0.5f,
		1.0f
	};

	int N = 40;
	float phongNorm = (N + 2) / (4 * pi);

	float3 col = BG_COLORS[0];
	//for (int i = 1; i < 5; i++)
	col = lerp(col, BG_COLORS[1], smoothstep(BG_DISTS[0], BG_DISTS[1], L.y));
	col = lerp(col, BG_COLORS[2], smoothstep(BG_DISTS[1], BG_DISTS[2], L.y));
	col = lerp(col, BG_COLORS[3], smoothstep(BG_DISTS[2], BG_DISTS[3], L.y));
	col = lerp(col, BG_COLORS[4], smoothstep(BG_DISTS[3], BG_DISTS[4], L.y));
	return 0;// col;
}

float3 SampleLight(float3 L)
{
	int N = 40;
	float phongNorm = (N + 2) / (4 * pi);
	return pow(max(0, dot(L, LightDirection)), N) * phongNorm * LightIntensity;
}

#include "HGPhase.h"

/// Use RTX TraceRay to detect a single intersection. No recursion is necessary
bool Intersect(float3 P, float3 D, out int tIndex, out int transformIndex, out int mIndex, out float3 barycenter) {
	RayPayload payload = (RayPayload)0;
	RayDesc ray;
	ray.Origin = P;
	ray.Direction = D;
	ray.TMin = 0;
	ray.TMax = 100.0;
	TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 1, 0, ray, payload);
	tIndex = payload.TriangleIndex;
	transformIndex = payload.TransformIndex;
	mIndex = payload.MaterialIndex;
	barycenter = payload.Barycentric;
	return tIndex >= 0;
}

// Represents a single bounce of path tracing
// Will accumulate emissive and direct lighting modulated by the carrying importance
// Will update importance with scattered ratio divided by pdf
// Will output scattered ray to continue with
void SurfelScattering(inout float3 x, inout float3 w, inout float3 importance, Vertex surfel, Material material)
{
	float3 V = -w;

	float NdotV;
	bool invertNormal;
	float3 fN;
	float4 R, T;
	ComputeImpulses(V, surfel, material,
		NdotV,
		invertNormal,
		fN,
		R,
		T);

	float3 ratio;
	float3 direction;
	float pdf;
	RandomScatterRay(V, fN, R, T, material, ratio, direction, pdf);

	// Update gathered Importance to the viewer
	importance *= max(0, ratio);// / (1 - russianRoulette);
	// Update scattered ray
	w = direction;
	x = surfel.P + sign(dot(direction, fN)) * 0.0001 * fN;
}

float3 ComputePath(float3 O, float3 D, inout int complexity)
{
	int cmp = PassCount % 3;
	float3 importance = 0;
	importance[cmp] = 3;
	float3 x = O;
	float3 w = D;

	bool inMedium = false;

	int bounces = 0;

	while (importance[cmp] > 0)
	{
		complexity++;

		int tIndex;
		int transformIndex;
		int mIndex;
		float3 coords;
		if (!Intersect(x, w, tIndex, transformIndex, mIndex, coords)) // 
			return importance * (SampleSkybox(w) + SampleLight(w) * (bounces > 0));

		Vertex surfel;
		Material material;
		VolumeMaterial volMaterial;
		GetHitInfo(coords, tIndex, transformIndex, mIndex, surfel, material, volMaterial, 0, 0);

		float d = length(surfel.P - x); // distance to the next mesh boundary

		float t = !inMedium || volMaterial.Extinction[cmp] == 0 ? 100000000 : -log(max(0.000000000001, 1 - random())) / volMaterial.Extinction[cmp];

		if (t > d) // surface scattering
		{
			if (!inMedium)
				bounces++;

			if (bounces > 5)
				importance = 0;

			SurfelScattering(x, w, importance, surfel, material);

			if (any(material.Specular) && material.Roulette.w > 0) // some fresnel
				inMedium = dot(surfel.N, w) < 0;
		}
		else // volume scattering will occur
		{
			x += t * w; // free traverse in a medium

			if (random() < 1 - volMaterial.ScatteringAlbedo[cmp]) // absorption instead
				importance = 0;

			w = ImportanceSamplePhase(volMaterial.G[cmp], w); // scattering event...
		}
	}

	return 0;
}


[shader("closesthit")]
void PTScattering(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
	payload.Barycentric = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	payload.TriangleIndex = objectInfo.TriangleOffset + PrimitiveIndex();
	payload.TransformIndex = objectInfo.TransformIndex;
	payload.MaterialIndex = objectInfo.MaterialIndex;
}

[shader("miss")]
void EnvironmentMap(inout RayPayload payload)
{
	payload.TriangleIndex = -1;
}

[shader("raygeneration")]
void PTMainRays()
{
	uint2 raysIndex = DispatchRaysIndex();
	uint2 raysDimensions = DispatchRaysDimensions();
	StartRandomSeedForRay(raysDimensions, 1, raysIndex, 0, PassCount);

	float2 coord = (raysIndex.xy + float2(random(), random())) / raysDimensions;

	float4 ndcP = float4(2 * coord - 1, 0, 1);
	ndcP.y *= -1;
	float4 ndcT = ndcP + float4(0, 0, 1, 0);

	float4 viewP = mul(ndcP, FromProjectionToWorld);
	viewP.xyz /= viewP.w;
	float4 viewT = mul(ndcT, FromProjectionToWorld);
	viewT.xyz /= viewT.w;

	float3 O = viewP.xyz;
	float3 D = normalize(viewT.xyz - viewP.xyz);

	int volBounces = 0;

	float3 color = ComputePath(O, D, volBounces);

	if (any(isnan(color)))
		color = float3(0, 0, 0);

	if (AccumulationIsComplexity)
		color = float3 (volBounces / 256.0, (volBounces % 256) / 256.0, 0);// // GetColor(volBounces);

	AccumulateOutput(raysIndex, color);
}
