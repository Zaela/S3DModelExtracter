
#include <cstdio>
#include <cstring>
#include "types.h"

class Buffer
{
public:
	Buffer();
	void Add(const void* in_data, uint32 len);
	byte* Take();
	uint32 GetLen() { return mLen; }
private:
	uint32 mLen;
	uint32 mCap;
	byte* mData;
};
