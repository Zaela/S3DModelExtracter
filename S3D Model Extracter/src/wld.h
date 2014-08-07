
#include <lua.hpp>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <regex>
#include "types.h"
#include "buffer.h"

namespace WLD
{
	void LoadFunctions(lua_State* L);

	struct Header
	{
		int magicWord;
		int version;
		int maxFragment;
		int unknownA[2];
		int nameHashLen;
		int unknownB;
		static const uint32 SIZE = sizeof(int) * 7;
		static const int MAGIC = 0x54503D02;
		static const int VERSION1 = 0x00015500;
		static const int VERSION2 = 0x1000C800;
	};

	struct FragHeader
	{
		uint32 len;
		uint32 type;
		static const uint32 SIZE = sizeof(uint32) * 2;
		int nameref;
	};

	struct LuaFrag
	{
		uint32 len;
		uint32 type;
		const char* name;
		uint32 offset;
	};

	struct Frag14
	{
		FragHeader header;
		uint32 flag;
		int ref1;
		int size[2];
		int ref2;
		//variable size part, followed by ref list
	};

	struct Frag11
	{
		FragHeader header;
		int ref;
		int param;
	};

	struct Frag10
	{
		FragHeader header;
		uint32 flag;
		int size1;
		int ref1;
		//variable size params
		//variable size list of bone entries followed by variable size list of refs
	};

	struct BoneEntry
	{
		int nameref;
		uint32 flag;
		int ref1;
		int ref2;
		int size;
		//variable size list of bone indices, each an int
	};

	struct Frag2D
	{
		FragHeader header;
		int ref;
		int param;
	};

	struct Frag13
	{
		FragHeader header;
		int ref;
	};

	struct Frag36
	{
		FragHeader header;
		uint32 flag;
		int ref[4];
	};

	struct Frag31
	{
		FragHeader header;
		uint32 flag;
		int size;
		int ref[2]; //variable size list of refs
	};

	struct Frag30
	{
		FragHeader header;
		uint32 flag;
		uint32 vis_flag;
		int param1;
		float param2;
		float param3;
		int ref;
	};

	struct Frag05
	{
		FragHeader header;
		int ref;
		uint32 flag;
	};

	struct Frag04
	{
		FragHeader header;
		uint32 flag;
		int num_refs;
		int ref; //if num_refs > 1, this field is anim milliseconds and list of refs follows
	};

	struct Frag03
	{
		FragHeader header;
		uint32 num_names;
		uint16 len;
		byte name[2];
	};
}
