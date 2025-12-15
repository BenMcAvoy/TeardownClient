#pragma once

#include "common.h"
#include "types.h"

#include "script.h"

TEAR_BEGIN

class Scene {
public:
	TEAR_PROP(scripts, Vector<Script*>*, 0x280);
};

TEAR_END
