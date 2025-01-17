#include "DirectXTexture.h"

#include <d3d11.h>

#include <wincodec.h>

#include <sstream>

leap::graphics::DirectXTexture::DirectXTexture(ID3D11Device* pDevice, const std::string& path)
{
	LoadTexture(pDevice, path);
}

leap::graphics::DirectXTexture::~DirectXTexture()
{
	if(m_pResource) m_pResource->Release();
	if(m_pSRV) m_pSRV->Release();
}

glm::vec4 leap::graphics::DirectXTexture::GetPixel(int /*x*/, int /*y*/) const
{
	// TODO: Implement this function
	throw std::runtime_error("DirectXEngine Error: GetPixel is not implemented");
}

glm::ivec2 leap::graphics::DirectXTexture::GetSize() const
{
	D3D11_TEXTURE2D_DESC desc{};
	m_pResource->GetDesc(&desc);
	return { desc.Width, desc.Height };
}

void leap::graphics::DirectXTexture::Reload(ID3D11Device* pDevice, const std::string& path)
{
	if (m_pResource) m_pResource->Release();
	if (m_pSRV) m_pSRV->Release();

	LoadTexture(pDevice, path);
}

void leap::graphics::DirectXTexture::LoadTexture(ID3D11Device* pDevice, const std::string& path)
{
	// Create a WIC factory
	IWICImagingFactory* pWICFactory{};
	HRESULT result{ CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWICFactory)) };
	if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to create WIC factory when creating a texture" };

	// Load the image using WIC
	std::wstringstream wss{};
	wss << path.c_str();
	IWICBitmapDecoder* pWICDecoder;
	result = pWICFactory->CreateDecoderFromFilename(wss.str().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pWICDecoder);
	if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to load image using the WIC factory" };

	// Get the WIC frame from the image
	IWICBitmapFrameDecode* pWICFrame{};
	pWICDecoder->GetFrame(0, &pWICFrame);

	// Get the dimensions from the WIC frame
	UINT wicWidth{}, wicHeight{};
	pWICFrame->GetSize(&wicWidth, &wicHeight);

	// Create an array to save all the color data in
	UINT stride{ wicWidth * 4 };
	UINT bufferSize{ stride * wicHeight };
	BYTE* pPixelData{ new BYTE[bufferSize] };

	// Get the format that the image is saved in
	WICPixelFormatGUID wicFormat{};
	pWICFrame->GetPixelFormat(&wicFormat);

	// Try to convert the WIC format to a DXGI format
	DXGI_FORMAT dxgiFormat = ConvertWICToDXGI(wicFormat);

	// If the format of the image is not a supported DXGI format, try to convert it
	if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
	{
		// Get a DXGI compatible WIC format from the current image format
		WICPixelFormatGUID compatibleWICFormat = ConvertWICToWIC(wicFormat);
		if (compatibleWICFormat == GUID_WICPixelFormatDontCare) throw std::runtime_error{ "DirectXEngine Error: Image format is not supported by the engine" };

		// Convert the converted WIC fromat to a DXGI format
		dxgiFormat = ConvertWICToDXGI(compatibleWICFormat);

		// Create the format converter
		IWICFormatConverter* pWICConverter{};
		result = pWICFactory->CreateFormatConverter(&pWICConverter);
		if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to create a WIC format converter" };

		// Make sure we can convert to the dxgi compatible format
		BOOL canConvert{};
		result = pWICConverter->CanConvert(wicFormat, compatibleWICFormat, &canConvert);
		if (FAILED(result) || !canConvert) throw std::runtime_error{ "DirectXEngine Error: The engine is not able to convert image to supported image format. Image format is not supported by the engine" };

		// Do the conversion (wicConverter will contain the converted image)
		result = pWICConverter->Initialize(pWICFrame, compatibleWICFormat, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
		if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to convert image to supported image format. The given image formate is not supported by the engine" };

		// The wic converter contains the converted image, copy data from converter to pixel array
		result = pWICConverter->CopyPixels(0, stride, bufferSize, pPixelData);
		if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to copy pixeldata to bitmap" };
	}
	else
	{
		// If the format was a dxgi supported format, just copy data from the wic frame to pixel array
		result = pWICFrame->CopyPixels(nullptr, stride, bufferSize, pPixelData);
		if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to copy pixeldata to bitmap" };
	}

	// Create a texture from the pixel data
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = wicWidth;
	desc.Height = wicHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = dxgiFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = /*realP*/pPixelData;
	initData.SysMemPitch = stride;
	initData.SysMemSlicePitch = static_cast<UINT>(wicHeight * stride);

	result = pDevice->CreateTexture2D(&desc, &initData, &m_pResource);
	if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to create directX texture using the WIC frame" };

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = dxgiFormat;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	pDevice->CreateShaderResourceView(m_pResource, &srvDesc, &m_pSRV);
	if (FAILED(result)) throw std::runtime_error{ "DirectXEngine Error: Failed to create shader resource view with the given texture" };

	// Clean up resources
	delete[] pPixelData;
	pWICFactory->Release();
	pWICDecoder->Release();
	pWICFrame->Release();
}

