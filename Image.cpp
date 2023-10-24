#include "Image.h"
#include <Windows.h>
#include "libpng/png.h"
#include "libpng/pngstruct.h"

#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

static const GUID MY_CLSID_WICImagingFactory1 =
{ 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };

static const GUID MY_CLSID_WICImagingFactory2 =
{ 0x317d06e8, 0x5f24, 0x433d, { 0xbd, 0xf7, 0x79, 0xce, 0x68, 0xd8, 0xab, 0xc2 } };

namespace ui {

Image::Image()
{
	bitmap_ = NULL;
	SetScale(0, 0, 0, 0);
}

Image::Image(const Image& other)
{
	bitmap_ = other.bitmap_;
	scale_left_ = other.scale_left_;
	scale_right_ = other.scale_right_;
	scale_top_ = other.scale_top_;
	scale_bottom_ = other.scale_bottom_;
}

Image::~Image()
{
	if (bitmap_)
	{
		::DeleteObject(bitmap_);
		bitmap_ = NULL;
	}
}

Image* Image::Clone()
{
	return new Image(*this);
}

bool Image::Load(const wchar_t* filename)
{
	bool success = false;

	HANDLE hFile = ::CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return success;

	DWORD dwSize = ::GetFileSize(hFile, NULL);
	if (dwSize != 0xFFFFFFFF)
	{
		unsigned char *buf = new unsigned char[dwSize];
		DWORD dwReadBytes = 0;
		::ReadFile(hFile, buf, dwSize, &dwReadBytes, NULL);

		success = FromStream(buf, dwSize);
		delete[] buf;
		::CloseHandle(hFile);
	}

	return success;
}

bool Image::FromStream(unsigned char* data, unsigned int size)
{
	if (data[0] == 0xFF && data[1] == 0xD8)
		return FromJpgStream(data, size);
	else if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
		return FromPngStream(data, size);
	else
		return false;
}

HBITMAP Image::AttachBitmap(HBITMAP bitmap)
{
	HBITMAP old_bitmap = bitmap_;
	bitmap_ = bitmap;
	return old_bitmap;
}

int Image::GetWidth() const
{
	if (!bitmap_)
		return 0;

	BITMAP bm;
	GetObject(bitmap_, sizeof(BITMAP), &bm);
	return bm.bmWidth;
}

int Image::GetHeight() const
{
	if (!bitmap_)
		return 0;

	BITMAP bm;
	GetObject(bitmap_, sizeof(BITMAP), &bm);
	return bm.bmHeight;
}

void Image::SetScale(unsigned short left, unsigned short right, unsigned short top, unsigned short bottom)
{
	scale_left_ = left;
	scale_right_ = right;
	scale_top_ = top;
	scale_bottom_ = bottom;
}

void Image::SetAttribute(const char* name, const char* value)
{
	if (stricmp(name, "scale_left") == 0)
	{
		scale_left_ = (unsigned short)atoi(value);
	}
	else if (stricmp(name, "scale_top") == 0)
	{
		scale_top_ = (unsigned short)atoi(value);
	}
	else if (stricmp(name, "scale_right") == 0)
	{
		scale_right_ = (unsigned short)atoi(value);
	}
	else if (stricmp(name, "scale_bottom") == 0)
	{
		scale_bottom_ = (unsigned short)atoi(value);
	}
}

void Image::Draw(const Canvas &canvas, const Rect &rect) const
{
	canvas.RenderImage(*this, rect);
}

//////////////////////////////////////////////////////////////////////////

HRESULT ConvertBitmapSourceToHBITMAP(IWICBitmapSource *pBitmapSource, 
									 IWICImagingFactory *pImagingFactory, 
									 HBITMAP *phbmp)
{
	*phbmp = NULL;

	IWICBitmapSource *pBitmapSourceConverted = NULL;
	WICPixelFormatGUID guidPixelFormatSource;
	HRESULT hr = pBitmapSource->GetPixelFormat(&guidPixelFormatSource);

	if (SUCCEEDED(hr) && (guidPixelFormatSource != GUID_WICPixelFormat32bppBGRA))
	{
		IWICFormatConverter *pFormatConverter;
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

			BYTE *pBits;
			HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, reinterpret_cast<void **>(&pBits), NULL, 0);
			hr = hbmp ? S_OK : E_OUTOFMEMORY;
			if (SUCCEEDED(hr))
			{
				WICRect rect = {0, 0, nWidth, nHeight};

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

HRESULT WICCreateHBITMAP(IStream *pstm, HBITMAP *phbmp)
{
	*phbmp = NULL;

	// Create the COM imaging factory.
	IWICImagingFactory *pImagingFactory;
	HRESULT hr = CoCreateInstance(MY_CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pImagingFactory));
	if (FAILED(hr))
		hr = CoCreateInstance(MY_CLSID_WICImagingFactory2, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pImagingFactory));
	if (SUCCEEDED(hr))
	{
		// Create an appropriate decoder.
		IWICBitmapDecoder *pDecoder;
		hr = pImagingFactory->CreateDecoderFromStream(pstm, &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, &pDecoder);
		if (SUCCEEDED(hr))
		{
			IWICBitmapFrameDecode *pBitmapFrameDecode;
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

bool Image::FromJpgStream(unsigned char* data, unsigned int size)
{
	typedef IStream* (__stdcall *SHCreateMemStreamProc)(const BYTE *pInit, UINT cbInit);

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

		bitmap_ = hBitmap;
		success = true;

	} while (0);

	if (pStream)
		pStream->Release();

	if (hShlWapi)
		::FreeLibrary(hShlWapi);

	return success;
}

//////////////////////////////////////////////////////////////////////////

static png_bytep cur_datas;

static void read_data_fn(png_structp /*png_ptr*/, png_bytep d, png_size_t length)
{
	memcpy(d, cur_datas, length);
	cur_datas += length;
}

static void convert(BYTE* to, const BYTE* from, int w, BOOL alpha)
{
	DWORD *t = (DWORD *)to;

	if (!alpha)
	{
		t += w;
		from += 3 * w;
		while (t > (DWORD *)to)
		{
			from -= 3;
			*--t = 0xff000000 | (from[0]<<16) | (from[1]<<8) | from[2];
		}
	}
	else
	{
		t += w;
		from += 4*w;
		while (t > (DWORD *)to)
		{
			from -= 4;
			BYTE a = from[3];
			*--t = (a<<24) | (((from[0]*a)<<8)&0xff0000) | ((from[1]*a)&0xff00) | ((from[2]*a)>>8);
		}
	}
}

bool Image::FromPngStream(unsigned char* data, unsigned int size)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height, linedelta;
	int bit_depth, color_type;
	bool ret = false;
	bool alpha = false;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (png_ptr == NULL)
		return false;

	info_ptr = png_create_info_struct(png_ptr);

	if (data)
	{
		cur_datas = (png_bytep)data;
		png_set_read_fn(png_ptr, cur_datas, read_data_fn);
	}

	if (info_ptr == NULL || (data == NULL))
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return FALSE;
	}

	if (data)
	{
		if (png_sig_cmp((BYTE*)data, (png_size_t)0, 8))
			goto error;
	}

	if (setjmp(png_ptr->longjmp_buffer))
		goto error;

	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	if (color_type == PNG_COLOR_TYPE_PALETTE && bit_depth <= 8) 
		png_set_expand(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) 
		png_set_expand(png_ptr);

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (bit_depth < 8)
		png_set_packing(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	if (color_type & PNG_COLOR_MASK_ALPHA)
		alpha = true;

	linedelta = width * 4;

	HDC hDC = ::GetDC(NULL);
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(BITMAPINFO));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = 0 - height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;         // four 8-bit components
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = width * height * 4;
	BYTE* buf = NULL;
	bitmap_ = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (void **)&buf, NULL, 0);

