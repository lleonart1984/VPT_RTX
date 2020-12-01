Texture2D<float4> tex : register (t0);

//cbuffer Transforming : register (b0) {
//	row_major matrix Transform;
//}

float4 main(float4 proj : SV_POSITION, float2 C : TEXCOORD) : SV_TARGET
{
	uint w,h;
	tex.GetDimensions(w, h);
	//return mul(tex.Sample(Samp, C), Transform);
	return tex[uint2(w,h)*C];
}