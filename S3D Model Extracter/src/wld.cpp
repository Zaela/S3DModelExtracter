
#include "wld.h"

namespace WLD
{
	void DecodeName(void* in_name, uint32 len)
	{
		static byte hashval[] = {0x95,0x3A,0xC5,0x2A,0x95,0x7A,0x95,0x6A};
		byte* name = (byte*)in_name;
		for (uint32 i = 0; i < len; ++i)
		{
			name[i] ^= hashval[i & 7];
		}
	}

	const char* GetName(const char* string_block, const int strings_len, int ref)
	{
		if (ref < 0 && -ref < strings_len)
			return &string_block[-ref];
		return nullptr;
	}

	int Read(lua_State* L)
	{
		//data table at 1
		lua_getfield(L, 1, "ptr");
		byte* ptr = (byte*)lua_touserdata(L, -1);
		lua_pop(L, 1);

		Header* header = (Header*)ptr;
		if (header->magicWord != Header::MAGIC)
			return luaL_argerror(L, 1, "file is not a valid WLD file");
		int version = header->version & 0xFFFFFFFE;
		if (version != Header::VERSION1 && version != Header::VERSION2)
			return luaL_argerror(L, 1, "file is not a valid WLD version");

		uint32 pos = Header::SIZE;
		const char* string_block = (const char*)&ptr[pos];
		const int strings_len = header->nameHashLen;

		byte* str = new byte[strings_len];
		memcpy(str, string_block, strings_len);
		DecodeName(str, header->nameHashLen);

		string_block = (const char*)str;
		pos += header->nameHashLen;

		lua_createtable(L, 0, 1); //to return

		lua_pushlightuserdata(L, str);
		lua_setfield(L, -2, "string_block");

		luaL_getmetatable(L, "WLDFrag");

		const int n = header->maxFragment;
		int tbl_pos = 0;
		for (int i = 1; i <= n; ++i)
		{
			FragHeader* fh = (FragHeader*)&ptr[pos];

			if (fh->type == 0x14)
			{
				lua_pushinteger(L, ++tbl_pos);
				LuaFrag* lf = (LuaFrag*)lua_newuserdata(L, sizeof(LuaFrag));
				lf->len = fh->len;
				lf->type = fh->type;
				lf->name = GetName(string_block, strings_len, fh->nameref);
				lf->offset = pos;

				lua_pushvalue(L, -3);
				lua_setmetatable(L, -2);

				lua_settable(L, -4);
			}

			pos += FragHeader::SIZE + fh->len;
		}

		lua_pop(L, 1); //metatable

		return 1;
	}

	int Close(lua_State* L)
	{
		//wld list table
		lua_getfield(L, 1, "string_block");
		byte* str = (byte*)lua_touserdata(L, -1);

		delete[] str;

		return 0;
	}

