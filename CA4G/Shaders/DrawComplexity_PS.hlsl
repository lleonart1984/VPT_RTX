Texture2D<int> tex : register (t0);

float3 GetColor(int complexity) {

	if (complexity == 0)
		return float3(1, 1, 1);

	//return float3(1,1,1);

	float level = log2(complexity);
	float3 stopPoints[11] = {
		float3(0,0,0.5), // 1
		float3(0,0,1), // 2
		float3(0,0.5,1), // 4
		float3(0,1,1), // 8
		float3(0,1,0.5), // 16
		float3(0,1,0), // 32
		float3(0.5,1,0), // 64
		float3(1,1,0), // 128
		float3(1,0.5,0), // 256
		float3(1,0,0), // 512
		float3(1,0,1) // 1024
	};

	if (level >= 10)
		return stopPoints[10];

	return lerp(stopPoints[(int)level], stopPoints[(int)level + 1], level % 1);
}

float4 main(float4 proj : SV_POSITION, float2 C : TEXCOORD) : SV_TARGET
{
	int width, height;
	tex.GetDimensions(width, height);
	return float4(GetColor(tex[uint2(width*C.x, height*C.y)]),1);
}