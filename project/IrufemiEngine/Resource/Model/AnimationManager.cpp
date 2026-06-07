#include "AnimationManager.h"

#include "Data/Animation.h"
#include "Engine/Core/Utility/Ease.h"
#include "Engine/Core/Math/Math.h"
#include "Data/ModelData.h"
#include "Data/ObjModel.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/Graphics/DirectX/DescriptorPool.h"
#include "../../Engine/Graphics/Data/VertexData.h"

#include <cassert>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <algorithm>
#include <Windows.h>
#include <assimp/postprocess.h>
#include <numeric>

void AnimationManager::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
    if (rootDir_.empty()) {
        rootDir_ = "resources/model";
    }
}

void AnimationManager::SetRootDirectory(std::string root) {
    std::replace(root.begin(), root.end(), '\\', '/');
    if (!root.empty() && root.back() == '/') root.pop_back();
    rootDir_ = std::move(root);
}

/*Animation*/

///Animationを解析する

Animation AnimationManager::LoadAnimationFile(const std::string& filename) {

    // まずはAnimationの長さを秒に変換する
    // ・ｍTicksPerSecond：周波数
    // ・mDuration：ｍTicksPerSecondで指定された周波数における長さ
    // たとえばｍTicksPerSecondが1000というのは、1000Hzのことなので、1Tick(周期)は1msである
    // このとき、mDurationが2000なら、2000ms = 2s である

    Animation animation; // 今回作るアニメーション
    Assimp::Importer importer;

    // ファイルパスを解決
    std::string filePath;
    // パス区切り文字が含まれているかチェック
    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
        // 含まれている場合は、ルートディレクトリからの相対パスとして扱う
        filePath = NormalizeAndResolve(filename);
    } else {
        // 含まれていない場合は、再帰的にファイルを検索
        filePath = FindFileRecursive(filename);
    }

    if (filePath.empty() || !std::filesystem::exists(filePath)) {
        OutputDebugStringA(("[AnimationManager] File not found: " + filename + "\n").c_str());
        return {}; // 空のアニメーションを返す
    }

    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_MakeLeftHanded);
    // アニメーションがない場合は空のアニメーションを返す
    if (!scene || scene->mNumAnimations == 0) {
        OutputDebugStringA(("[AnimationManager] No animations found in file: " + filename + "\n").c_str());
        return {}; // 空のアニメーションを返す
    }
    aiAnimation* animationAssimp = scene->mAnimations[0]; // 最初のアニメーションだけ採用。もちろん複数対応するに越したことはない
    animation.duration = float(animationAssimp->mDuration / animationAssimp->mTicksPerSecond); // 時間の単位を秒に変換

    /// NodeAnimationを解析する

    // assimpでは個々のNodeのAnimationをchannelと呼んでいるのでchannelを回してNodeAnimationの情報をとってくる
    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // ここも秒に変換
            keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };//右手->左手
            nodeAnimation.translate.keyframes.push_back(keyframe);
        }

        // RotateはmNumRotationKeys/mRotationKeys、ScaleはmNumScalingKeys/mScalingKeysで取得できるので同様に行う。
        // RotateはQuaternionで、右手->左手に変換するために、yとzを反転させる必要がある。Scaleはそのままで良い。
        // keyframe.value = {rotate.x, -rotate.y, -rotate.z, rotate.w};

        // Rotation キーフレームを追加
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            KeyframeQuaternion keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // 秒に変換
            aiQuaternion& q = keyAssimp.mValue;
            // 右手系->左手系変換: y,z を反転
            keyframe.value = { q.x, q.y, q.z, q.w };
            nodeAnimation.rotate.keyframes.push_back(keyframe);
        }

        // Scale キーフレームを追加
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            KeyframeVector3 keyframe;
            keyframe.time = float(keyAssimp.mTime / animationAssimp->mTicksPerSecond); // 秒に変換
            keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z }; // スケールはそのまま
            nodeAnimation.scale.keyframes.push_back(keyframe);
        }
    }
    // 解析完了
    return animation;
}

// 任意の時刻の値を取得する
Vector3 AnimationManager::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {

    // まずは関数の先頭で特殊なケースを除外しておく
    assert(!keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.size() == 1 || time <= keyframes[0].time) { // キーが1つか、時刻がキーフレーム前なら最初の値とする
        return keyframes[0].value;
    }

    // 前提としてkeyframesは先頭から時刻の早い順に並んでいる
    // 先頭から順番に時刻を調べ、指定した時刻が範囲内であれば、補間を行い値を返す
    // 補間方法はVector3は線形補間、Quaternionは球面線形補間にする
    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        // indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            // 範囲内を補間する
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }
    // ここまできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
    return (*keyframes.rbegin()).value;
}

