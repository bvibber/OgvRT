#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"

namespace OgvRT
{
	// This sample renderer instantiates a basic rendering pipeline.
	class Sample3DSceneRenderer
	{
	public:
		Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();
		void StartTracking();
		void TrackingUpdate(float positionX);
		void StopTracking();
		bool IsTracking() { return m_tracking; }


	private:
		void Rotate(float radians);
		void CreateTexture(int width, int height, Microsoft::WRL::ComPtr<ID3D11Texture2D> &tex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> &view);
		void UpdateTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D> &tex, const char *bytes, int nbytes);

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_textureY;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_textureCb;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_textureCr;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureViewY;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureViewCb;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureViewCr;

		// System resources for cube geometry.
		ModelViewProjectionConstantBuffer	m_constantBufferData;
		uint32	m_indexCount;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		float	m_degreesPerSecond;
		bool	m_tracking;
	};
}

