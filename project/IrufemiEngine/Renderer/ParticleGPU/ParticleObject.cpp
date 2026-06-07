#include "ParticleObject.h"
#include "Renderer/ParticleGPU/GPUParticleManager.h"
#include <fstream>
#include <iostream>

TextureManager* ParticleObject::textureManager_ = nullptr;

ParticleObject::ParticleObject() {}

ParticleObject::~ParticleObject() {
    if (emitterHandle_.IsValid()) {
        GPUParticleManager::GetInstance()->UnregisterEmitter(emitterHandle_);
    }
}

void ParticleObject::Initialize() {
    if (emitOnAwake_) {
        Play();
    }
}

void ParticleObject::Play() {
    isPlaying_ = true;
    MarkDirty();
}

void ParticleObject::Stop() {
    isPlaying_ = false;
    MarkDirty();
}

void ParticleObject::EmitBurst(int count) {
    burstCountPending_ += count;
    Play();
}

void ParticleObject::Update() {
    if (isDirty_ || burstCountPending_ > 0) {
        UpdateSystem();
        isDirty_ = false;
    }
}

void ParticleObject::UpdateSystem() {
    if (!emitterHandle_.IsValid()) {
        emitterHandle_ = GPUParticleManager::GetInstance()->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_);
    }

    GPUParticleEmitter data;
    data.emit = isPlaying_ ? 1 : 0;
    
    data.type = emitType_;
    data.translateX = position_.x;
    data.translateY = position_.y;
    data.translateZ = position_.z;
    data.emissionRate = emissionRate_;
    data.minLife = lifeTimeMin_;
    data.maxLife = lifeTimeMax_;
    data.velocity = velocity_;
    data.radius = radius_;
    data.spread = spread_;
    data.gravity = gravity_;
    data.damping = damping_;

    data.startColorMinR = color_.x; data.startColorMinG = color_.y; data.startColorMinB = color_.z; data.startColorMinA = color_.w;
    data.startColorMaxR = color_.x; data.startColorMaxG = color_.y; data.startColorMaxB = color_.z; data.startColorMaxA = color_.w;
    
    data.midColorMinR = midColor_.x; data.midColorMinG = midColor_.y; data.midColorMinB = midColor_.z; data.midColorMinA = midColor_.w;
    data.midColorMaxR = midColor_.x; data.midColorMaxG = midColor_.y; data.midColorMaxB = midColor_.z; data.midColorMaxA = midColor_.w;

    data.endColorMinR = color_.x; data.endColorMinG = color_.y; data.endColorMinB = color_.z; data.endColorMinA = 0.0f;
    data.endColorMaxR = color_.x; data.endColorMaxG = color_.y; data.endColorMaxB = color_.z; data.endColorMaxA = 0.0f;

    data.startScaleMinX = startScale_.x; data.startScaleMinY = startScale_.y; data.startScaleMinZ = startScale_.z;
    data.startScaleMaxX = startScale_.x; data.startScaleMaxY = startScale_.y; data.startScaleMaxZ = startScale_.z;
    
    data.midScaleMinX = midScale_.x; data.midScaleMinY = midScale_.y; data.midScaleMinZ = midScale_.z;
    data.midScaleMaxX = midScale_.x; data.midScaleMaxY = midScale_.y; data.midScaleMaxZ = midScale_.z;

    data.endScaleMinX = endScale_.x; data.endScaleMinY = endScale_.y; data.endScaleMinZ = endScale_.z;
    data.endScaleMaxX = endScale_.x; data.endScaleMaxY = endScale_.y; data.endScaleMaxZ = endScale_.z;
    
    data.midPoint = midPoint_;

    data.directionX = direction_.x;
    data.directionY = direction_.y;
    data.directionZ = direction_.z;
    
    data.areaSizeX = areaSize_.x;
    data.areaSizeY = areaSize_.y;
    data.areaSizeZ = areaSize_.z;
    
    data.bounce = bounce_;
    data.groundHeight = groundHeight_;
    
    data.attractorStrength = attractorStrength_;
    data.attractorPosX = attractorPos_.x;
    data.attractorPosY = attractorPos_.y;
    data.attractorPosZ = attractorPos_.z;
    
    data.atlasRows = atlasRows_;
    data.atlasCols = atlasCols_;
    data.billboardMode = billboardMode_;
    data.jitter = jitter_;

    if (burstCountPending_ > 0) {
        data.burstCount = burstCountPending_;
        burstCountPending_ = 0;
    }
    GPUParticleManager::GetInstance()->UpdateEmitterData(emitterHandle_, data);
}

