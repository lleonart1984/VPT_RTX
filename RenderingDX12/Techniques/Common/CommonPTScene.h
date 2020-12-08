/// Common header to any library that will use the scene retained for an accumulative path-tracing.

#include "CommonRTScene.h" // adds all 

cbuffer AccumulativeInfo : register(b0, space1) {
	int PassCount;
	int AccumulationIsComplexity;
}

RWTexture2D<float3> Accumulation : register(u1, space1);

#include "CommonComplexity.h"

void AccumulateOutput(uint2 coord, float3 value) {

	if (AccumulationIsComplexity) {

		float oldValue = Accumulation[coord].x;
		float currentValue = (value.x * 256 + value.y);
		//// Maximum complexity among frames
		//float newValue = max(oldValue, currentValue);

		// Average complexity among frames
		float newValue = (oldValue * PassCount + currentValue)/(PassCount + 1);

		Accumulation[coord] = float3(newValue, 0, 0);

		Output[coord] = GetColor((int)round(newValue));
	}
	else {
		Accumulation[coord] += value;
		Output[coord] = Accumulation[coord] / (PassCount + 1);
	}
}