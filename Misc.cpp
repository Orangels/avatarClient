#include "Misc.h"
#include <Shlwapi.h>
#include <wincodec.h>
#include <dshow.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Strmiids.lib")

namespace logger {

void Log(const wchar_t* fmt, ...)
{
	wchar_t buf[512] = { 0 };
	va_list arg;
	va_start(arg, fmt);
	StringCchVPrintfW(buf, _countof(buf), fmt, arg);
	va_end(arg);
	OutputDebugStringW(buf);
}

}


static const GUID MY_CLSID_WICImagingFactory1 =
{ 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };

static const GUID MY_CLSID_WICImagingFactory2 =
{ 0x317d06e8, 0x5f24, 0x433d, { 0xbd, 0xf7, 0x79, 0xce, 0x68, 0xd8, 0xab, 0xc2 } };
HRESULT ConvertBitmapSourceToHBITMAP(IWICBitmapSource* pBitmapSource,
	IWICImagingFactory* pImagingFactory,
	HBITMAP* phbmp)
{
	*phbmp = NULL;

	IWICBitmapSource* pBitmapSourceConverted = NULL;
	WICPixelFormatGUID guidPixelFormatSource;
	HRESULT hr = pBitmapSource->GetPixelFormat(&guidPixelFormatSource);

	if (SUCCEEDED(hr) && (guidPixelFormatSource != GUID_WICPixelFormat32bppBGRA))
	{
		IWICFormatConverter* pFormatConverter;
		hr = pImagingFactory->CreateFormatConverter(&pFormatConverter);
		if (SUCCEEDED(hr))
		{
			// Create the appropriate pixel format converter.
			hr = pFormatConverter->Initialize(pBitmapSource,
				GUID_WICPixelFormat32bppBGRA,
				WICBitmapDitherTypeNone,
				NULL,
				0,
				WICBitmapPaletteTypeCustom);
			if (SUCCEEDED(hr))
			{
				hr = pFormatConverter->QueryInterface(&pBitmapSourceConverted);
			}
			pFormatConverter->Release();
		}
	}
	else
	{
		// No conversion is necessary.
		hr = pBitmapSource->QueryInterface(&pBitmapSourceConverted);
	}

	if (SUCCEEDED(hr))
	{
		UINT nWidth, nHeight;
		hr = pBitmapSourceConverted->GetSize(&nWidth, &nHeight);
		if (SUCCEEDED(hr))
		{
			BITMAPINFO bmi = { sizeof(bmi.bmiHeader) };
			bmi.bmiHeader.biWidth = nWidth;
			bmi.bmiHeader.biHeight = -static_cast<LONG>(nHeight);
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;

			BYTE* pBits;
			HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pBits), NULL, 0);
			hr = hbmp ? S_OK : E_OUTOFMEMORY;
			if (SUCCEEDED(hr))
			{
				WICRect rect = { 0, 0, nWidth, nHeight };

				// Convert the pixels and store them in the HBITMAP.  
				// Note: the name of the function is a little misleading - 
				// we're not doing any extraneous copying here.  CopyPixels 
				// is actually converting the image into the given buffer.
				hr = pBitmapSourceConverted->CopyPixels(&rect, nWidth * 4, nWidth * nHeight * 4, pBits);
				if (SUCCEEDED(hr))
				{
					*phbmp = hbmp;
				}
				else
				{
					DeleteObject(hbmp);
				}
			}
		}

		pBitmapSourceConverted->Release();
	}
	return hr;
}

HRESULT WICCreateHBITMAP(IStream* pstm, HBITMAP* phbmp)
{
	*phbmp = NULL;

	// Create the COM imaging factory.
	IWICImagingFactory* pImagingFactory;
	HRESULT hr = CoCreateInstance(MY_CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pImagingFactory));
	if (FAILED(hr))
		hr = CoCreateInstance(MY_CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pImagingFactory));
	if (SUCCEEDED(hr))
	{
		// Create an appropriate decoder.
		IWICBitmapDecoder* pDecoder;
		hr = pImagingFactory->CreateDecoderFromStream(pstm, &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, &pDecoder);
		if (SUCCEEDED(hr))
		{
			IWICBitmapFrameDecode* pBitmapFrameDecode;
			hr = pDecoder->GetFrame(0, &pBitmapFrameDecode);
			if (SUCCEEDED(hr))
			{
				hr = ConvertBitmapSourceToHBITMAP(pBitmapFrameDecode, pImagingFactory, phbmp);
				pBitmapFrameDecode->Release();
			}
			pDecoder->Release();
		}
		pImagingFactory->Release();
	}
	return hr;
}

HBITMAP DecodeStream(unsigned char* data, unsigned int size)
{
	typedef IStream* (__stdcall* SHCreateMemStreamProc)(const BYTE* pInit, UINT cbInit);

	bool success = false;
	HMODULE hShlWapi = NULL;
	SHCreateMemStreamProc pfnSHCreateMemStream = NULL;
	IStream* pStream = NULL;
	HBITMAP hBitmap = NULL;
	HRESULT hr = S_OK;

	do
	{
		hShlWapi = ::LoadLibraryW(L"shlwapi.dll");
		if (!hShlWapi)
			break;

		pfnSHCreateMemStream = (SHCreateMemStreamProc)::GetProcAddress(hShlWapi, (LPCSTR)12);
		if (!pfnSHCreateMemStream)
			break;

		pStream = pfnSHCreateMemStream(data, size);
		if (!pStream)
			break;

		hr = WICCreateHBITMAP(pStream, &hBitmap);
		if (FAILED(hr))
			break;

	} while (0);

	if (pStream)
		pStream->Release();

	if (hShlWapi)
		::FreeLibrary(hShlWapi);

	return hBitmap;
}

HBITMAP DecodeImage(const char* filename)
{
	HANDLE hFile = ::CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD dwSize = ::GetFileSize(hFile, NULL);
		if (dwSize != 0xFFFFFFFF)
		{
			std::vector<unsigned char> data(dwSize, 0);
			DWORD dwReadBytes = 0;
			::ReadFile(hFile, data.data(), dwSize, &dwReadBytes, NULL);
			::CloseHandle(hFile);
			if (dwReadBytes == dwSize)
			{
				return DecodeStream(data.data(), data.size());
			}
		}
	}

	return NULL;
}

__int64 GetUnixTime()
{
	FILETIME ft;
	__int64 t;
	GetSystemTimeAsFileTime(&ft);
	t = (__int64)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
	return t / 10 - 11644473600000000;
}