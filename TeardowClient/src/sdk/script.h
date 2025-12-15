#pragma once

#include "common.h"

TEAR_BEGIN

class ScriptCore {
public:
	TEAR_PROP(L, lua_State**, 0xA0);
};

class Script {
public:
	TEAR_PROP(clientCore, ScriptCore*, 0x40);
	TEAR_PROP(serverCore, ScriptCore*, 0x48);

	TEAR_PROP(name, String, 0x88);
	TEAR_PROP(filePath, String, 0xA8);
};

TEAR_END
