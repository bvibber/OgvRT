// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
	float2 pos : SV_POSITION;
	float2 vLumaPosition : TEXCOORD0;
	float2 vChromaPosition : TEXCOORD1;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 vLumaPosition : TEXCOORD0;
	float2 vChromaPosition : TEXCOORD1;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	output.pos = float4(input.pos, 0, 0);
	output.vLumaPosition = input.vLumaPosition;
	output.vChromaPosition = input.vChromaPosition;

	return output;
}