	{ // avoid y initialization error in VC6 because of 'goto error' code
		for (unsigned y = 0; y < height; y++)
		{
			BYTE* b = buf + y * linedelta;
			png_read_row(png_ptr, b, NULL);
			convert(b, b, width, alpha);
		}
	}

	png_read_end(png_ptr, NULL);
	ret = true;

	::ReleaseDC(NULL, hDC);

error:
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
	return ret;
}

bool Image::FromWebpStream(unsigned char* data, unsigned int size)
{
	bool ret = false;
	return ret;
}

//////////////////////////////////////////////////////////////////////////
// GifImage

#define NEXTBYTE(data) (data ? *data++ : 0)
#define GETSHORT(data, var) var = NEXTBYTE(data); var += NEXTBYTE(data) << 8

GifImage::GifImage()
{
	delay_ = 0;
	cur_frame_ = 0;
}

GifImage::~GifImage()
{
	for (int i = 1; i < frames_.GetSize(); i++)
	{
		if (frames_.GetAt(i))
			delete frames_.GetAt(i);
	}
}

void GifImage::Animate() const
{
	if (++cur_frame_ == frames_.GetSize())
		cur_frame_ = 0;
}

bool GifImage::FromStream(unsigned char* data, unsigned int size)
{
	if (data[0] != 'G' || data[1] != 'I' || data[2] != 'F')
		return false;

	// skip flag and version info
	data += 6;

	int width = 0; 
	GETSHORT(data, width);

	int height = 0;
	GETSHORT(data, height);

	if (width <= 0 || height <= 0)
		return false;

	unsigned char ch = NEXTBYTE(data);
	char has_color_table = ((ch & 0x80) != 0);
	int bits_per_pixel = (ch & 7) + 1;
	int color_table_size = 1 << bits_per_pixel;
	unsigned char bg_color_index = NEXTBYTE(data); // Background Color index
	unsigned char aspect_ratio = NEXTBYTE(data); // Aspect ratio is N/64

	// Read in global colormap:
	unsigned char transparent_pixel = 0;
	char has_transparent = 0;
	unsigned int color_table[256] = {0};

	if (has_color_table)
	{
		for (int i = 0; i < color_table_size; i++)
		{
			unsigned int r = NEXTBYTE(data);
			unsigned int g = NEXTBYTE(data);
			unsigned int b = NEXTBYTE(data);
			color_table[i] = 0xff000000 | (r << 16) | (g << 8) | b;
		}
	}
	else
	{
		for (int i = 0; i < color_table_size; i++)
			color_table[i] = 0xff000000 | (0x10101 * i);
	}

	int block_len = 0;
	int code_size = 0;		/* Code size, init from GIF header, increases... */
	char interlace = 0;

	for (;;)
	{
		unsigned char i = NEXTBYTE(data);
		if (i < 0)
		{
			// unexpected EOF
			break;
		}

		if (i == 0x3B)
			break; // eof code


		if (i == 0x21) // a gif extension
		{		
			ch = NEXTBYTE(data);
			block_len = NEXTBYTE(data);

			if (ch == 0xF9 && block_len == 4) // Netscape animation extension
			{
				char bits;
				bits = NEXTBYTE(data);
				GETSHORT(data, delay_);
				transparent_pixel = NEXTBYTE(data);
				if (bits & 1)
					has_transparent = 1;
				block_len = NEXTBYTE(data);
			} 
			else if (ch == 0xFF) // Netscape repeat count
			{

			} 
			else if (ch == 0xFE) // Gif Comment
			{
#if 0
				if (blocklen > 0)
				{
					char *comment = new char[blocklen + 1];
					int l;
					for (l = 0; blocklen; l++, blocklen--)
						comment[l] = NEXTBYTE(data);
					comment[l] = 0;
					delete[] comment;
					NEXTBYTE(data); //End marker
				}
#endif
			} 
			else
			{
				// unknown gif extension
			}
		} 
		else if (i == 0x2c)	// an image
		{
			unsigned short x_pos = 0;
			GETSHORT(data, x_pos);
			unsigned short y_pos = 0;
			GETSHORT(data, y_pos);
			GETSHORT(data, width);
			GETSHORT(data, height);
			ch = NEXTBYTE(data);
			interlace = ((ch & 0x40) != 0);
			if (ch & 0x80)
			{
				// read local color table
				int n = 2 << (ch & 7);
				for (i = 0; i < n; i++)
				{
					unsigned char r = NEXTBYTE(data);
					unsigned char g = NEXTBYTE(data);
					unsigned char b = NEXTBYTE(data);
					color_table[i] = 0xff000000 | (r<<16) | (g<<8) | b;
				}
			}
			code_size = NEXTBYTE(data) + 1;

			if (bits_per_pixel >= code_size)
			{
				// Workaround for broken GIF files...
				bits_per_pixel = code_size - 1;
				color_table_size = 1 << bits_per_pixel;
			}

			unsigned char *ImageBuf = new unsigned char[width * height];
			if (!ImageBuf)
			{
				// Insufficient memory
				return false;
			}
			int YC = 0, Pass = 0; // Used to de-interlace the picture
			unsigned char *p = ImageBuf;
			unsigned char *eol = p + width;

			int InitCodeSize = code_size;
			int ClearCode = (1 << (code_size - 1));
			int EOFCode = ClearCode + 1;
			int FirstFree = ClearCode + 2;
			int FinChar = 0;
			int ReadMask = (1 << code_size) - 1;
			int FreeCode = FirstFree;
			int OldCode = ClearCode;

			// tables used by LZW decompresser:
			short int Prefix[4096];
			unsigned char Suffix[4096];

			block_len = NEXTBYTE(data);
			unsigned char thisbyte = NEXTBYTE(data); block_len--;
			int frombit = 0;

			for (;;)
			{
				// Fetch the next code from the raster data stream.  The codes can be
				// any length from 3 to 12 bits, packed into 8-bit bytes, so we have to
				// maintain our location as a pointer and a bit offset.
				// In addition, gif adds totally useless and annoying block counts
				// that must be correctly skipped over. 
				int CurCode = thisbyte;
				if (frombit + code_size > 7)
				{
					if (block_len <= 0)
					{
						block_len = NEXTBYTE(data);
						if (block_len <= 0)
							break;
					}
					thisbyte = NEXTBYTE(data); block_len--;
					CurCode |= thisbyte << 8;
				}
				if (frombit + code_size > 15)
				{
					if (block_len <= 0)
					{
						block_len = NEXTBYTE(data);
						if (block_len <= 0)
							break;
					}
					thisbyte = NEXTBYTE(data); block_len--;
					CurCode |= thisbyte << 16;
				}
				CurCode = (CurCode >> frombit) & ReadMask;
				frombit = (frombit + code_size) % 8;

				if (CurCode == ClearCode)
				{
					code_size = InitCodeSize;
					ReadMask = (1 << code_size) - 1;
					FreeCode = FirstFree;
					OldCode = ClearCode;
					continue;
				}

				if (CurCode == EOFCode)
					break;

				unsigned char OutCode[1025]; // temporary array for reversing codes
				unsigned char *tp = OutCode;
				int i;
				if (CurCode < FreeCode)
					i = CurCode;
				else if (CurCode == FreeCode)
				{
					*tp++ = FinChar;
					i = OldCode;
				}
				else
				{
					break;
				}

				while (i >= color_table_size)
				{
					*tp++ = Suffix[i];
					i = Prefix[i];
				}
				*tp++ = FinChar = i;
				while (tp > OutCode)
				{
					*p++ = *--tp;
					if (p >= eol)
					{
						if (!interlace)
							YC++;
						else
						{
							switch (Pass)
							{
							case 0: 
								{
									YC += 8;
									if (YC >= height)
									{
										Pass++;
										YC = 4;
									}
								}
								break;
							case 1:
								{
									YC += 8;
									if (YC >= height)
									{
										Pass++;
										YC = 2;
									}
								}
								break;
							case 2:
								{
									YC += 4;
									if (YC >= height)
									{
										Pass++; 
										YC = 1;
									}
								}
								break;
							case 3:
								{
									YC += 2;
								}
								break;
							}
						}
						if (YC >= height)
							YC = 0; // cheap bug fix when excess data
						p = ImageBuf + YC * width;
						eol = p + width;
					}
				}

				if (OldCode != ClearCode)
				{
					Prefix[FreeCode] = OldCode;
					Suffix[FreeCode] = FinChar;
					FreeCode++;
					if (FreeCode > ReadMask)
					{
						if (code_size < 12)
						{
							code_size++;
							ReadMask = (1 << code_size) - 1;
						}
						else
						{
							FreeCode--;
						}
					}
				}
				OldCode = CurCode;
			}

			int line_delta = width * 4;

			HDC hDC = ::GetDC(NULL);
			BITMAPINFO bmi;
			memset(&bmi, 0, sizeof(BITMAPINFO));
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = width;
			bmi.bmiHeader.biHeight = 0 - height;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;         // four 8-bit components
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biSizeImage = width * height * 4;
			BYTE* buf = NULL;
			HBITMAP bitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (void **)&buf, NULL, 0);

			if (has_transparent)
				color_table[transparent_pixel] = 0;

			for (int y = 0; y < height; y++)
			{
				unsigned int* to = (unsigned int*)(buf + y * line_delta);
				p = ImageBuf + y * width;
				for (int x = 0; x < width; x++)
					to[x] = color_table[*p++];
			}

			::ReleaseDC(NULL, hDC);
			delete[] ImageBuf;

			if (frames_.GetSize() == 0)
			{
				AttachBitmap(bitmap);
				GifFrame *frame = new GifFrame(x_pos, y_pos, this);
				frames_.Add(frame);
				block_len = NEXTBYTE(data);
			}
			else
			{
				Image* img = new Image;
				img->AttachBitmap(bitmap);
				GifFrame *frame = new GifFrame(x_pos, y_pos, img);
				frames_.Add(frame);
				block_len = 0;
			}
		}
		else
		{
			// unknown gif code
			block_len = 0;
		}

		// skip the data:
		while (block_len > 0)
		{
			while (block_len--)
			{
				NEXTBYTE(data);
			}
			block_len = NEXTBYTE(data);
		}
	}

	return (frames_.GetSize() != 0);
}

