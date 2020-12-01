

float random();

float3 randomHSDirection(float3 N, out float NdotD)
{
	float r1 = random();
	float r2 = random() * 2 - 1;
	float sqrR2 = r2 * r2;
	float two_pi_by_r1 = two_pi * r1;
	float sqrt_of_one_minus_sqrR2 = sqrt(1.0 - sqrR2);
	float x = cos(two_pi_by_r1) * sqrt_of_one_minus_sqrR2;
	float y = sin(two_pi_by_r1) * sqrt_of_one_minus_sqrR2;
	float z = r2;
	float3 d = float3(x, y, z);
	NdotD = dot(N, d);
	d *= (NdotD < 0 ? -1 : 1);
	NdotD *= NdotD < 0 ? -1 : 1;
	return d;
}

void CreateOrthonormalBasis2(float3 D, out float3 B, out float3 T) {
	float3 other = abs(D.z) >= 0.999 ? float3(1, 0, 0) : float3(0, 0, 1);
	B = normalize(cross(other, D));
	T = normalize(cross(D, B));
}


float3 randomDirection(float3 D) {
	float r1 = random();
	float r2 = random() * 2 - 1;
	float sqrR2 = r2 * r2;
	float two_pi_by_r1 = two_pi * r1;
	float sqrt_of_one_minus_sqrR2 = sqrt(1.0 - sqrR2);
	float x = cos(two_pi_by_r1) * sqrt_of_one_minus_sqrR2;
	float y = sin(two_pi_by_r1) * sqrt_of_one_minus_sqrR2;
	float z = r2;

	float3 t0, t1;
	CreateOrthonormalBasis2(D, t0, t1);

	return t0 * x + t1 * y + D * z;
}

float3 randomDirectionWrong()
{
	while (true)
	{
		float3 x = float3(random(), random(), random()) * 2 - 1;
		float lsqr = dot(x, x);
		float s;
		if (lsqr > 0.01 && lsqr < 1)
			return normalize(x);// randomHSDirection(x / sqrt(lsqr), s);
	}
	/*
	float r1 = random();
	float r2 = random() * 2 - 1;
	float sqrR2 = r2 * r2;
	float two_pi_by_r1 = two_pi * r1;
	float sqrt_of_one_minus_sqrR2 = sqrt(max(0, 1.0 - sqrR2));
	float x = cos(two_pi_by_r1) * sqrt_of_one_minus_sqrR2;
	float y = sin(two_pi_by_r1) * sqrt_of_one_minus_sqrR2;
	float z = r2;
	return float3(x, y, z);*/
}

#define USE_BOX_MULLER

#ifdef USE_BOX_MULLER

float2 BM_2() {
	float u1 = 1.0 - random(); //uniform(0,1] random doubles
	float u2 = 1.0 - random();
	float r = sqrt(-2.0 * log(max(0.0000000001, u1)));
	float t = 2.0 * pi * u2;
	return r * float2(cos(t), sin(t));
}

float4 BM_4() {
	float2 u1 = 1.0 - float2(random(), random()); //uniform(0,1] random doubles
	float2 u2 = 1.0 - float2(random(), random());

	float2 r = sqrt(-2.0 * log(max(0.0000000001, u1)));
	float2 t = 2.0 * pi * u2;
	return float4(r.x * float2(cos(t.x), sin(t.x)), r.y * float2(cos(t.y), sin(t.y)));
}

float randomStdNormal() {
	return BM_2().x;
}

float3 randomStdNormal3() {
	return BM_4().xyz;
}

float2 randomStdNormal2() {
	return BM_2();
}

float4 randomStdNormal4() {
	return BM_4();
}

//float randomStdNormal() {
//	float u1 = 1.0 - random(); //uniform(0,1] random doubles
//	float u2 = 1.0 - random();
//	return sqrt(-2.0 * log(max(0.0000000001, u1))) *
//		sin(2.0 * pi * u2); //random normal(0,1)
//}
//
//float3 randomStdNormal3() {
//
//	return float3(randomStdNormal(), randomStdNormal(), randomStdNormal());
//
//	float3 u1 = 1.0 - float3(random(), random(), random()); //uniform(0,1] random doubles
//	float3 u2 = 1.0 - float3(random(), random(), random());
//	return sqrt(-2.0 * log(max(0.0000000001, u1))) *
//		sin(2.0 * pi * u2); //random normal(0,1)
//}
//
//float2 randomStdNormal2() {
//
//	return float2(randomStdNormal(), randomStdNormal());
//
//	float2 u1 = 1.0 - float2(random(), random()); //uniform(0,1] random doubles
//	float2 u2 = 1.0 - float2(random(), random());
//	return sqrt(-2.0 * log(max(0.0000000001, u1))) *
//		sin(2.0 * pi * u2); //random normal(0,1)
//}
//
//float4 randomStdNormal4() {
//	return float4(randomStdNormal(), randomStdNormal(), randomStdNormal(), randomStdNormal());
//	float4 u1 = 1.0 - float4(random(), random(), random(), random()); //uniform(0,1] random doubles
//	float4 u2 = 1.0 - float4(random(), random(), random(), random());
//	return sqrt(-2.0 * log(max(0.0000000001, u1))) *
//		sin(2.0 * pi * u2); //random normal(0,1)
//}

#else

float MBG_erfinv(float x)
{
	float w, p;
	w = -log(max(0.0000000001, (1.0f - x) * (1.0f + x)));
	if (w < 5.000000f) {
		w = w - 2.500000f;
		p = 2.81022636e-08f;
		p = 3.43273939e-07f + p * w;
		p = -3.5233877e-06f + p * w;
		p = -4.39150654e-06f + p * w;
		p = 0.00021858087f + p * w;
		p = -0.00125372503f + p * w;
		p = -0.00417768164f + p * w;
		p = 0.246640727f + p * w;
		p = 1.50140941f + p * w;
	}
	else {
		w = sqrt(w) - 3.000000f;
		p = -0.000200214257f;
		p = 0.000100950558f + p * w;
		p = 0.00134934322f + p * w;
		p = -0.00367342844f + p * w;
		p = 0.00573950773f + p * w;
		p = -0.0076224613f + p * w;
		p = 0.00943887047f + p * w;
		p = 1.00167406f + p * w;
		p = 2.83297682f + p * w;
	}
	return p * x;
}

float randomStdNormal() {
	float p = random(); //uniform(0,1] random doubles
	return 1.414213562373 * MBG_erfinv(2 * p - 1);
}

float3 randomStdNormal3() {
	float3 p = 2 * float3(random(), random(), random()) - 1;
	return 1.414213562373 * float3(MBG_erfinv(p.x), MBG_erfinv(p.y), MBG_erfinv(p.z));
}

float2 randomStdNormal2() {
	float2 p = 2 * float2(random(), random()) - 1;
	return 1.414213562373 * float2(MBG_erfinv(p.x), MBG_erfinv(p.y));
}

float4 randomStdNormal4() {
	float4 p = 2 * float4(random(), random(), random(), random()) - 1;
	return 1.414213562373 * float4(MBG_erfinv(p.x), MBG_erfinv(p.y), MBG_erfinv(p.z), MBG_erfinv(p.w));
}

#endif

float gauss(float mu = 0, float sigma = 1) {
	return mu + sigma * randomStdNormal(); //random normal(mean,stdDev^2)
}


