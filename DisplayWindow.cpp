#include "DisplayWindow.h"
#include "Misc.h"
#include <stdio.h>

#define WM_DISPLAY_FRAME (WM_USER + 100)

static bool first_video_frame_displayed = false;

struct DisplayFrameParam
{
	unsigned char* data;
	int data_len;
	int width;
	int height;
};

DisplayWindow::DisplayWindow()
{
	hwnd_ = NULL;
	hbitmap_ = NULL;
	frame_width_ = 0;
	frame_height_ = 0;
}

DisplayWindow::~DisplayWindow()
{
	if (::IsWindow(hwnd_))
		::DestroyWindow(hwnd_);

	if (hbitmap_)
		DeleteObject(hbitmap_);
}

bool DisplayWindow::Create(HWND parent, const wchar_t* name, unsigned int style, unsigned int ex_style, int x, int y, int w, int h)
{
	if (!RegisterWindowClass())
		return false;

	HINSTANCE hInst = (HINSTANCE)GetModuleHandleW(NULL);
	hwnd_ = ::CreateWindowExW(ex_style, L"DisplayWindow", name, style, x, y, w, h, parent, NULL, hInst, this);
	return (hwnd_ != NULL);
}

void DisplayWindow::Destroy()
{
	if (::IsWindow(hwnd_))
		::DestroyWindow(hwnd_);
	hwnd_ = NULL;
}

void DisplayWindow::Display(unsigned char* data, int data_len, int width, int height)
{
	DisplayFrameParam param;
	param.data = data;
	param.data_len = data_len;
	param.width = width;
	param.height = height;
	::SendMessageW(hwnd_, WM_DISPLAY_FRAME, 0, (LPARAM)&param);
}

void DisplayWindow::Clear()
{
	DeleteObject(hbitmap_);
	hbitmap_ = NULL;
	InvalidateRect(hwnd_, NULL, TRUE);
}

void DisplayWindow::ShowWindow()
{
	::ShowWindow(hwnd_, SW_SHOW);
}

void DisplayWindow::UpdateWindow()
{
	::UpdateWindow(hwnd_);
}

void DisplayWindow::MoveWindow(int x, int y, int width, int height)
{
	::MoveWindow(hwnd_, x, y, width, height, FALSE);
}

bool DisplayWindow::RegisterWindowClass()
{
	HINSTANCE hinst = (HINSTANCE)GetModuleHandleW(NULL);
	WNDCLASSEXW wc;
	if (::GetClassInfoExW(hinst, L"DisplayWindow", &wc))
	{
		return true;
	}
	else
	{
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wc.lpfnWndProc = WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hinst;
		wc.hIcon = NULL;
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.lpszMenuName = NULL;
		wc.lpszClassName = L"DisplayWindow";
		wc.hIconSm = NULL;

		return (0 != ::RegisterClassExW(&wc));
	}
}

LRESULT DisplayWindow::ProcessWindowMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ERASEBKGND:
		{
			return FALSE;
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint(hwnd_, &ps);
			{
				RECT client_rect;
				GetClientRect(hwnd_, &client_rect);

				if (hbitmap_)
				{
					HDC hBitmapDC = CreateCompatibleDC(hDC);
					HGDIOBJ hOldBmp = SelectObject(hBitmapDC, (HGDIOBJ)hbitmap_);

					double xscale = 1.0 * client_rect.right / frame_width_;
					double yscale = 1.0 * client_rect.bottom / frame_height_;
					double scale = xscale < yscale ? xscale : yscale;
					int new_width = int(scale * frame_width_ + 0.5);
					int new_height = int(scale * frame_height_ + 0.5);

					SetStretchBltMode(hDC, HALFTONE);
					StretchBlt(hDC, (client_rect.right - new_width) / 2, (client_rect.bottom - new_height) / 2, new_width, new_height, hBitmapDC, 0, 0, frame_width_, frame_height_, SRCCOPY);
					SelectObject(hBitmapDC, hOldBmp);
					DeleteDC(hBitmapDC);

					if (!first_video_frame_displayed)
					{
						first_video_frame_displayed = true;
						logger::Log(L"First video frame displayed!!! %I64d\n", GetUnixTime());
					}
				}
				else
				{
					FillSolidRect(hDC, 0, 0, client_rect.right, client_rect.bottom, 0);
				}
			}
			EndPaint(hwnd_, &ps);
			return 0L;
		}
		break;
	case WM_DISPLAY_FRAME:
		{
			DisplayFrameParam* param = (DisplayFrameParam*)lParam;
			frame_width_ = param->width;
			frame_height_ = param->height;

			if (!hbitmap_)
			{
				BITMAPINFO bmi = { sizeof(bmi.bmiHeader) };
				bmi.bmiHeader.biWidth = frame_width_;
				bmi.bmiHeader.biHeight = -frame_height_;
				bmi.bmiHeader.biPlanes = 1;
				bmi.bmiHeader.biBitCount = 24;
				bmi.bmiHeader.biCompression = BI_RGB;

				BYTE *pBits;
				hbitmap_ = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, reinterpret_cast<void **>(&pBits), NULL, 0);
				memcpy(pBits, param->data, param->data_len);
			}
			else
			{
				BITMAP bm = { 0 };
				GetObject(hbitmap_, sizeof(BITMAP), &bm);
				memcpy(bm.bmBits, param->data, param->data_len);
			}

			InvalidateRect(hwnd_, NULL, TRUE);
			return 0L;
		}
		break;
	default:
		break;
	}
	return ::DefWindowProc(hwnd_, uMsg, wParam, lParam);
}

LRESULT DisplayWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DisplayWindow* self = NULL;
	if (uMsg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		self = static_cast<DisplayWindow*>(lpcs->lpCreateParams);
		self->hwnd_ = hWnd;
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	else
	{
		self = reinterpret_cast<DisplayWindow*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
		if (uMsg == WM_NCDESTROY && self)
		{
			LRESULT lRes = ::DefWindowProc(hWnd, uMsg, wParam, lParam);
			::SetWindowLongPtr(hWnd, GWLP_USERDATA, 0L);
			self->hwnd_ = NULL;
			return lRes;
		}
	}

	if (self)
	{
		return self->ProcessWindowMessage(uMsg, wParam, lParam);
	}
	else
	{
		return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

void DisplayWindow::FillSolidRect(HDC hdc, int x, int y, int w, int h, COLORREF clr)
{
	RECT rect = { x, y, x + w, y + h };
	::SetBkColor(hdc, clr);
	::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
}
