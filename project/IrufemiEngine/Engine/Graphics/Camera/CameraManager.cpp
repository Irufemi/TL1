#include "CameraManager.h"
#include <cassert>

void CameraManager::AddCamera(const std::string& name, std::shared_ptr<Camera> camera) {
    if (camera) {
        cameras_[name] = camera;
        // 初めて追加されたカメラをアクティブにする
        if (activeCameraName_.empty()) {
            activeCameraName_ = name;
        }
    }
}

void CameraManager::RemoveCamera(const std::string& name) {
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
        cameras_.erase(it);
        if (activeCameraName_ == name) {
            activeCameraName_.clear();
            // 代わりのカメラを適当に設定する
            if (!cameras_.empty()) {
                activeCameraName_ = cameras_.begin()->first;
            }
        }
    }
}

void CameraManager::SetActiveCamera(const std::string& name) {
    if (cameras_.contains(name)) {
        activeCameraName_ = name;
    }
}

Camera* CameraManager::GetActiveCamera() const {
    auto it = cameras_.find(activeCameraName_);
    if (it != cameras_.end()) {
        return it->second.get();
    }
    return nullptr;
}

Camera* CameraManager::GetCamera(const std::string& name) const {
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void CameraManager::Update() {
    for (auto& pair : cameras_) {
        if (pair.second) {
            pair.second->Update();
        }
    }
}

void CameraManager::OnResize(int width, int height) {
    for (auto& pair : cameras_) {
        if (pair.second) {
            pair.second->Initialize(width, height);
        }
    }
}
