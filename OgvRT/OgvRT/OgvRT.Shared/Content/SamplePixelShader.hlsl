// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 vLumaPosition : TEXCOORD0;
	float2 vChromaPosition : TEXCOORD1;
};

sampler uSamplerY;
Texture2D <float4> uTextureY;

sampler uSamplerCb;
Texture2D <float4> uTextureCb;

sampler uSamplerCr;
Texture2D <float4> uTextureCr;

float4 main(PixelShaderInput input) : SV_TARGET
{
	float Y = uTextureY.Sample(uSamplerY, input.vLumaPosition).x;
	float Cb = uTextureCb.Sample(uSamplerCb, input.vChromaPosition).x;
	float Cr = uTextureCr.Sample(uSamplerCr, input.vChromaPosition).x;

	// Now assemble that into a YUV vector, and premultipy the Y...
	float3 YUV = float3(
		Y * 1.1643828125,
		Cb,
		Cr
		);
	// And convert that to RGB!
	return float4(
		YUV.x + 1.59602734375 * YUV.z - 0.87078515625,
		YUV.x - 0.39176171875 * YUV.y - 0.81296875 * YUV.z + 0.52959375,
		YUV.x + 2.017234375   * YUV.y - 1.081390625,
		1
		);

}
