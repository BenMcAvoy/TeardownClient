#pragma once

#include "common.h"

TEAR_BEGIN

class PlayerData {
public:
	TEAR_PROP(id, int32_t, 0x0);
};

struct PlayerDataWrapper {
	PlayerData* data;
};

TEAR_END
