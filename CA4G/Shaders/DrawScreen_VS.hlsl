float4 main(float2 pos : POSITION, out float2 C : TEXCOORD) : SV_POSITION
{
	C = float2((pos.x + 1)*0.5, (1 - pos.y)*0.5);
	return float4(pos.x, pos.y, 0.5, 1);
}