// 任意の時刻の値を取得する
Quaternion AnimationManager::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {

    // まずは関数の先頭で特殊なケースを除外しておく
    assert(!keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.size() == 1 || time <= keyframes[0].time) { // キーが1つか、時刻がキーフレーム前なら最初の値とする
        return keyframes[0].value;
    }

    // 前提としてkeyframesは先頭から時刻の早い順に並んでいる
    // 先頭から順番に時刻を調べ、指定した時刻が範囲内であれば、補間を行い値を返す
    // 補間方法はVector3は線形補間、Quaternionは球面線形補間にする
    for (size_t index = 0; index < keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        // indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
        if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
            // 範囲内を補間する
            float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
            return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
        }
    }
    // ここまできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
    return (*keyframes.rbegin()).value;
}

// 任意の時刻の値を取得する
Vector3 AnimationManager::CalculateValue(const AnimationCurve<Vector3>& keyframes, float time) {

    // まずは関数の先頭で特殊なケースを除外しておく
    assert(!keyframes.keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.keyframes.size() == 1 || time <= keyframes.keyframes[0].time) { // キーが1つか、時刻がキーフレーム前なら最初の値とする
        return keyframes.keyframes[0].value;
    }

    // 前提としてkeyframes.keyframesは先頭から時刻の早い順に並んでいる
    // 先頭から順番に時刻を調べ、指定した時刻が範囲内であれば、補間を行い値を返す
    // 補間方法はVector3は線形補間、Quaternionは球面線形補間にする
    for (size_t index = 0; index < keyframes.keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        // indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
        if (keyframes.keyframes[index].time <= time && time <= keyframes.keyframes[nextIndex].time) {
            // 範囲内を補間する
            float t = (time - keyframes.keyframes[index].time) / (keyframes.keyframes[nextIndex].time - keyframes.keyframes[index].time);
            return Lerp(keyframes.keyframes[index].value, keyframes.keyframes[nextIndex].value, t);
        }
    }
    // ここまできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
    return (*keyframes.keyframes.rbegin()).value;
}

// 任意の時刻の値を取得する
Quaternion AnimationManager::CalculateValue(const AnimationCurve<Quaternion>& keyframes, float time) {

    // まずは関数の先頭で特殊なケースを除外しておく
    assert(!keyframes.keyframes.empty()); // キーがないものは返す値がわからないのでダメ
    if (keyframes.keyframes.size() == 1 || time <= keyframes.keyframes[0].time) { // キーが1つか、時刻がキーフレーム前なら最初の値とする
        return keyframes.keyframes[0].value;
    }

    // 前提としてkeyframes.keyframesは先頭から時刻の早い順に並んでいる
    // 先頭から順番に時刻を調べ、指定した時刻が範囲内であれば、補間を行い値を返す
    // 補間方法はVector3は線形補間、Quaternionは球面線形補間にする
    for (size_t index = 0; index < keyframes.keyframes.size() - 1; ++index) {
        size_t nextIndex = index + 1;
        // indexとnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
        if (keyframes.keyframes[index].time <= time && time <= keyframes.keyframes[nextIndex].time) {
            // 範囲内を補間する
            float t = (time - keyframes.keyframes[index].time) / (keyframes.keyframes[nextIndex].time - keyframes.keyframes[index].time);
            return Math::Slerp(keyframes.keyframes[index].value, keyframes.keyframes[nextIndex].value, t);
        }
    }
    // ここまできた場合は一番後の時刻よりも後ろなので最後の値を返すことにする
    return (*keyframes.keyframes.rbegin()).value;
}

// 任意の時刻の値を取得する(オイラー角)
Vector3 AnimationManager::CalculateValueAsEuler(const AnimationCurve<Quaternion>& keyframes, float time) {
    Quaternion rotation = CalculateValue(keyframes, time);
    return Math::ToEuler(rotation);
}

// Nodeの階層構造からSkeletonを作る
Skeleton AnimationManager::CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

    // 名前とindexのマッピングを行いアクセスしやすくする
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    return skeleton;
}

