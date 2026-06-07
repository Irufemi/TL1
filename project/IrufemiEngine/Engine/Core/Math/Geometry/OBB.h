#pragma once

#include "../Vector3.h"

struct OBB {
    // !< 中心点
    Vector3 center;
    // !< 座標軸。正規化・直交必須
    Vector3 orientations[3];
    // !< 座標軸方向の長さの半分。中心から面までの距離
    Vector3 size;
};