	int Extract(lua_State* L)
	{
		//wld entry table, string_block userdata, WLDFrag userdata, 3 letter model identifier
		const char* string_block = (const char*)lua_touserdata(L, 2);
		LuaFrag* lf = (LuaFrag*)luaL_checkudata(L, 3, "WLDFrag");
		const char* model_name = luaL_checkstring(L, 4);

		lua_getfield(L, 1, "ptr");
		byte* base_data = (byte*)lua_touserdata(L, -1);
		lua_getfield(L, 1, "inflated_len");
		uint32 len = lua_tointeger(L, -1);
		byte* data = new byte[len];
		memcpy(data, base_data, len);
		lua_pop(L, 2);

		Header* old_header = (Header*)data;
		const uint32 strings_len = old_header->nameHashLen;
		uint32 pos = Header::SIZE + strings_len;
		const int frag_count = old_header->maxFragment;

		std::vector<FragHeader*> frags_by_pos;
		std::unordered_map<const char*, FragHeader*> frags_by_name;
		std::unordered_map<FragHeader*, int> positions_by_frag;
		std::vector<FragHeader*> anim_frags;

		//get all fragments into the lists so we can follow references later
		for (int i = 1; i <= frag_count; ++i)
		{
			FragHeader* frag = (FragHeader*)&data[pos];

			frags_by_pos.push_back(frag);
			positions_by_frag[frag] = i;
			if (frag->type == 0x12 || frag->type == 0x13)
				anim_frags.push_back(frag);
			const char* name = GetName(string_block, strings_len, frag->nameref);
			if (name)
				frags_by_name[name] = frag;

			pos += FragHeader::SIZE + frag->len;
		}

		lua_newtable(L); //image name list, -1
		int texture_name_count = 0;

		//to be read backwards
		std::vector<FragHeader*> top_section; //0x14, 0x11, 0x10, 0x2D
		std::vector<FragHeader*> anim_section; //0x13, 0x12
		std::vector<FragHeader*> mesh_section; //0x36
		std::vector<FragHeader*> texture_section; //0x31, 0x30, 0x05, 0x04, 0x03

		auto GetFragByRef = [&](int ref) -> FragHeader*
		{
			if (ref > 0 && ref <= frag_count)
				return frags_by_pos[ref - 1];
			else if (ref < 0)
			{
				uint32 index = /*(ref == 0) ? 1 :*/ -ref;
				if (index < strings_len)
				{
					const char* name = &string_block[index];
					if (frags_by_name.count(name))
						return frags_by_name[name];
				}
			}
			return nullptr;
		};

		std::unordered_set<FragHeader*> handled_frags;

		std::function<void(FragHeader*)> Recurse;
		Recurse = [&, L](FragHeader* frag)
		{
			if (handled_frags.count(frag))
				return;
			handled_frags.insert(frag);

			switch (frag->type)
			{
			case 0x03:
			{
				texture_section.push_back(frag);
				Frag03* f = (Frag03*)frag;
				char name[64];
				memcpy(name, f->name, f->len);
				DecodeName(name, f->len);
				lua_pushinteger(L, ++texture_name_count);
				lua_pushstring(L, name);
				lua_settable(L, -3);
				break;
			}
			case 0x04:
			{
				texture_section.push_back(frag);
				Frag04* f = (Frag04*)frag;
				FragHeader* g;
				if (f->num_refs <= 1)
				{
					g = GetFragByRef(f->ref);
					if (g) Recurse(g);
				}
				else
				{
					int* ref = (int*)&f->ref;
					++ref;
					for (int i = 0; i < f->num_refs; ++i)
					{
						g = GetFragByRef(*ref++);
						if (g) Recurse(g);
					}
				}
				break;
			}
			case 0x05:
			{
				texture_section.push_back(frag);
				Frag05* f = (Frag05*)frag;
				FragHeader* g = GetFragByRef(f->ref);
				if (g) Recurse(g);
				break;
			}
			case 0x30:
			{
				texture_section.push_back(frag);
				Frag30* f = (Frag30*)frag;
				FragHeader* g = GetFragByRef(f->ref);
				if (g) Recurse(g);
				break;
			}
			case 0x31:
			{
				texture_section.push_back(frag);
				Frag31* f = (Frag31*)frag;
				FragHeader* g;
				for (int i = 0; i < f->size; ++i)
				{
					g = GetFragByRef(f->ref[i]);
					if (g) Recurse(g);
				}
				break;
			}
			case 0x36:
			{
				mesh_section.push_back(frag);
				Frag36* f = (Frag36*)frag;
				FragHeader* g;
				for (int i = 0; i < 4; ++i)
				{
					g = GetFragByRef(f->ref[i]);
					if (g) Recurse(g);
				}
				break;
			}
			case 0x12:
			{
				anim_section.push_back(frag);
				break;
			}
			case 0x13:
			{
				anim_section.push_back(frag);
				Frag13* f = (Frag13*)frag;
				FragHeader* g = GetFragByRef(f->ref);
				if (g) Recurse(g);
				break;
			}
			case 0x2D:
			{
				top_section.push_back(frag);
				Frag2D* f = (Frag2D*)frag;
				FragHeader* g = GetFragByRef(f->ref);
				if (g) Recurse(g);
				break;
			}
			case 0x10:
			{
				top_section.push_back(frag);
				Frag10* f = (Frag10*)frag;
				FragHeader* g = GetFragByRef(f->ref1);
				if (g) Recurse(g);

				byte* d = (byte*)f + sizeof(Frag10);
				if (f->flag & (1 << 0))
					d += sizeof(int) * 3;
				if (f->flag & (1 << 1))
					d += sizeof(float);

				for (int i = 0; i < f->size1; ++i)
				{
					BoneEntry* b = (BoneEntry*)d;
					g = GetFragByRef(b->ref1);
					if (g) Recurse(g);
					g = GetFragByRef(b->ref2);
					if (g) Recurse(g);
					d += sizeof(BoneEntry) + b->size * sizeof(int);
				}

				if (f->flag & (1 << 9))
				{
					const int size2 = *(int*)d;
					d += sizeof(int);
					int* ref = (int*)d;
					for (int i = 0; i < size2; ++i)
					{
						g = GetFragByRef(*ref++);
						if (g) Recurse(g);
					}
				}
				break;
			}
			case 0x11:
			{
				top_section.push_back(frag);
				Frag11* f = (Frag11*)frag;
				FragHeader* g = GetFragByRef(f->ref);
				if (g) Recurse(g);
				break;
			}
			case 0x14:
			{
				top_section.push_back(frag);
				Frag14* f = (Frag14*)frag;
				FragHeader* g = GetFragByRef(f->ref1);
				if (g) Recurse(g);
				g = GetFragByRef(f->ref2);
				if (g) Recurse(g);

				//lots of variable size skipping
				byte* d = (byte*)f + sizeof(Frag14);
				if (f->flag & (1 << 0))
					d += sizeof(int);
				if (f->flag & (1 << 1))
					d += sizeof(int);
				for (int i = 0; i < f->size[0]; ++i)
				{
					int* size = (int*)d;
					d += *size * 8 + sizeof(int);
				}

				int* ref = (int*)d;
				for (int i = 0; i < f->size[1]; ++i)
				{
					g = GetFragByRef(*ref++);
					if (g) Recurse(g);
				}
				break;
			}
			default:
				break;
			}
		};

		Recurse((FragHeader*)&data[lf->offset]);

		std::string str = "\\b[A-Z][0-9][0-9]";
		str += model_name;
		std::regex reg(str);

		std::vector<FragHeader*> anim_section2;

		for (FragHeader* frag : anim_frags)
		{
			const char* name = GetName(string_block, strings_len, frag->nameref);
			if (name && std::regex_search(name, reg))
				anim_section2.push_back(frag);
		}

		//now that we finally have all that... time to actually start doing the work:
		//making the new wld and translating reference indices

		std::unordered_map<const char*, int> string_positions;
		std::unordered_map<int, int> position_translations;

		Buffer string_buf;
		Buffer frag_buf;
		int cur_pos = 1; //positive references are +1
		char null = 0;
		string_buf.Add(&null, sizeof(char)); //first byte of string block is not referencible

		auto TranslateRef = [&](int ref) -> int
		{
			if (ref > 0)
				return position_translations[ref];
			else if (ref < 0)
			{
				uint32 index = -ref;
				if (index < strings_len)
				{
					const char* name = &string_block[index];
					if (string_positions.count(name))
						return string_positions[name];
				}
			}
			return 0;
		};

		auto WriteFragment = [&](FragHeader* frag)
		{
			int pos = positions_by_frag[frag];
			position_translations[pos] = cur_pos++;

			const char* name = GetName(string_block, strings_len, frag->nameref);
			if (name)
			{
				int buf_pos;
				if (string_positions.count(name))
				{
					buf_pos = string_positions[name];
				}
				else
				{
					int b = string_buf.GetLen();
					buf_pos = -b;
					string_buf.Add(name, strlen(name) + 1);
					string_positions[name] = buf_pos;
				}
				frag->nameref = buf_pos;
			}

			switch (frag->type)
			{
			case 0x04:
			{
				Frag04* f = (Frag04*)frag;
				if (f->num_refs <= 1)
				{
					f->ref = TranslateRef(f->ref);
				}
				else
				{
					int* ref = (int*)&f->ref;
					++ref;
					for (int i = 0; i < f->num_refs; ++i)
					{
						*ref = TranslateRef(*ref);
						++ref;
					}
				}
				break;
			}
			case 0x05:
			{
				Frag05* f = (Frag05*)frag;
				f->ref = TranslateRef(f->ref);
				break;
			}
			case 0x30:
			{
				Frag30* f = (Frag30*)frag;
				f->ref = TranslateRef(f->ref);
				break;
			}
			case 0x31:
			{
				Frag31* f = (Frag31*)frag;
				for (int i = 0; i < f->size; ++i)
				{
					f->ref[i] = TranslateRef(f->ref[i]);
				}
				break;
			}
			case 0x36:
			{
				Frag36* f = (Frag36*)frag;
				for (int i = 0; i < 4; ++i)
				{
					f->ref[i] = TranslateRef(f->ref[i]);
				}
				break;
			}
			case 0x13:
			{
				Frag13* f = (Frag13*)frag;
				f->ref = TranslateRef(f->ref);
				break;
			}
			case 0x2D:
			{
				Frag2D* f = (Frag2D*)frag;
				f->ref = TranslateRef(f->ref);
				break;
			}
			case 0x10:
			{
				Frag10* f = (Frag10*)frag;
				f->ref1 = TranslateRef(f->ref1);

				byte* d = (byte*)f + sizeof(Frag10);
				if (f->flag & (1 << 0))
					d += sizeof(int) * 3;
				if (f->flag & (1 << 1))
					d += sizeof(float);

				for (int i = 0; i < f->size1; ++i)
				{
					BoneEntry* b = (BoneEntry*)d;
					b->ref1 = TranslateRef(b->ref1);
					b->ref2 = TranslateRef(b->ref2);
					d += sizeof(BoneEntry) + b->size * sizeof(int);
				}

				if (f->flag & (1 << 9))
				{
					const int size2 = *(int*)d;
					d += sizeof(int);
					int* ref = (int*)d;
					for (int i = 0; i < size2; ++i)
					{
						*ref = TranslateRef(*ref);
						++ref;
					}
				}
				break;
			}
			case 0x11:
			{
				Frag11* f = (Frag11*)frag;
				f->ref = TranslateRef(f->ref);
				break;
			}
			case 0x14:
			{
				Frag14* f = (Frag14*)frag;
				f->ref1 = TranslateRef(f->ref1);
				f->ref2 = TranslateRef(f->ref2);

				//lots of variable size skipping
				byte* d = (byte*)f + sizeof(Frag14);
				if (f->flag & (1 << 0))
					d += sizeof(int);
				if (f->flag & (1 << 1))
					d += sizeof(int);
				for (int i = 0; i < f->size[0]; ++i)
				{
					int* size = (int*)d;
					d += *size * 8 + sizeof(int);
				}

				int* ref = (int*)d;
				for (int i = 0; i < f->size[1]; ++i)
				{
					*ref = TranslateRef(*ref);
					++ref;
				}
				break;
			}
			default:
				break;
			}

			frag_buf.Add(frag, FragHeader::SIZE + frag->len);
		};

		auto HandleReverseVector = [&](std::vector<FragHeader*> vec)
		{
			for (int i = vec.size() - 1; i >= 0; --i)
			{
				WriteFragment(vec[i]);
			}
		};

		HandleReverseVector(texture_section);
		HandleReverseVector(mesh_section);
		HandleReverseVector(anim_section);
		HandleReverseVector(top_section);

		for (FragHeader* frag : anim_section2)
		{
			WriteFragment(frag);
		}

		//create final form of file
		int slen = string_buf.GetLen();
		Header header;
		header = *old_header; //hopefully the unknown fields aren't too important
		header.maxFragment = cur_pos - 1;
		header.nameHashLen = slen;

		Buffer buf;
		buf.Add(&header, Header::SIZE);

		byte* b = string_buf.Take();
		DecodeName(b, slen);
		buf.Add(b, slen);
		delete[] b;

		b = frag_buf.Take();
		buf.Add(b, frag_buf.GetLen());
		delete[] b;

		int footer = 0xFFFFFFFF;
		buf.Add(&footer, sizeof(int));

		b = buf.Take();
		lua_pushlightuserdata(L, b);
		lua_pushinteger(L, buf.GetLen());

		delete[] data;

		return 3;
	}