//NodeからJointを作る
int32_t AnimationManager::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = Math::MakeIdentity4x4();
    joint.transform = node.transform;
    joint.index = int32_t(joints.size()); // 現在登録されている数をIndexに
    joint.parent = parent;
    joints.push_back(joint); // SkeletonのJoint列に追加
    for (const Node& child : node.children) {
        // 子Jointを作成し、そのIndexを登録
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }
    // 自身のIndexを返す
    return joint.index;
}

// Skeletonの更新
void AnimationManager::SkeletonUpdate(Skeleton& skeleton) {
    // すべてのJointを更新。親が若いので通常ループで処理可能になっている。
    for (Joint& joint : skeleton.joints) {
        joint.localMatrix = Math::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
        if (joint.parent) { // 親がいれば親の行列を掛ける
            joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
        } else { // 親がいないんでlocalMatrixとskeletonSpaceMatrixは一致する
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

// Skeletonに対してAnimationを適用する
void AnimationManager::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
    // アニメーションが変更された場合（または初回）のみ、バインディングを再構築する
    if (skeleton.lastAppliedAnimation != &animation) {
        skeleton.lastAppliedAnimation = &animation;
        skeleton.activeAnimationBindings.clear();

        // アニメーション側のノード名から、対象のジョイントを探してキャッシュ
        for (const auto& [nodeName, nodeAnimation] : animation.nodeAnimations) {
            auto it = skeleton.jointMap.find(nodeName);
            if (it != skeleton.jointMap.end()) {
                skeleton.activeAnimationBindings.push_back({ it->second, &nodeAnimation });
            }
        }
    }

    // キャッシュを使って毎フレームの文字列検索 (std::map::find) を排除！
    for (const auto& binding : skeleton.activeAnimationBindings) {
        Joint& joint = skeleton.joints[binding.first];
        const NodeAnimation& rootNodeAnimation = *binding.second;

        joint.transform.translate = CalculateValue(rootNodeAnimation.translate, animationTime);
        joint.transform.rotate = CalculateValue(rootNodeAnimation.rotate, animationTime);
        joint.transform.scale = CalculateValue(rootNodeAnimation.scale, animationTime);
    }
}

std::string AnimationManager::NormalizeAndResolve(const std::string& filename) const {
    std::string f = filename;
    std::replace(f.begin(), f.end(), '\\', '/');
    if (StartsWith(f, rootDir_ + "/")) {
        // OK
    } else if (StartsWith(f, rootDir_)) {
        f = rootDir_ + "/" + f.substr(rootDir_.size());
    } else {
        f = rootDir_ + "/" + f;
    }
    std::transform(f.begin(), f.end(), f.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return f;
}

bool AnimationManager::StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), s.begin());
}

std::pair<std::string, std::string> AnimationManager::SplitDirectoryAndFile(const std::string& full) {
    auto pos = full.find_last_of('/');
    if (pos == std::string::npos) return { ".", full };
    return { full.substr(0, pos), full.substr(pos + 1) };
}

std::string AnimationManager::FindFileRecursive(const std::string& filename) const {
    namespace fs = std::filesystem;
    std::string lowerFilename = filename;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = filePathCache_.find(lowerFilename); it != filePathCache_.end()) {
            return it->second;
        }
    }

    const fs::path rootPath = rootDir_;
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        return "";
    }

    for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
        if (entry.is_regular_file()) {
            std::string entryFilename = entry.path().filename().string();
            std::transform(entryFilename.begin(), entryFilename.end(), entryFilename.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (entryFilename == lowerFilename) {
                std::string foundPath = entry.path().string();
                std::replace(foundPath.begin(), foundPath.end(), '\\', '/');
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    filePathCache_[lowerFilename] = foundPath;
                }
                return foundPath;
            }
        }
    }

    return ""; // 見つからなかった
}

/*Skinning*/

/// SkinClusterの生成