void GifImage::SetAttribute(const char* name, const char* value)
{
	Image::SetAttribute(name, value);
}

void GifImage::Draw(const Canvas &canvas, const Rect &rect) const
{
	Image::Draw(canvas, rect);

	for (unsigned short i = 1; i <= cur_frame_; i++)
	{
		GifFrame* frame = frames_.GetAt(i);
		Rect rc;
		rc.x = rect.x + frame->x_pos;
		rc.y = rect.y + frame->y_pos;
		rc.w = frame->image->GetWidth();
		rc.h = frame->image->GetHeight();
		frame->image->Draw(canvas, rc);
	}
}

MultiPieceImage::MultiPieceImage() : Image()
{
	pieces_ = 1;
	memset(states_, 0, sizeof(states_));
	current_state_ = UISTATE_NORMAL;
}

MultiPieceImage::MultiPieceImage(const MultiPieceImage& other) : Image(other)
{
	pieces_ = other.pieces_;
	memcpy(states_, other.states_, sizeof(states_));
	current_state_ = other.current_state_;
}

MultiPieceImage::~MultiPieceImage()
{

}

Image* MultiPieceImage::Clone()
{
	return new MultiPieceImage(*this);
}

void MultiPieceImage::Set(int state, Drawable *drawable)
{
	states_[state] = (unsigned char)drawable;
}

