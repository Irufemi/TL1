#include "RailPathComponent.h"
#include <algorithm>
#include <cmath>

void RailPathComponent::OnRegisterProperties() {
    // 拡張した Float3Array を使ってウェイポイントをプロパティに登録
    RegisterProperty("Waypoints", &waypoints_);
}

Vector3 RailPathComponent::GetPointAt(float t) const {
    if (waypoints_.empty()) return {0.0f, 0.0f, 0.0f};
    if (waypoints_.size() == 1) return waypoints_[0];

    t = std::clamp(t, 0.0f, 1.0f);
    
    // セグメント数
    int segments = static_cast<int>(waypoints_.size()) - 1;
    // 現在のtが属するセグメント
    float scaledT = t * segments;
    int index = static_cast<int>(scaledT);
    if (index >= segments) {
        index = segments - 1;
        scaledT = static_cast<float>(segments);
    }
    
    // セグメント内のローカルt (0.0 ~ 1.0)
    float localT = scaledT - index;

    // Catmull-Rom スプライン補間のための制御点4つを取得
    Vector3 p0 = waypoints_[std::max(0, index - 1)];
    Vector3 p1 = waypoints_[index];
    Vector3 p2 = waypoints_[std::min(segments, index + 1)];
    Vector3 p3 = waypoints_[std::min(segments, index + 2)];

    float t2 = localT * localT;
    float t3 = t2 * localT;

    Vector3 result;
    result.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * localT +
                       (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                       (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
                       
    result.y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * localT +
                       (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                       (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
                       
    result.z = 0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * localT +
                       (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
                       (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);

    return result;
}

Vector3 RailPathComponent::GetTangentAt(float t) const {
    if (waypoints_.size() < 2) return {0.0f, 0.0f, 1.0f}; // デフォルトの進行方向
    
    // 少し先の点を計算して差分から接線を求める (簡易的な近似)
    float delta = 0.01f;
    float t1 = std::clamp(t, 0.0f, 1.0f);
    float t2 = std::clamp(t + delta, 0.0f, 1.0f);
    
    // もし終端に近ければ、少し前の点から計算する
    if (t >= 1.0f - delta) {
        t1 = std::clamp(t - delta, 0.0f, 1.0f);
        t2 = std::clamp(t, 0.0f, 1.0f);
    }
    
    Vector3 p1 = GetPointAt(t1);
    Vector3 p2 = GetPointAt(t2);
    
    Vector3 tangent = {p2.x - p1.x, p2.y - p1.y, p2.z - p1.z};
    float length = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
    
    if (length > 0.0001f) {
        tangent.x /= length;
        tangent.y /= length;
        tangent.z /= length;
    } else {
        tangent = {0.0f, 0.0f, 1.0f};
    }
    
    return tangent;
}
