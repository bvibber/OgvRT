#pragma once

namespace OgvRT
{
	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	// Used to send per-vertex data to the vertex shader.
	struct VertexPositions
	{
		DirectX::XMFLOAT2 pos;
		DirectX::XMFLOAT2 lumaPos;
		DirectX::XMFLOAT2 chromaPos;
	};
}