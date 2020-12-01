

struct xorwow_state {
	uint a, b, c, d;
	uint counter;
};

shared static xorwow_state rng_state;

/* The state array must be initialized to not be all zero in the first four words */
uint xorwow()
{
	/* Algorithm "xorwow" from p. 5 of Marsaglia, "Xorshift RNGs" */
	uint t = rng_state.d;

	uint s = rng_state.a;
	rng_state.d = rng_state.c;
	rng_state.c = rng_state.b;
	rng_state.b = s;

	t ^= t >> 2;
	t ^= t << 1;
	t ^= s ^ (s << 4);
	rng_state.a = t;

	rng_state.counter += 362437;
	return t + rng_state.counter;
}

float random()
{
	return xorwow() / 4294967296.0;
}

//void initializeRandom(uint seed) {
//	if (seed == 0)
//		seed = 1002039242112;
//	rng_state.a = seed;// ^ 0xFEFE;
//	rng_state.b = seed;
//	rng_state.c = seed;
//	rng_state.d = seed;
//	rng_state.counter = 0;
//
//	/*[loop]
//	for (int i = 0; i < seed % 7 + 2; i++)
//		random();*/
//}


void initializeRandom(ULONG seed) {
	if (seed.l == 0)
		seed.l = 1002039242;
	rng_state.a = seed.l;// ^ 0xFEFE;
	rng_state.b = seed.l;
	rng_state.c = seed.l;
	rng_state.d = seed.l;
	rng_state.counter = 0;
}

ULONG getRandomSeed() {
	ULONG r = { rng_state.c, rng_state.d };
	return r;
}


