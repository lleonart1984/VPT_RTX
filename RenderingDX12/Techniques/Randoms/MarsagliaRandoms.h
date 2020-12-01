shared static uint rng_state;

uint rand_xorshift()
{
	// Xorshift algorithm from George Marsaglia's paper
	rng_state ^= (rng_state << 13);
	rng_state ^= (rng_state >> 17);
	rng_state ^= (rng_state << 5);
	return rng_state;
}

float random()
{
	return rand_xorshift() * (1.0 / 4294967296.0);
}

void initializeRandom(uint seed) {
	rng_state = seed;
	/*[loop]
	for (int i = 0; i < seed % 7 + 2; i++)
		random();*/
}