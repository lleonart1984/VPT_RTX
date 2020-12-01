
shared static ULONG rng_state;
shared static ULONG initialSeed;

/* The state array must be initialized to not be all zero in the first four words */
uint xorshift64s()
{
	if (rng_state.h == 0 && rng_state.l == 0)
	{
		rng_state.l = initialSeed.h;
		rng_state.h = initialSeed.l ^ (1 - initialSeed.h);
	}

	//if (rng_state.l == 0)
	//{
	//	rng_state.l = 0x1FF2B12;
	//}

	//x ^= x >> 12; // a
	rng_state = XOR(rng_state, RSHIFT(rng_state, 12));
	//x ^= x << 25; // b
	rng_state = XOR(rng_state, LSHIFT(rng_state, 25));
	//x ^= x >> 27; // c
	rng_state = XOR(rng_state, RSHIFT(rng_state, 27));

	ULONG c = { 0x2545F491, 0x4F6CDD1D };

	return MUL(rng_state, c).h;
}

float random()
{
	return xorshift64s() / 4294967296.0;
}

void initializeRandom(ULONG seed) {
	rng_state = seed;
	initialSeed = seed;
}

ULONG getRandomSeed() {
	return rng_state;
}

