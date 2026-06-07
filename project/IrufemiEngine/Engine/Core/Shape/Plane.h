#pragma once

#include "../Math/Vector3.h"

//平面とは無限遠平面のこと。範囲に限りがない。

struct Plane{
    //!<法線(平面の向き)
	Vector3 normal{};

    //!<距離(原点から法線方向に離れている距離)
    float distance{};
};

