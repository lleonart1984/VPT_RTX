// This includes vertex info and materials in space1, as well as acummulation buffers for a path-tracer
#include "..\Common\CommonPTScene.h" 

// Random using is HybridTaus
#include "..\Randoms\RandomUsed.h"

// Includes some functions for surface scattering and texture mapping
#include "..\Common\ScatteringTools.h"

#include "HGPhase.h"

// Generated code with the CVAE model for unitary sphere scattering
#include "CVAEScattering.h"

// Top level structure with the scene
RaytracingAccelerationStructure Scene	: register(t0, space0);

struct GridInfo {
	// Used to scale a distance from grid to world.
	float Grid2WorldScalling; 
	// Transform from world to grid space.
	// Each cell in grid space has 1 unit size
	float4x3 FromWorldToGrid; 
};

StructuredBuffer<GridInfo> GridInfos	: register(t1);
Texture3D<float> Grids[100]				: register(t2);

cbuffer Lighting : register(b0) {
	float3 LightPosition;
	float3 LightIntensity;
	float3 LightDirection;
}

#include "../Common/Environment.h"

cbuffer Transforming : register(b1) {
	row_major matrix FromProjectionToWorld;
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

/// Query the distance field grid.
float MaximalRadius(float3 P, int object) {

	GridInfo info = GridInfos[object];
	float3 positionInGrid = mul(float4(P, 1), info.FromWorldToGrid);
	float radius = Grids[object][positionInGrid];
	
	if (radius < 0) // no empty cell
		return 0;
	
	float3 distToMinCorner = positionInGrid % 1;
	float3 m = min(distToMinCorner, 1 - distToMinCorner);
	float minDistanceToCellBorder = min(m.x, min(m.y, m.z));
	float safeDistanceInGridSpace = minDistanceToCellBorder + radius;
	return safeDistanceInGridSpace * info.Grid2WorldScalling;
}

float sampleNormal(float mu, float logVar) {
	//return mu + gauss() * exp(logVar * 0.5);
	return mu + gauss() * exp(clamp(logVar, -10, 70) * 0.5);
}

bool GenerateVariablesWithModel(float G, float Phi, float3 win, float density, out float3 x, out float3 w)
{
	x = float3(0, 0, 0);
	w = win;

	float3 temp = abs(win.x) >= 0.9999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 winY = normalize(cross(temp, win));
	float3 winX = cross(win, winY);
	float rAlpha = random() * 2 * pi;
	float3x3 R = (mul(float3x3(
		cos(rAlpha), -sin(rAlpha), 0,
		sin(rAlpha), cos(rAlpha), 0,
		0, 0, 1), float3x3(winX, winY, win)));

	float codedDensity = density;// pow(density / 400.0, 0.125);

	float2 lenLatent = randomStdNormal2();
	// Generate length
	float lenInput[4];
	float lenOutput[2];
	lenInput[0] = codedDensity;
	lenInput[1] = G;
	lenInput[2] = lenLatent.x;
	lenInput[3] = lenLatent.y;
	lenModel(lenInput, lenOutput);

	float logN = max(0, sampleNormal(lenOutput[0], lenOutput[1]));
	float n = (exp(logN));
	//logN = log(n);

	if (random() >= pow(Phi, n))
		return false;

	float4 pathLatent14 = randomStdNormal4();
	float pathLatent5 = randomStdNormal();
	// Generate path
	float pathInput[8];
	float pathOutput[6];
	pathInput[0] = codedDensity;
	pathInput[1] = G;
	pathInput[2] = logN;
	pathInput[3] = pathLatent14.x;
	pathInput[4] = pathLatent14.y;
	pathInput[5] = pathLatent14.z;
	pathInput[6] = pathLatent14.w;
	pathInput[7] = pathLatent5.x;
	pathModel(pathInput, pathOutput);
	float3 sampling = randomStdNormal3();
	float3 pathMu = float3(pathOutput[0], pathOutput[1], pathOutput[2]);
	float3 pathLogVar = float3(pathOutput[3], pathOutput[4], pathOutput[5]);
	float3 pathOut = clamp(pathMu + exp(clamp(pathLogVar, -10, 70) * 0.5) * sampling, -0.9999, 0.9999);
	float costheta = pathOut.x;
	float wt = pathOut.y;
	float wb = pathOut.z;
	x = float3(0, sqrt(1 - costheta * costheta), costheta);
	float3 N = x;
	float3 B = float3(1, 0, 0);
	float3 T = cross(x, B);
	w = normalize(N * sqrt(max(0, 1 - wt * wt - wb * wb)) + T * wt + B * wb);
	x = mul(x, (R));
	w = mul(w, (R)); // move to radial space

	return true;// random() >= 1 - pow(Phi, n);
}

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

	[loop]
	while (true)
	{
		complexity++;

		int tIndex;
		int transformIndex;
		int mIndex;
		float3 coords;

		if (!Intersect(x, w, tIndex, transformIndex, mIndex, coords)) // 
		{
			// Uncomment this to check for geometry leaking.
			/*if (inMedium)
				return float3(1, 0, 1) * 100000; */
			
			return importance * (SampleSkybox(w) + SampleLight(w) * (bounces > 0));
		}

		Vertex surfel;
		Material material;
		VolumeMaterial volMaterial;
		GetHitInfo(coords, tIndex, transformIndex, mIndex, surfel, material, volMaterial, 0, 0);

		float d = length(surfel.P - x); // distance to the next mesh boundary

		float t = !inMedium || volMaterial.Extinction[cmp] == 0 ? 100000000 : -log(max(0.000000000001, 1 - random())) / volMaterial.Extinction[cmp];

		[branch]
		if (t >= d) // surface scattering
		{
			bounces += (!inMedium);
			
			if (bounces > 5)
				return 0;

			SurfelScattering(x, w, importance, surfel, material);

			if (any(material.Specular) && material.Roulette.w > 0) // some fresnel
				inMedium = dot(surfel.N, w) < 0;
		}
		else // volume scattering will occur
		{
			x += t * w; // free traverse in a medium
			//bool pathTrace = DispatchRaysIndex().x / (float)DispatchRaysDimensions().x < 0.5;
			//if (pathTrace)
			//{
			//	if (random() < 1 - volMaterial.ScatteringAlbedo[cmp]) // absorption instead
			//		return 0;

			//	w = ImportanceSamplePhase(volMaterial.G[cmp], w); // scattering event...
			//}
			//else
			{
				float r = MaximalRadius(x, transformIndex);

				float3 _x, _w, _X, _W;

				if (!GenerateVariablesWithModel(volMaterial.G[cmp], volMaterial.ScatteringAlbedo[cmp], w, volMaterial.Extinction[cmp] * r, _x, _w))
					return 0;

				w = _w;
				x += _x * r;
			}
		}
	}
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
		color = float3 (volBounces / 256, (volBounces % 256), 0);// // GetColor(volBounces);

	AccumulateOutput(raysIndex, color);
}