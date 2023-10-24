#ifndef DISPLAY_WINDOW_H
#define DISPLAY_WINDOW_H

#include <Windows.h>
#include <stdio.h>

class DisplayWindow
{
	HWND hwnd_;
	HBITMAP hbitmap_;
	int frame_width_;
	int frame_height_;

public:
	DisplayWindow();
	~DisplayWindow();

	bool Create(HWND parent, const wchar_t* name, unsigned int style, unsigned int ex_style, int x, int y, int w, int h);
	void Destroy();
	void Display(unsigned char* data, int data_len, int width, int height);
	void Clear();
	void ShowWindow();
	void UpdateWindow();
	void MoveWindow(int x, int y, int width, int height);
	bool IsWindow() { return TRUE == ::IsWindow(hwnd_); }

protected:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	bool RegisterWindowClass();
	LRESULT ProcessWindowMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void FillSolidRect(HDC hdc, int x, int y, int w, int h, COLORREF clr);
};

#endif
