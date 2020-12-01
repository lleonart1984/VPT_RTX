#pragma once

struct Globals {
	float4x4 projection;
	float4x4 view;
};

struct Locals {
	float4x4 world;
};

struct Lighting
{
	float3 LightPosition; float res0;
	float3 LightIntensity; float res1;
	float3 LightDirection; float res2;
};

struct Materialing {
	float3 Diffuse;
	float RefractionIndex;
	float3 Specular;
	float SpecularSharpness;
	int4 Texture_Mask;
	float3 Emissive; float res2;
	float4 Roulette;
};

/*
 * APIT Constants
 */

#define MAX_NUMBER_OF_FRAGMENTS 20000000 // up to 20 million of fragments supported

struct ScreenInfo {
	int Width;
	int Height;

	int Levels;
	float res0;
};

struct Fragment {
	int Index;
	float MinDepth;
	float MaxDepth;
};

struct PITNode {
	int Parent;
	int LeftChild;
	int RightChild;

	float Discriminant;
};

struct LevelInfo {
	int Length;

	int Level;

	float2 res;
};

struct RayInfo
{
	float3 Position;
	float3 Direction;
	float3 AccumulationFactor;
};

struct RayIntersection {
	int TriangleIndex;
	float3 Coordinates;
};

struct Reticulation {
	int K;
	int D;
};

struct ComputeShaderInfo {
	uint3 InputSize;
	float res;
};

struct MinMax {
	int3 min;
	int3 max;
};