#include "../CommonGI/Definitions.h"
#include "../Randoms/RandomX.h"

static uint4 rng_state;

uint TausStep(uint z, int S1, int S2, int S3, uint M)
{
	uint b = (((z << S1) ^ z) >> S2);
	return ((z & M) << S3) ^ b;
}
uint LCGStep(uint z, uint A, uint C)
{
	return A * z + C;
}

float HybridTaus()
{
	rng_state.x = TausStep(rng_state.x, 13, 19, 12, 4294967294);
	rng_state.y = TausStep(rng_state.y, 2, 25, 4, 4294967288);
	rng_state.z = TausStep(rng_state.z, 3, 11, 17, 4294967280);
	rng_state.w = LCGStep(rng_state.w, 1664525, 1013904223);

	return 2.3283064365387e-10 * (rng_state.x ^ rng_state.y ^ rng_state.z ^ rng_state.w);
}

float random() {
	return HybridTaus();
}

void StartRandomSeedForRay(uint2 gridDimensions, int maxBounces, uint2 raysIndex, int bounce, int frame) {
	uint index = 0;
	uint dim = 1;
	index += raysIndex.x * dim;
	dim *= gridDimensions.x;
	index += raysIndex.y * dim;
	dim *= gridDimensions.y;
	index += bounce * dim;
	dim *= maxBounces;
	index += frame * dim;

	rng_state = uint4(index, index, index, index);

	for (int i = 0; i < 23 + index % 13; i++)
		random();
}

uint4 getRNG() {
	return rng_state;
}

void setRNG(uint4 state) {
	rng_state = state;
}