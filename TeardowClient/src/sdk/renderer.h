#pragma once

#include "common.h"

#include <d3d12.h>
#include <dxgi.h>

TEAR_BEGIN

struct TRendererContext {
	virtual ~TRendererContext() = default;
}; // unknown context and size

class TRendererD3D12 : public TRendererContext {
public:
	TEAR_PROP_READ(device, ID3D12Device*, 0xE38);
	TEAR_PROP_READ(commandList, ID3D12GraphicsCommandList*, 0xE40);
	TEAR_PROP_READ(commandQueue, ID3D12CommandQueue*, 0xE50);
	TEAR_PROP_READ(swapchain, IDXGISwapChain*, 0xEC8);
};

TEAR_END
