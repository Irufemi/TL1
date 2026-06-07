#pragma once

#include "Joint.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>


struct Skeleton {
    // RootJointのIndex
    int32_t root;
    // Joint名とIndexとの辞書
    std::map<std::string, int32_t> jointMap;
    // 所属しているジョイント
    std::vector<Joint> joints;

    // --- 最適化用キャッシュ ---
    // 最後に適用したアニメーションのアドレス（変更検知用）
    const void* lastAppliedAnimation = nullptr;
    // Jointインデックスと対象NodeAnimationのポインタを紐付けたリスト
    // (std::map::find を回避するためのキャッシュ)
    std::vector<std::pair<int32_t, const struct NodeAnimation*>> activeAnimationBindings;
};