void ParticleObject::Serialize(nlohmann::json& j) const {
    j["texturePath"] = texturePath_;
    j["blendMode"] = static_cast<int>(blendMode_);
    j["isUnscaledTime"] = isUnscaledTime_;
    j["emitOnAwake"] = emitOnAwake_;
    
    j["emitType"] = emitType_;
    j["emissionRate"] = emissionRate_;
    j["lifeTimeMin"] = lifeTimeMin_;
    j["lifeTimeMax"] = lifeTimeMax_;
    j["velocity"] = velocity_;
    j["radius"] = radius_;
    j["spread"] = spread_;
    
    j["atlasRows"] = atlasRows_;
    j["atlasCols"] = atlasCols_;
    
    j["gravity"] = gravity_;
    j["damping"] = damping_;
    j["bounce"] = bounce_;
    j["groundHeight"] = groundHeight_;
    j["attractorStrength"] = attractorStrength_;
    j["attractorPos"] = { attractorPos_.x, attractorPos_.y, attractorPos_.z };
    j["jitter"] = jitter_;
    
    j["billboardMode"] = billboardMode_;
    j["color"] = { color_.x, color_.y, color_.z, color_.w };
    j["midColor"] = { midColor_.x, midColor_.y, midColor_.z, midColor_.w };
    j["startScale"] = { startScale_.x, startScale_.y, startScale_.z };
    j["midScale"] = { midScale_.x, midScale_.y, midScale_.z };
    j["endScale"] = { endScale_.x, endScale_.y, endScale_.z };
    j["midPoint"] = midPoint_;
    
    j["direction"] = { direction_.x, direction_.y, direction_.z };
    j["areaSize"] = { areaSize_.x, areaSize_.y, areaSize_.z };
}

