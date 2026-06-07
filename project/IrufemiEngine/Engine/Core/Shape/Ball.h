#pragma once

#include "../Math/Vector3.h"

struct Ball {
    //ボールの位置
	Vector3 position;
    //ボールの速度
	Vector3 velocity;
    //ボールの加速度
	Vector3 acceleration;
    //ボールの質量
    float mass;
    //ボールの半径
    float radius;
    //ボールの色
    unsigned int color;
};