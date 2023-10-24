#pragma once

#include <list>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <stdlib.h>

const int kSamplesPerFrame = 640;
const int kDataSize = kSamplesPerFrame * sizeof(short);

struct AudioFrame
{
	time_t pts;
	unsigned char* data;
	int len;

	AudioFrame()
	{
		data = new unsigned char[kDataSize];
		len = 0;
	}
	AudioFrame(const AudioFrame& other)
	{
		pts = other.pts;
		data = new unsigned char[kDataSize];
		memcpy(data, other.data, other.len);
		len = other.len;
	}
	~AudioFrame()
	{
		delete[] data;
		data = nullptr;
	}

	void SetData(const unsigned char* buf, int size)
	{
		int n = size < (kDataSize - len) ? size : (kDataSize - len);
		memcpy(data + len, buf, n);
		len += n;
	}

	bool IsFull()
	{
		return len == kDataSize;
	}

	int GetFreeSize()
	{
		return kDataSize - len;
	}

	void Reset()
	{
		len = 0;
	}
};

struct VideoFrame
{
	time_t pts;
	unsigned char* data;
	int len;

	VideoFrame()
	{
		data = nullptr;
		len = 0;
	}
	VideoFrame(const VideoFrame& other)
	{
		pts = other.pts;
		data = new unsigned char[other.len];
		memcpy(data, other.data, other.len);
		len = other.len;
	}
	~VideoFrame()
	{
		if (data)
		{
			delete[] data;
			data = nullptr;
		}
	}

	void SetData(const unsigned char* buf, int size)
	{
		if (!data)
			data = new unsigned char[size];
		memcpy(data, buf, size);
		len = size;
	}
};

///////////////////////////////////////////////////////////////////////////////

template <class Type>
class FrameQueue
{
	Type* elements_;
	std::atomic<int> front_;
	std::atomic<int> rear_;
	int max_size_;

public:
	FrameQueue(int max_size)
	{
		elements_ = new Type[max_size];
		max_size_ = max_size;
		for (int i = 0; i < max_size_; i++)
			elements_[i] = nullptr;
		front_ = 0;
		rear_ = 0;
	}
	~FrameQueue()
	{
		delete[] elements_;
		elements_ = nullptr;
		max_size_ = 0;
		front_ = 0;
		rear_ = 0;
	}

	bool IsEmpty()
	{
		return front_ == rear_;
	}

	bool IsFull()
	{
		return (rear_ + 1) % max_size_ == front_;
	}

	int GetSize()
	{
		return (rear_ - front_ + max_size_) % max_size_;
	}

	void EnQueue(const Type& item)
	{
		rear_ = (rear_ + 1) % max_size_;
		elements_[rear_] = item;
	}

	Type DeQueue()
	{
		front_ = (front_ + 1) % max_size_;
		return elements_[front_];
	}

	Type Peek()
	{
		return elements_[(front_ + 1) % max_size_];
	}
};