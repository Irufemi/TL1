#pragma once
#include <cstdint>
#include "../../Engine/Graphics/DirectX/DirectXCommon.h" // kMaxFramesInFlight

/**
 * @class MultiBufferSyncState
 * @brief 定数バッファ等のマルチバッファリングにおける同期フラグ状態を管理するMixinクラス
 * @details 各フレームごとの「バッファ更新が必要か」という状態（ダーティフラグ）を管理します。
 * 主にBaseResourceやIRenderable実装クラスに継承させて利用します。
 */
class MultiBufferSyncState {
public:
    MultiBufferSyncState() {
        // 初期状態ではすべてのフレームのバッファ更新が必要
        MarkAsDirty();
    }
    
    virtual ~MultiBufferSyncState() = default;

    /**
     * @brief 全フレームバッファをダーティ状態（更新が必要）にする
     * @details オブジェクトの座標や色、マテリアルパラメータなどが変更された際に呼び出します。
     */
    virtual void MarkAsDirty() {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            isDirtyBuffer_[i] = true;
        }
    }

    /**
     * @brief 指定したフレームバッファがダーティか確認し、ダーティであればフラグを下ろす
     * @param frameIndex 確認するフレームインデックス（通常はDirectXCommonから取得）
     * @return ダーティだった場合は true
     */
    bool CheckAndClearDirty(uint32_t frameIndex) {
        if (isDirtyBuffer_[frameIndex]) {
            isDirtyBuffer_[frameIndex] = false;
            return true;
        }
        return false;
    }

protected:
    bool isDirtyBuffer_[kMaxFramesInFlight] = { true, true, true };
};
