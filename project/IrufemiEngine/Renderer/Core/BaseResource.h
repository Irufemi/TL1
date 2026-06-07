#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <memory>

#include "MultiBufferSyncState.h"

class DirectXCommon;

class BaseResource : public MultiBufferSyncState {
public:
    virtual ~BaseResource() = default;

    static void SetDirectXCommon(DirectXCommon* dxCommon) { s_dxCommon_ = dxCommon; }
    static DirectXCommon* GetDirectXCommon() { return s_dxCommon_; }

    virtual void CreateResource() = 0;
    virtual void Map() = 0;
    virtual void Unmap() = 0;

protected:
    static DirectXCommon* s_dxCommon_;
};
