/// Common header to any library that will use the scene retained for an accumulative path-tracing.

#include "CommonRTScene.h" // adds all 

cbuffer AccumulativeInfo : register(b0, space1) {
	int PassCount;
	int AccumulationIsComplexity;
}

RWTexture2D<float3> Accumulation : register(u1, space1);

#include "CommonComplexity.h"

void AccumulateOutput(uint2 coord, float3 value) {
	Accumulation[coord] += value;

	if (AccumulationIsComplexity) {

		float3 acc = Accumulation[coord] / (PassCount + 1);

		Output[coord] = GetColor((int)(acc.x * 256 + acc.y));
	}
	else {
		Output[coord] = Accumulation[coord] / (PassCount + 1);
	}
}