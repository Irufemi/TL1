#pragma once
#include "Framework/Component/Component.h"
#include "Engine/Core/Math/Vector3.h"

class DebugEnemySpawnerComponent : public Component {
public:
    DebugEnemySpawnerComponent() = default;
    ~DebugEnemySpawnerComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override {}
    std::string GetComponentName() const override { return "DebugEnemySpawnerComponent"; }

private:
    void SpawnEnemy(const Vector3& position);
};