void ParticleObject::Deserialize(const nlohmann::json& j) {
    if (j.contains("texturePath")) texturePath_ = j["texturePath"].get<std::string>();
    if (j.contains("blendMode")) blendMode_ = static_cast<BlendMode>(j["blendMode"].get<int>());
    if (j.contains("isUnscaledTime")) isUnscaledTime_ = j["isUnscaledTime"].get<bool>();
    if (j.contains("emitOnAwake")) emitOnAwake_ = j["emitOnAwake"].get<bool>();
    
    if (j.contains("emitType")) emitType_ = j["emitType"].get<int>();
    if (j.contains("emissionRate")) emissionRate_ = j["emissionRate"].get<float>();
    if (j.contains("lifeTimeMin")) lifeTimeMin_ = j["lifeTimeMin"].get<float>();
    if (j.contains("lifeTimeMax")) lifeTimeMax_ = j["lifeTimeMax"].get<float>();
    if (j.contains("velocity")) velocity_ = j["velocity"].get<float>();
    if (j.contains("radius")) radius_ = j["radius"].get<float>();
    if (j.contains("spread")) spread_ = j["spread"].get<float>();
    
    if (j.contains("atlasRows")) atlasRows_ = j["atlasRows"].get<int>();
    if (j.contains("atlasCols")) atlasCols_ = j["atlasCols"].get<int>();
    
    if (j.contains("gravity")) gravity_ = j["gravity"].get<float>();
    if (j.contains("damping")) damping_ = j["damping"].get<float>();
    if (j.contains("bounce")) bounce_ = j["bounce"].get<float>();
    if (j.contains("groundHeight")) groundHeight_ = j["groundHeight"].get<float>();
    if (j.contains("attractorStrength")) attractorStrength_ = j["attractorStrength"].get<float>();
    if (j.contains("attractorPos") && j["attractorPos"].size() == 3) {
        attractorPos_ = { j["attractorPos"][0], j["attractorPos"][1], j["attractorPos"][2] };
    }
    if (j.contains("jitter")) jitter_ = j["jitter"].get<float>();
    
    if (j.contains("billboardMode")) billboardMode_ = j["billboardMode"].get<int>();
    if (j.contains("color") && j["color"].size() == 4) {
        color_ = { j["color"][0], j["color"][1], j["color"][2], j["color"][3] };
    }
    if (j.contains("midColor") && j["midColor"].size() == 4) {
        midColor_ = { j["midColor"][0], j["midColor"][1], j["midColor"][2], j["midColor"][3] };
    }
    if (j.contains("startScale") && j["startScale"].size() == 3) {
        startScale_ = { j["startScale"][0], j["startScale"][1], j["startScale"][2] };
    }
    if (j.contains("midScale") && j["midScale"].size() == 3) {
        midScale_ = { j["midScale"][0], j["midScale"][1], j["midScale"][2] };
    }
    if (j.contains("endScale") && j["endScale"].size() == 3) {
        endScale_ = { j["endScale"][0], j["endScale"][1], j["endScale"][2] };
    }
    if (j.contains("midPoint")) midPoint_ = j["midPoint"].get<float>();
    
    if (j.contains("direction") && j["direction"].size() == 3) {
        direction_ = { j["direction"][0], j["direction"][1], j["direction"][2] };
    }
    if (j.contains("areaSize") && j["areaSize"].size() == 3) {
        areaSize_ = { j["areaSize"][0], j["areaSize"][1], j["areaSize"][2] };
    }

    MarkDirty();
}

bool ParticleObject::LoadFromJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open particle json: " << filepath << std::endl;
        return false;
    }
    
    nlohmann::json j;
    try {
        file >> j;
        Deserialize(j);
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parse error in " << filepath << ": " << e.what() << std::endl;
        return false;
    }
    return true;
}
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include "Resource/Texture/TextureManager.h"

