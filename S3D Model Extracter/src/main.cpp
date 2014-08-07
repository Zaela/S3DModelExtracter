
#include <lua.hpp>
#include <cstdio>

#include <iup.h>
#include <iuplua.h>
#include <iupcontrols.h>
#include <iupluacontrols.h>

#include "types.h"
#include "wld.h"

#ifdef _WIN32
#include <windows.h>
#endif

void ShowError(const char* fmt, const char* str)
{
#ifdef _WIN32
		char msg[1024];
		snprintf(msg, 1024, fmt, str);
		MessageBox(NULL, msg, NULL, MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
		printf(fmt, str);
#endif
}

#ifdef _WIN32
int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nCmdShow)
#else
int main()
#endif
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	IupOpen(nullptr, nullptr);
	IupControlsOpen();
	iuplua_open(L);
	iupcontrolslua_open(L);

	WLD::LoadFunctions(L);

	if (luaL_loadfile(L, "gui/main.lua") != 0)
	{
		ShowError("Could not load GUI script:\n%s\n", lua_tostring(L, -1));
	}
	else if (lua_pcall(L, 0, 0, 0) != 0)
	{
		ShowError("Runtime error:\n%s\n", lua_tostring(L, -1));
	}

	lua_close(L);
	return 0;
}