Drawable* MultiPieceImage::Get(int state)
{
	current_state_ = (unsigned char)state;
	return this;
}

int MultiPieceImage::GetWidth() const
{
	if (!bitmap_)
		return 0;

	BITMAP bm;
	GetObject(bitmap_, sizeof(BITMAP), &bm);
	return bm.bmWidth / pieces_;
}

int MultiPieceImage::GetHeight() const
{
	if (!bitmap_)
		return 0;

	BITMAP bm;
	GetObject(bitmap_, sizeof(BITMAP), &bm);
	return bm.bmHeight;
}

void MultiPieceImage::SetAttribute(const char* name, const char* value)
{
	if (_stricmp(name, "pieces") == 0)
	{
		pieces_ = (unsigned char)atoi(value);
	}
	else if (_stricmp(name, "normal") == 0)
	{
		states_[UISTATE_NORMAL] = (unsigned char)atoi(value);
		states_[UISTATE_HOVER] = states_[UISTATE_NORMAL];
		states_[UISTATE_PRESSED] = states_[UISTATE_NORMAL];
		states_[UISTATE_DISABLED] = states_[UISTATE_NORMAL];
		states_[UISTATE_SELECTED] = states_[UISTATE_NORMAL];
	}
	else if (_stricmp(name, "hover") == 0)
	{
		states_[UISTATE_HOVER] = (unsigned char)atoi(value);
	}
	else if (_stricmp(name, "pressed") == 0)
	{
		states_[UISTATE_PRESSED] = (unsigned char)atoi(value);
	}
	else if (_stricmp(name, "disable") == 0)
	{
		states_[UISTATE_DISABLED] = (unsigned char)atoi(value);
	}
	else if (_stricmp(name, "selected") == 0)
	{
		states_[UISTATE_SELECTED] = (unsigned char)atoi(value);
	}
	else
	{
		Image::SetAttribute(name, value);
	}
}

void MultiPieceImage::Draw(const Canvas &canvas, const Rect &rect) const
{
	int img_w = GetWidth();
	int img_h = GetHeight();
	Rect rcFrom(states_[current_state_] * img_w, 0, img_w, img_h);
	canvas.RenderImage(*this, rcFrom, rect);
}

} // namespace ui