void ParticleObject::DebugUI(const char* name) {
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        
        if (ImGui::TreeNodeEx("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Texture Manager Combo
            auto* textureManager = GetTextureManager();
            if (textureManager && !textureManager->GetTextureNamesForDebug().empty()) {
                auto textureNames = textureManager->GetTextureNamesForDebug();
                std::vector<const char*> namesCStr;
                int currentIndex = -1;
                for (int i = 0; i < textureNames.size(); ++i) {
                    namesCStr.push_back(textureNames[i].c_str());
                    if (texturePath_ == textureNames[i]) {
                        currentIndex = i;
                    }
                }
                if (currentIndex == -1) currentIndex = 0; // fallback
                
                if (ImGui::Combo("Texture", &currentIndex, namesCStr.data(), (int)namesCStr.size())) {
                    if (texturePath_ != textureNames[currentIndex]) {
                        texturePath_ = textureNames[currentIndex];
                        if (emitterHandle_.IsValid()) {
                            GPUParticleManager::GetInstance()->UnregisterEmitter(emitterHandle_);
                            emitterHandle_ = GPUParticleManager::GetInstance()->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_);
                        }
                        changed = true;
                    }
                }
            }
            
            const char* blendNames[] = { "None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Premultiplied" };
            int currentBlend = static_cast<int>(blendMode_);
            if (ImGui::Combo("Blend Mode", &currentBlend, blendNames, 7)) {
                blendMode_ = static_cast<BlendMode>(currentBlend);
                if (emitterHandle_.IsValid()) {
                    GPUParticleManager::GetInstance()->UnregisterEmitter(emitterHandle_);
                    emitterHandle_ = GPUParticleManager::GetInstance()->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_);
                }
                changed = true;
            }
            
            if (ImGui::Checkbox("Unscaled Time", &isUnscaledTime_)) {
                if (emitterHandle_.IsValid()) {
                    GPUParticleManager::GetInstance()->UnregisterEmitter(emitterHandle_);
                    emitterHandle_ = GPUParticleManager::GetInstance()->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_);
                }
                changed = true;
            }
            
            changed |= ImGui::Checkbox("Emit On Awake", &emitOnAwake_);

            ImGui::Separator();
            changed |= ImGui::DragInt("Atlas Rows", &atlasRows_, 1, 1, 16);
            changed |= ImGui::DragInt("Atlas Cols", &atlasCols_, 1, 1, 16);
            
            ImGui::Separator();
            if (ImGui::Button("Test Burst (50 particles)")) {
                EmitBurst(50);
            }
            ImGui::SameLine();
            if (ImGui::Button("Play")) Play();
            ImGui::SameLine();
            if (ImGui::Button("Stop")) Stop();

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* shapeNames[] = { "Sphere", "Beam", "Box", "Cylinder" };
            changed |= ImGui::Combo("Emit Shape", &emitType_, shapeNames, 4);

            const char* billboardNames[] = { "None", "Billboard", "Y-Axis" };
            changed |= ImGui::Combo("Billboard Mode", &billboardMode_, billboardNames, 3);

            ImGui::Separator();
            if (emitType_ == 2) { // Box
                changed |= ImGui::DragFloat3("Area Size", &areaSize_.x, 0.1f, 0.0f, 100.0f);
            } else {
                changed |= ImGui::DragFloat("Radius / Size", &radius_, 0.1f, 0.0f, 100.0f);
            }
            
            changed |= ImGui::DragFloat("Spread", &spread_, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat3("Direction", &direction_.x, 0.05f);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Emission Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat("Emission Rate (/sec)", &emissionRate_, 1.0f, 0.0f, 10000.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Physics & Kinetics", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat("Velocity", &velocity_, 0.1f, 0.0f, 100.0f);
            changed |= ImGui::DragFloat("Gravity", &gravity_, 0.1f, -50.0f, 50.0f);
            changed |= ImGui::DragFloat("Damping", &damping_, 0.005f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Bounce", &bounce_, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Ground Height", &groundHeight_, 0.1f, -100.0f, 100.0f);
            
            ImGui::Separator();
            changed |= ImGui::DragFloat("Attractor Strength", &attractorStrength_, 0.1f, -50.0f, 50.0f);
            changed |= ImGui::DragFloat3("Attractor Pos", &attractorPos_.x, 0.1f);

            ImGui::Separator();
            changed |= ImGui::DragFloat("Jitter", &jitter_, 0.01f, 0.0f, 10.0f);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Lifetime & Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat("Life Min", &lifeTimeMin_, 0.05f, 0.01f, 10.0f);
            changed |= ImGui::DragFloat("Life Max", &lifeTimeMax_, 0.05f, 0.01f, 10.0f);
            
            ImGui::Separator();
            changed |= ImGui::ColorEdit4("Start Color", &color_.x);
            changed |= ImGui::ColorEdit4("Mid Color", &midColor_.x);
            
            ImGui::Separator();
            changed |= ImGui::DragFloat3("Start Scale", &startScale_.x, 0.05f);
            changed |= ImGui::DragFloat3("Mid Scale", &midScale_.x, 0.05f);
            changed |= ImGui::DragFloat3("End Scale", &endScale_.x, 0.05f);

            ImGui::Separator();
            changed |= ImGui::SliderFloat("Mid Point (0~1)", &midPoint_, 0.0f, 1.0f);

            ImGui::TreePop();
        }
        
        if (changed) {
            MarkDirty();
        }
    }
#endif
}
