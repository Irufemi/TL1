#include "GPUParticleManager.h"
#include "../../Engine/Manager/DrawManager.h"
#include "../../Engine/IrufemiEngine.h"

GPUParticleManager* GPUParticleManager::GetInstance() {
    static GPUParticleManager instance;
    return &instance;
}

void GPUParticleManager::Initialize() {
    systems_.clear();
}

void GPUParticleManager::Update() {
    for (auto& pair : systems_) {
        pair.second.system->Update();
    }
}

void GPUParticleManager::Draw() {
    for (auto& pair : systems_) {
        pair.second.system->Draw();
    }
}

void GPUParticleManager::Finalize() {
    systems_.clear();
}

GPUParticleManager::EmitterHandle GPUParticleManager::RegisterEmitter(const std::string& texturePath, BlendMode blendMode, bool isUnscaledTime) {
    SystemKey key{ texturePath, blendMode, isUnscaledTime };
    auto& ctx = systems_[key];

    // 新規テクスチャの場合はシステムを初期化
    if (!ctx.system) {
        ctx.system = std::make_unique<GPUParticleSystem>();
        ctx.system->Initialize(texturePath);
        ctx.system->SetBlendMode(blendMode);         // TODO: GPUParticleSystem に SetBlendMode を追加予定
        ctx.system->SetUnscaledTime(isUnscaledTime); // TODO: GPUParticleSystem に SetUnscaledTime を追加予定
    }
    
    uint32_t assignedIndex = 0;
    if (!ctx.freeIndices.empty()) {
        assignedIndex = ctx.freeIndices.back();
        ctx.freeIndices.pop_back();
    } else {
        assignedIndex = ctx.nextIndex++;
        // Resize emittersData_ to accommodate the new index if needed
        if (assignedIndex >= ctx.system->emittersData_.size()) {
            ctx.system->emittersData_.resize(assignedIndex + 1);
        }
    }
    
    // Initialize the slot
    ctx.system->emittersData_[assignedIndex] = GPUParticleEmitter();
    
    EmitterHandle handle;
    handle.system = ctx.system.get();
    handle.emitterIndex = assignedIndex;
    return handle;
}

void GPUParticleManager::UnregisterEmitter(const EmitterHandle& handle) {
    if (!handle.IsValid()) return;
    
    // Find the context to add free index
    for (auto& pair : systems_) {
        if (pair.second.system.get() == handle.system) {
            // Disable emission
            if (handle.emitterIndex < pair.second.system->emittersData_.size()) {
                pair.second.system->emittersData_[handle.emitterIndex].emit = 0;
            }
            pair.second.freeIndices.push_back(handle.emitterIndex);
            break;
        }
    }
}

void GPUParticleManager::UpdateEmitterData(const EmitterHandle& handle, const GPUParticleEmitter& data) {
    if (handle.IsValid() && handle.emitterIndex < handle.system->emittersData_.size()) {
        // burstCount is additive in our system, so we accumulate it from the incoming data and clear the incoming data's burstCount?
        // Actually, ParticleEmitterComponent might send burstCount. We add it and reset component's.
        uint32_t burst = handle.system->emittersData_[handle.emitterIndex].burstCount + data.burstCount;
        
        handle.system->emittersData_[handle.emitterIndex] = data;
        
        handle.system->emittersData_[handle.emitterIndex].burstCount = burst;
    }
}

#if defined(USE_IMGUI)
#include <imgui.h>
#endif
void GPUParticleManager::Debug() {
#if defined(USE_IMGUI)
    if (ImGui::BeginTabItem("GPU Particle Manager")) {
        ImGui::Text("System Statistics");
        ImGui::Separator();
        ImGui::Text("Active Particle Systems (Textures): %d", (int)systems_.size());
        
        int totalEmitters = 0;
        int maxEmitters = static_cast<int>(systems_.size()) * GPUParticleSystem::kMaxEmitters;
        for (const auto& pair : systems_) {
            totalEmitters += (int)(GPUParticleSystem::kMaxEmitters - pair.second.freeIndices.size());
        }
        
        ImGui::Text("Total Emitters Used: %d / %d", totalEmitters, maxEmitters);
        ImGui::Separator();
        
        ImGui::Spacing();
        ImGui::Text("System Details per Texture");
        for (auto& pair : systems_) {
            const std::string& textureName = pair.first.texturePath;
            auto& context = pair.second;
            
            if (ImGui::TreeNode(textureName.c_str())) {
                ImGui::Text("Emitters: %d / %d", (int)(GPUParticleSystem::kMaxEmitters - context.freeIndices.size()), GPUParticleSystem::kMaxEmitters);
                context.system->Debug();
                ImGui::TreePop();
            }
        }
        ImGui::EndTabItem();
    }
#endif
}