	int Rename(lua_State* L)
	{
		const char* from = luaL_checkstring(L, 2);
		const char* to = luaL_checkstring(L, 3);

		lua_getfield(L, 1, "ptr");
		byte* data = (byte*)lua_touserdata(L, -1);
		lua_pop(L, 1);

		Header* header = (Header*)data;
		const uint32 strings_len = header->nameHashLen;
		char* end = (char*)&data[Header::SIZE + strings_len];
		char* str = (char*)&data[Header::SIZE];

		DecodeName(str, strings_len);

		while (str < end)
		{
			char* match = strstr(str, from);
			if (match && (match - str) <= 4)
			{
				memcpy(match, to, 3);
			}
			str += strlen(str) + 1;
		}

		DecodeName(&data[Header::SIZE], strings_len);
		return 0;
	}

	int GetName(lua_State* L)
	{
		LuaFrag* lf = (LuaFrag*)luaL_checkudata(L, 1, "WLDFrag");
		lua_pushstring(L, lf->name ? lf->name : "<Unnamed>");
		return 1;
	}

	static const luaL_Reg funcs[] = {
		{"Read", Read},
		{"Close", Close},
		{"Extract", Extract},
		{"Rename", Rename},
		{nullptr, nullptr}
	};

	static const luaL_Reg obj_funcs[] = {
		{"GetName", GetName},
		{nullptr, nullptr}
	};

	void LoadFunctions(lua_State* L)
	{
		luaL_register(L, "wld", funcs);

		luaL_newmetatable(L, "WLDFrag");
		luaL_register(L, nullptr, obj_funcs);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		lua_pop(L, 1);
	}
}
