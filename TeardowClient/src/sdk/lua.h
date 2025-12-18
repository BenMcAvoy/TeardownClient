#pragma once

#include "types.h"

typedef int (*lua_CFunction)(lua_State *L);

using GetField_t = int(__fastcall*)(void* L, int idx, const char* k);
using PushString_t = void(__fastcall*)(void* L, const char* s);
using PCall_t = int(__fastcall*)(void* L, int nargs, int nresults, int errfunc);
using ToString_t = const char* (__fastcall*)(void* L, int idx);
using PushCClosure_t = void(__fastcall*)(void*, lua_CFunction fn, int n);
using SetField_t = void(__fastcall*)(void* L, int idx, const char* k);

namespace Lua {
	inline GetField_t GetField = nullptr;
	inline PushString_t PushString = nullptr;
	inline PCall_t PCall = nullptr;
	inline ToString_t ToString = nullptr;
	inline PushCClosure_t PushCClosure = nullptr;
	inline SetField_t SetField = nullptr;

	constexpr int GLOBALS_INDEX = -10002;

    inline void Pop(lua_State* L, int n) {
        L->top -= n;
	}

    inline int GetTop(lua_State* L) {
        return static_cast<int>(L->top - L->base);
    }

    inline void SetTop(lua_State* L, int idx) {
        L->top = L->base + idx;
    }
}

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8

enum class LuaType {
    NIL = LUA_TNIL,
    BOOLEAN = LUA_TBOOLEAN,
    LIGHTUSERDATA = LUA_TLIGHTUSERDATA,
    NUMBER = LUA_TNUMBER,
    STRING = LUA_TSTRING,
    TABLE = LUA_TTABLE,
    FUNCTION = LUA_TFUNCTION,
    USERDATA = LUA_TUSERDATA,
    THREAD = LUA_TTHREAD,
    UNKNOWN = -1
};

inline LuaType GetLuaType(lua_State* L, int idx) {
    LuaStackID addr;
    if (idx > 0) {
        addr = L->base + (idx - 1);
    }
    else if (idx > -10000) {
        addr = L->top + idx;
    }
    else {
        return LuaType::UNKNOWN;
    }

    if (addr >= L->top || addr < L->base)
        return LuaType::UNKNOWN;

    return (LuaType)addr->tt;
}
