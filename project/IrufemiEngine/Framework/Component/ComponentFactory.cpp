#include "ComponentFactory.h"
#include "TransformComponent.h"
#include "Renderer/MeshRendererComponent.h"
#include "Renderer/PrimitiveRendererComponent.h"
#include "Renderer/SpriteRendererComponent.h"
#include "Renderer/TextRendererComponent.h"
#include "Collider/AABBColliderComponent.h"
#include "Collider/SphereColliderComponent.h"
#include "Collider/OBBColliderComponent.h"
#include "Collider/RaycastComponent.h"
#include "Script/RotatorComponent.h"
#include "AudioSourceComponent.h"
#include "Renderer/ParticleEmitterComponent.h"
#include "UI/ButtonComponent.h"
#include "UI/CanvasComponent.h"
#include "Camera/CameraComponent.h"

std::map<std::string, ComponentFactory::CreatorFunc>& ComponentFactory::GetMap() {
    static std::map<std::string, CreatorFunc> factoryMap;
    return factoryMap;
}

void ComponentFactory::Register(const std::string& typeName, CreatorFunc func) {
    GetMap()[typeName] = func;
}

std::shared_ptr<Component> ComponentFactory::Create(const std::string& typeName) {
    auto& map = GetMap();
    if (map.find(typeName) != map.end()) {
        return map[typeName]();
    }
    return nullptr;
}

const std::map<std::string, ComponentFactory::CreatorFunc>& ComponentFactory::GetFactoryMap() {
    return GetMap();
}

void ComponentFactory::RegisterAllCoreComponents() {
    Register("TransformComponent", []() { return std::make_shared<TransformComponent>(); });
    Register("MeshRendererComponent", []() { return std::make_shared<MeshRendererComponent>(); });
    Register("PrimitiveRendererComponent", []() { return std::make_shared<PrimitiveRendererComponent>(); });
    Register("SpriteRendererComponent", []() { return std::make_shared<SpriteRendererComponent>(); });
    Register("TextRendererComponent", []() { return std::make_shared<TextRendererComponent>(); });
    Register("AABBColliderComponent", []() { return std::make_shared<AABBColliderComponent>(); });
    Register("SphereColliderComponent", []() { return std::make_shared<SphereColliderComponent>(); });
    Register("OBBColliderComponent", []() { return std::make_shared<OBBColliderComponent>(); });
    Register("RaycastComponent", []() { return std::make_shared<RaycastComponent>(); });
    Register("RotatorComponent", []() { return std::make_shared<RotatorComponent>(); });
    Register("AudioSourceComponent", []() { return std::make_shared<AudioSourceComponent>(); });
    Register("ParticleEmitterComponent", []() { return std::make_shared<ParticleEmitterComponent>(); });
    Register("ButtonComponent", []() { return std::make_shared<ButtonComponent>(); });
    Register("CanvasComponent", []() { return std::make_shared<CanvasComponent>(); });
    Register("CameraComponent", []() { return std::make_shared<CameraComponent>(); });
}
