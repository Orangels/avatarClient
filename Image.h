#ifndef _IMAGE_H
#define _IMAGE_H

#include "Drawable.h"

namespace ui {

class Canvas;

class Image : public Drawable
{
protected:
	HBITMAP bitmap_;
	unsigned short scale_left_;
	unsigned short scale_right_;
	unsigned short scale_top_;
	unsigned short scale_bottom_;

public:
	Image();
	Image(const Image& other);
	virtual ~Image();

	virtual bool FromStream(unsigned char* data, unsigned int size);
	virtual Image* Clone();
	
	bool Load(const wchar_t* filename);

	HBITMAP AttachBitmap(HBITMAP bitmap);
	HBITMAP GetBitmap() const { return bitmap_; }

	void SetScale(unsigned short left, unsigned short right, unsigned short top, unsigned short bottom);
	unsigned short GetScaleLeft() const { return scale_left_; }
	unsigned short GetScaleRight() const { return scale_right_; }
	unsigned short GetScaleTop() const { return scale_top_; }
	unsigned short GetScaleBottom() const { return scale_bottom_; }

	int GetWidth() const;
	int GetHeight() const;
	void SetAttribute(const char* name, const char* value);
	void Draw(const Canvas &canvas, const Rect &rect) const;

protected:
	bool FromPngStream(unsigned char* data, unsigned int size);
	bool FromJpgStream(unsigned char* data, unsigned int size);
	bool FromWebpStream(unsigned char* data, unsigned int size);
};

class GifImage : public Image
{
	struct GifFrame
	{
		GifFrame(unsigned short x = 0, unsigned short y = 0, Image* img = 0)
		{
			x_pos = x;
			y_pos = y;
			image = img;
		}
		~GifFrame()
		{
			if (image)
				delete image;
		}
		unsigned short x_pos;
		unsigned short y_pos;
		Image* image;
	};

	PtrArray<GifFrame> frames_;
	unsigned short delay_;
	mutable unsigned short cur_frame_;

public:
	GifImage();
	~GifImage();

	unsigned short GetDelay() const { return delay_; }
	void Animate() const;

	bool FromStream(unsigned char* data, unsigned int size);

	bool IsAnimation() const { return (frames_.GetSize() > 1); }
	void SetAttribute(const char* name, const char* value);
	void Draw(const Canvas &canvas, const Rect &rect) const;
};

class MultiPieceImage : public Image,
						public IStateful
{
	unsigned char pieces_;
	unsigned char states_[UISTATE_MAX];
	unsigned char current_state_;

public:
	MultiPieceImage();
	MultiPieceImage(const MultiPieceImage& other);
	~MultiPieceImage();

	Image* Clone();

	void Set(int state, Drawable *drawable);
	Drawable* Get(int state);

	IStateful* GetStateful() const { return (IStateful*)this; }
	int GetWidth() const;
	int GetHeight() const;
	void SetAttribute(const char* name, const char* value);
	void Draw(const Canvas &canvas, const Rect &rect) const;
};

} // namespace ui

#endif // _IMAGE_H
