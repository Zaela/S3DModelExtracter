
#include "buffer.h"

Buffer::Buffer() : mLen(0), mCap(8192)
{
	mData = new byte[8192];
}

void Buffer::Add(const void* in_data, uint32 len)
{
	const byte* data = (byte*)in_data;
	uint32 newlen = mLen + len;
	if (newlen >= mCap)
	{
		while (newlen >= mCap)
			mCap <<= 1;
		byte* add = new byte[mCap];
		memcpy(add, mData, mLen);
		delete[] mData;
		mData = add;
	}
	memcpy(&mData[mLen], data, len);
	mLen = newlen;
}

byte* Buffer::Take()
{
	byte* ret = mData;
	mData = nullptr;
	return ret;
}