// SkinClusterを生成
SkinCluster AnimationManager::CreateSkinCluster(const Skeleton& skeleton, const ModelData& modelData) {
    SkinCluster skinCluster;

    /// MatrixPalleteの作成

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        // pallete用のResourceを確保
        skinCluster.paletteResource[i] = dxCommon_->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
        WellForGPU* mappedPallete = nullptr;
        skinCluster.paletteResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedPallete));
        skinCluster.mappedPalette[i] = { mappedPallete, skeleton.joints.size() }; // spanを使ってアクセスするようにする

        // SRV用のインデックスを確保
        uint32_t paletteSrvIndex = dxCommon_->GetSrvPool()->Allocate();
        assert(paletteSrvIndex != DescriptorPool::kInvalid);
        skinCluster.paletteSrvHandle[i].first = dxCommon_->GetSrvPool()->GetCPUHandle(paletteSrvIndex);
        skinCluster.paletteSrvHandle[i].second = dxCommon_->GetSrvPool()->GetGPUHandle(paletteSrvIndex);

        // palette用のsrvを作成。StructuredBufferでアクセスできるようにする。
        D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
        paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        paletteSrvDesc.Buffer.FirstElement = 0;
        paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
        paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);

        dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.paletteResource[i].Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle[i].first);
    }

    /// influence用Resourceの作成

    // influence用のResourceを確保。頂点毎にinfluence情報を追加できるようにする。
    skinCluster.influenceResource = dxCommon_->CreateBufferResource(sizeof(VertexInfluence) * modelData.vertices.size());
    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
    std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size()); // 0埋め。weightを0にしておく。
    skinCluster.mappedInfluence = { mappedInfluence,modelData.vertices.size() };

    // Influence用のVBVの作成
    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

    // InverseBindPoseMatrixInverseBindPoseMatrixを格納する場所を作成して、単位行列で埋める
    skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
    std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), [] {return Math::MakeIdentity4x4(); });

    /// ModelDataを解析してInstanceを埋める

    for (const auto& jointWeight : modelData.skinClusterData) { // ModelのskinClusterの情報を解析
        auto it = skeleton.jointMap.find(jointWeight.first); // jointWeight.firstはJoint名なので、skeletonに対象となるjointが含まれているか判断
        if (it == skeleton.jointMap.end()) { // そんな名前のJointは存在しない。なので次に回す。
            continue;
        }
        // (*it).secondにはjointのindexが入っているので、該当のindexのinverseBindPoseMatrixを代入
        skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
        for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
            auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vertexIndex]; // 該当のvertexIndexのinfluence情報を参照しておく
            for (uint32_t index = 0; index < kNumMaxInfluence; ++index) { // 空いているところに入れる
                if (currentInfluence.weights[index] == 0.0f) { // weight==0が空いている状態なので、その場所にweightとjointのindexを代入
                    currentInfluence.weights[index] = vertexWeight.weight;
                    currentInfluence.jointIndices[index] = (*it).second;
                }

            }

        }
    }

    return skinCluster;
}

