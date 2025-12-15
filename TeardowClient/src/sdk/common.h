#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <span>
#include <string>

using lu_byte = uint8_t;

typedef struct GCObject GCObject;

struct LuaValue {
    uint64_t value;
    int tt;
    int extra;  // 16 bytes total
};

using LuaStackID = LuaValue*;


#define CommonHeader    GCObject *next; lu_byte tt; lu_byte marked
struct lua_State {
    CommonHeader;
    uint8_t status;

    uint8_t activememcat; // memory category that is used for new GC object allocations

    bool isactive;   // thread is currently executing, stack may be mutated without barriers
    bool singlestep; // call debugstep hook after each instruction

    LuaStackID top;                                        // first free slot in the stack
    LuaStackID base;                                       // base of current function
    void* global;
    void* ci;                                     // call info for current function
    void* stack_last;                                 // last free slot in the stack
    void* stack;                                      // stack base

    void* end_ci;                          // points after end of ci array
    void* base_ci;                         // array of CallInfo's

    int stacksize;
    int size_ci;                               // size of array `base_ci'

    unsigned short nCcalls;     // number of nested C calls
    unsigned short baseCcalls;  // nested C calls when resuming coroutine

    int cachedslot;    // when table operations or INDEX/NEWINDEX is invoked from Luau, what is the expected slot for lookup?

    void* gt;           // table of globals
    void* openupval;       // list of open upvalues in this stack
    void* gclist;

    void* namecall; // when invoked from Luau using NAMECALL, what method do we need to invoke?

    void* userdata;
};

// SDK property utilities:

#define TEAR_PROP(name, type, offset) \
    __declspec(property(get = get_##name, put = put_##name)) type name; \
        type get_##name() { return *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(this) + offset); } \
        void put_##name(type val) { *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(this) + offset) = val; }

#define TEAR_PROP_READ(name, type, offset) \
    __declspec(property(get = get_##name)) type name; \
        type get_##name() { return *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(this) + offset); }

#define TEAR_PROP_WRITE(name, type, offset) \
    __declspec(property(put = put_##name)) type name; \
        void put_##name(type val) { *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(this) + offset) = val; }

// Definition utilities:

#define TEAR_SIZE(type, size) \
    static_assert(sizeof(type) == size, "Size of " #type " is not " #size " bytes!");

#define TEAR_CLASS struct // For public by default

#define TEAR_BEGIN namespace Tear {
#define TEAR_END } // namespace Tear
#define TEAR_USING using namespace Tear;
#define TEAR_PAD(size) \
    uint8_t _pad##__LINE__[size];

const auto TEARBASE = (uintptr_t)GetModuleHandleA(nullptr);
#define TEAR_SINGLETON(type, offset) \
    static type* Get() { \
        return *reinterpret_cast<type**>(TEARBASE + offset); \
    }