DXGI_FORMAT leap::graphics::DirectXTexture::ConvertWICToDXGI(const WICPixelFormatGUID& wicFormatGUID)
{
	if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) return DXGI_FORMAT_R32G32B32A32_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) return DXGI_FORMAT_R16G16B16A16_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) return DXGI_FORMAT_R16G16B16A16_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) return DXGI_FORMAT_R8G8B8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) return DXGI_FORMAT_B8G8R8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR) return DXGI_FORMAT_B8G8R8X8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) return DXGI_FORMAT_R10G10B10A2_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) return DXGI_FORMAT_B5G5R5A1_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) return DXGI_FORMAT_B5G6R5_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) return DXGI_FORMAT_R32_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) return DXGI_FORMAT_R16_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGray) return DXGI_FORMAT_R16_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) return DXGI_FORMAT_R8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) return DXGI_FORMAT_A8_UNORM;

	else return DXGI_FORMAT_UNKNOWN;
}

WICPixelFormatGUID leap::graphics::DirectXTexture::ConvertWICToWIC(const WICPixelFormatGUID& wicFormatGUID)
{
	if (wicFormatGUID == GUID_WICPixelFormatBlackWhite) return GUID_WICPixelFormat8bppGray;
	else if (wicFormatGUID == GUID_WICPixelFormat1bppIndexed) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat2bppIndexed) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat4bppIndexed) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat8bppIndexed) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat2bppGray) return GUID_WICPixelFormat8bppGray;
	else if (wicFormatGUID == GUID_WICPixelFormat4bppGray) return GUID_WICPixelFormat8bppGray;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayFixedPoint) return GUID_WICPixelFormat16bppGrayHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFixedPoint) return GUID_WICPixelFormat32bppGrayFloat;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR555) return GUID_WICPixelFormat16bppBGRA5551;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR101010) return GUID_WICPixelFormat32bppRGBA1010102;
	else if (wicFormatGUID == GUID_WICPixelFormat24bppBGR) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat24bppRGB) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppPBGRA) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppPRGBA) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat48bppRGB) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat48bppBGR) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRA) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBA) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppPBGRA) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat48bppBGRFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;
	else if (wicFormatGUID == GUID_WICPixelFormat128bppPRGBAFloat) return GUID_WICPixelFormat128bppRGBAFloat;
	else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFloat) return GUID_WICPixelFormat128bppRGBAFloat;
	else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;
	else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBE) return GUID_WICPixelFormat128bppRGBAFloat;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppCMYK) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppCMYK) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat40bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat80bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGB) return GUID_WICPixelFormat32bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGB) return GUID_WICPixelFormat64bppRGBA;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBAHalf) return GUID_WICPixelFormat64bppRGBAHalf;
#endif

	else return GUID_WICPixelFormatDontCare;
}