// SkinClusterを生成 (ObjModel版)
SkinCluster AnimationManager::CreateSkinCluster(const Skeleton& skeleton, const ObjModel& objModel) {
    SkinCluster skinCluster;

    // 全メッシュの頂点数を合計
    size_t totalVertices = 0;
    for (const auto& mesh : objModel.meshes) {
        totalVertices += mesh.vertices.size();
    }

    /// MatrixPalleteの作成
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        skinCluster.paletteResource[i] = dxCommon_->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
        WellForGPU* mappedPallete = nullptr;
        skinCluster.paletteResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedPallete));
        skinCluster.mappedPalette[i] = { mappedPallete, skeleton.joints.size() };

        uint32_t paletteSrvIndex = dxCommon_->GetSrvPool()->Allocate();
        assert(paletteSrvIndex != DescriptorPool::kInvalid);
        skinCluster.paletteSrvHandle[i].first = dxCommon_->GetSrvPool()->GetCPUHandle(paletteSrvIndex);
        skinCluster.paletteSrvHandle[i].second = dxCommon_->GetSrvPool()->GetGPUHandle(paletteSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
        paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        paletteSrvDesc.Buffer.FirstElement = 0;
        paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
        paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
        dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.paletteResource[i].Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle[i].first);
    }

    /// influence用Resourceの作成
    skinCluster.influenceResource = dxCommon_->CreateBufferResource(sizeof(VertexInfluence) * totalVertices);
    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
    std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * totalVertices);
    skinCluster.mappedInfluence = { mappedInfluence, totalVertices };

    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * totalVertices);
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

    // influence用SRV
    uint32_t influenceSrvIndex = dxCommon_->GetSrvPool()->Allocate();
    assert(influenceSrvIndex != DescriptorPool::kInvalid);
    skinCluster.influenceSrvHandle.first = dxCommon_->GetSrvPool()->GetCPUHandle(influenceSrvIndex);
    skinCluster.influenceSrvHandle.second = dxCommon_->GetSrvPool()->GetGPUHandle(influenceSrvIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC influenceSrvDesc{};
    influenceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    influenceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    influenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    influenceSrvDesc.Buffer.FirstElement = 0;
    influenceSrvDesc.Buffer.NumElements = UINT(totalVertices);
    influenceSrvDesc.Buffer.StructureByteStride = sizeof(VertexInfluence);
    dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.influenceResource.Get(), &influenceSrvDesc, skinCluster.influenceSrvHandle.first);


    skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
    std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), [] {return Math::MakeIdentity4x4(); });

    /// ModelDataを解析してInstanceを埋める
    for (const auto& jointWeight : objModel.skinClusterData) {
        auto it = skeleton.jointMap.find(jointWeight.first);
        if (it == skeleton.jointMap.end()) {
            continue;
        }
        skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
        for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
            auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vertexIndex];
            for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
                if (currentInfluence.weights[index] == 0.0f) {
                    currentInfluence.weights[index] = vertexWeight.weight;
                    currentInfluence.jointIndices[index] = (*it).second;
                    break;
                }
            }
        }
    }

    // --- コンピュートシェーダ用のリソース生成 ---
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        // Skinned Vertex Buffer (UAV)
        skinCluster.skinnedVertexResource[i] = dxCommon_->CreateUAVBufferResource(sizeof(VertexData) * totalVertices);
        // UAV
        uint32_t skinnedVertexUavIndex = dxCommon_->GetSrvPool()->Allocate();
        assert(skinnedVertexUavIndex != DescriptorPool::kInvalid);
        skinCluster.skinnedVertexUavHandle[i].first = dxCommon_->GetSrvPool()->GetCPUHandle(skinnedVertexUavIndex);
        skinCluster.skinnedVertexUavHandle[i].second = dxCommon_->GetSrvPool()->GetGPUHandle(skinnedVertexUavIndex);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = UINT(totalVertices);
        uavDesc.Buffer.StructureByteStride = sizeof(VertexData);
        dxCommon_->GetDevice()->CreateUnorderedAccessView(skinCluster.skinnedVertexResource[i].Get(), nullptr, &uavDesc, skinCluster.skinnedVertexUavHandle[i].first);

        // SRV
        uint32_t skinnedVertexSrvIndex = dxCommon_->GetSrvPool()->Allocate();
        assert(skinnedVertexSrvIndex != DescriptorPool::kInvalid);
        skinCluster.skinnedVertexSrvHandle[i].first = dxCommon_->GetSrvPool()->GetCPUHandle(skinnedVertexSrvIndex);
        skinCluster.skinnedVertexSrvHandle[i].second = dxCommon_->GetSrvPool()->GetGPUHandle(skinnedVertexSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = UINT(totalVertices);
        srvDesc.Buffer.StructureByteStride = sizeof(VertexData);
        dxCommon_->GetDevice()->CreateShaderResourceView(skinCluster.skinnedVertexResource[i].Get(), &srvDesc, skinCluster.skinnedVertexSrvHandle[i].first);

        // VBV
        skinCluster.skinnedVertexBufferView[i].BufferLocation = skinCluster.skinnedVertexResource[i]->GetGPUVirtualAddress();
        skinCluster.skinnedVertexBufferView[i].SizeInBytes = UINT(sizeof(VertexData) * totalVertices);
        skinCluster.skinnedVertexBufferView[i].StrideInBytes = sizeof(VertexData);
    }

    // Skinning Information (CBV)
    skinCluster.skinningInformationResource = dxCommon_->CreateBufferResource(sizeof(SkinningInformation));
    skinCluster.skinningInformationResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster.mappedSkinningInformation));
    skinCluster.mappedSkinningInformation->numVertices = static_cast<uint32_t>(totalVertices);


    return skinCluster;
}

// SkinClusterの更新
void AnimationManager::SkinClusterUpdate(SkinCluster& skinCluster, const Skeleton& skeleton, uint32_t frameIndex) {
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
        skinCluster.mappedPalette[frameIndex][jointIndex].skeletonSpaceMatrix = skinCluster.inverseBindPoseMatrices[jointIndex] * skeleton.joints[jointIndex].skeletonSpaceMatrix;
        skinCluster.mappedPalette[frameIndex][jointIndex].skeletonSpaceInverseTransposeMatrix = Math::Transpose(Math::Inverse(skinCluster.mappedPalette[frameIndex][jointIndex].skeletonSpaceMatrix));
    }
}