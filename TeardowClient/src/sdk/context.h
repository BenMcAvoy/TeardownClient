#pragma once

#include "common.h"
#include "types.h"

#include "renderer.h"
#include "player.h"
#include "scene.h"

TEAR_BEGIN

class Context {
public:
	TEAR_PROP_READ(renderer, TRendererD3D12*, 0x30);
	TEAR_PROP_READ(scene, Scene*, 0x50);
	TEAR_PROP_READ(players, Vector<PlayerDataWrapper*>, 0xB8);
	TEAR_PROP_READ(localPlayerIdx, int32_t, 0x168);

	TEAR_SINGLETON(Context, 0xC350E0);

	PlayerDataWrapper* GetLocalPlayer() {
		int32_t idx = localPlayerIdx;

		if (idx < 0 || static_cast<size_t>(idx) >= players.size()) {
			return nullptr;
		}

		return players[idx];
	}

private:
	TEAR_PAD(0x918);
};
TEAR_SIZE(Context, 0x918);

TEAR_END
