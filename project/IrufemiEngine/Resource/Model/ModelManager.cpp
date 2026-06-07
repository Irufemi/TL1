#include "ModelManager.h"
#include "Engine/Core/System/ThreadPool.h"
#include <filesystem>
#include <Windows.h>
#include <chrono>
#include <thread>
#include <format>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/Graphics/DirectX/DescriptorPool.h"
#include "Resource/Texture/TextureManager.h"
#include "Engine/IrufemiEngine.h"
#include "Framework/SceneManager.h"
#include "Engine/Graphics/Data/Material.h"
#include "../../Engine/Graphics/Data/VertexData.h"
#include "Resource/Model/Data/Node.h"
#include "Resource/Model/Data/Skeleton.h"
#include "Resource/Model/Data/SkinCluster.h"
#include <thread>
#include <algorithm>
#include <limits>

//======================
// キャッシュ系(インスタンス)
//======================

DirectXCommon* GpuMesh::sDxCommon = nullptr;

GpuMesh::~GpuMesh() {
    if (sDxCommon && sDxCommon->GetSrvPool() && srvIndex != 0xFFFFFFFF) {
        sDxCommon->GetSrvPool()->FreeAfterFence(srvIndex, sDxCommon->GetFenceValue());
    }
}

ModelManager::ModelManager() = default;
ModelManager::~ModelManager() = default;

void ModelManager::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    dxCommon_ = dxCommon;
    GpuMesh::sDxCommon = dxCommon;
    textureManager_ = textureManager; // 追加
    if (rootDir_.empty()) {
        rootDir_ = "resources/model";
    }
    if (!threadPool_) {
        threadPool_ = std::make_unique<ThreadPool>(4); // 推奨された4スレッド
    }
    if (!taskGroup_) {
        taskGroup_ = std::make_shared<TaskGroup>();
    }
    if (!backgroundTaskGroup_) {
        backgroundTaskGroup_ = std::make_shared<TaskGroup>();
    }
}

void ModelManager::SetRootDirectory(std::string root) {
    std::replace(root.begin(), root.end(), '\\', '/');
    if (!root.empty() && root.back() == '/') root.pop_back();
    rootDir_ = std::move(root);
}

std::shared_ptr<ManagedModel> ModelManager::GetModel(const std::string& filename) {
    auto managedModel = GetModelAsync(filename);
    if (!managedModel) return nullptr;

    // ロード中か待機中であれば、確定状態（Loaded or Failed）になるまで待機
    while (true) {
        auto status = managedModel->status.load();
        if (status == ManagedModel::LoadingStatus::Loaded || status == ManagedModel::LoadingStatus::Failed) {
            break;
        }
        std::this_thread::yield();
    }

    if (managedModel->status.load() == ManagedModel::LoadingStatus::Failed) {
        OutputDebugStringA(std::format("[ModelManager] [Thread:{}] Load failed (waited): {}\n", GetCurrentThreadId(), filename).c_str());
        return nullptr;
    }

    return managedModel;
}

std::shared_ptr<ManagedModel> ModelManager::GetModelAsync(const std::string& filename) {
    const std::string key = filename;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = cache_.find(key); it != cache_.end()) {
            if (auto sp = it->second.lock()) {
                // OutputDebugStringA(std::format("[ModelManager] [Thread:{}] Cache hit: {}\n", GetCurrentThreadId(), filename).c_str());
                return sp;
            }
        }
    }

    // ファイルパスを解決
    std::string fullPath;
    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
        fullPath = NormalizeAndResolve(filename);
    } else {
        fullPath = FindFileRecursive(filename);
    }

    if (fullPath.empty() || !std::filesystem::exists(fullPath)) {
        OutputDebugStringA(("[ModelManager] File not found: " + filename + "\n").c_str());
        return nullptr;
    }

    // 非同期ロード用のプロキシ作成
    auto managedModel = std::make_shared<ManagedModel>();
    managedModel->status.store(ManagedModel::LoadingStatus::Pending);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = managedModel;
    }

    OutputDebugStringA(std::format("[ModelManager] [Thread:{}] Request async load: {}\n", GetCurrentThreadId(), filename).c_str());

    // タスクをキューイング (TaskGroup 経由)
    threadPool_->Enqueue(taskGroup_, [this, managedModel, fullPath]() {
        LoadInternal(managedModel, fullPath);
    });

    return managedModel;
}

void ModelManager::LoadInternal(std::shared_ptr<ManagedModel> managedModel, const std::string& fullPath) {

    std::string key = SplitDirectoryAndFile(fullPath).second;
    OutputDebugStringA(std::format("[ModelManager] [Thread:{}] Worker START: {}\n", GetCurrentThreadId(), key).c_str());

    managedModel->status.store(ManagedModel::LoadingStatus::Loading);

    try {
        // CPUモデルロード
        auto pair = SplitDirectoryAndFile(fullPath);
        managedModel->cpuModel = std::make_shared<ObjModel>(ModelManager::LoadModelFromFile(pair.first, pair.second));

        // GPUリソース生成
        managedModel->gpuMeshes.reserve(managedModel->cpuModel->meshes.size());
        managedModel->gpuMaterials.reserve(managedModel->cpuModel->meshes.size());

        for (const auto& cpuMesh : managedModel->cpuModel->meshes) {
            auto gpuMesh = std::make_shared<GpuMesh>();

            // Vertex Buffer
            if (!cpuMesh.vertices.empty()) {
                const size_t vbSize = sizeof(VertexData) * cpuMesh.vertices.size();
                gpuMesh->vertexResource = dxCommon_->CreateBufferResource(vbSize);
                if (!gpuMesh->vertexResource) {
                    OutputDebugStringA("[ModelManager] Failed to create vertex buffer resource!\n");
                    continue; // Skip this mesh if resource creation failed
                }
                gpuMesh->vertexCount = static_cast<UINT>(cpuMesh.vertices.size());
                gpuMesh->vertexBufferView.BufferLocation = gpuMesh->vertexResource->GetGPUVirtualAddress();
                gpuMesh->vertexBufferView.SizeInBytes = static_cast<UINT>(vbSize);
                gpuMesh->vertexBufferView.StrideInBytes = sizeof(VertexData);
                VertexData* vbData = nullptr;
                gpuMesh->vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vbData));
                std::memcpy(vbData, cpuMesh.vertices.data(), vbSize);
                gpuMesh->vertexResource->Unmap(0, nullptr);

                gpuMesh->srvIndex = dxCommon_->GetSrvPool()->Allocate();
                assert(gpuMesh->srvIndex != DescriptorPool::kInvalid);
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Buffer.FirstElement = 0;
                srvDesc.Buffer.NumElements = gpuMesh->vertexCount;
                srvDesc.Buffer.StructureByteStride = sizeof(VertexData);
                dxCommon_->GetDevice()->CreateShaderResourceView(gpuMesh->vertexResource.Get(), &srvDesc, dxCommon_->GetSrvPool()->GetCPUHandle(gpuMesh->srvIndex));
                gpuMesh->vertexSrvHandle = dxCommon_->GetSrvPool()->GetGPUHandle(gpuMesh->srvIndex);
            }

            // Index Buffer
            if (!cpuMesh.indices.empty()) {
                const size_t ibSize = sizeof(uint32_t) * cpuMesh.indices.size();
                gpuMesh->indexResource = dxCommon_->CreateBufferResource(ibSize);
                if (gpuMesh->indexResource) {
                    gpuMesh->indexCount = static_cast<UINT>(cpuMesh.indices.size());
                    gpuMesh->indexBufferView.BufferLocation = gpuMesh->indexResource->GetGPUVirtualAddress();
                    gpuMesh->indexBufferView.SizeInBytes = static_cast<UINT>(ibSize);
                    gpuMesh->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
                    uint32_t* ibData = nullptr;
                    gpuMesh->indexResource->Map(0, nullptr, reinterpret_cast<void**>(&ibData));
                    std::memcpy(ibData, cpuMesh.indices.data(), ibSize);
                    gpuMesh->indexResource->Unmap(0, nullptr);
                } else {
                    OutputDebugStringA("[ModelManager] Failed to create index buffer resource!\n");
                }
            }
            managedModel->gpuMeshes.push_back(std::move(gpuMesh));

            // Materialリソース生成
            auto gpuMaterial = std::make_shared<GpuMaterial>();
            gpuMaterial->materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
            Material* materialData = nullptr;
            gpuMaterial->materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

            materialData->color = cpuMesh.material.color;
            materialData->enableLighting = cpuMesh.material.enableLighting;
            materialData->uvTransform = cpuMesh.material.uvTransform;
            materialData->metallic = cpuMesh.material.metallic;
            materialData->roughness = cpuMesh.material.roughness;
            materialData->hasTexture = !cpuMesh.material.textureFilePath.empty();
            materialData->environmentCoefficient = 0.0f;
            materialData->lightingMode = cpuMesh.material.enableLighting ? 3 : 0;
            if (materialData->color.w <= 0.0f) { materialData->color.w = 1.0f; }

            if (materialData->hasTexture) {
                gpuMaterial->textureHandle = textureManager_->GetTextureHandle(cpuMesh.material.textureFilePath);
            } else {
                gpuMaterial->textureHandle = textureManager_->GetWhiteTextureHandle();
            }
            managedModel->gpuMaterials.push_back(std::move(gpuMaterial));
        }


        // --- すべてのテクスチャのロード完了を待機 ---
        std::vector<std::string> texturePaths;
        for (const auto& mesh : managedModel->cpuModel->meshes) {
            if (!mesh.material.textureFilePath.empty()) {
                texturePaths.push_back(mesh.material.textureFilePath);
            }
        }

        bool allTexturesReady = false;
        while (!allTexturesReady) {
            allTexturesReady = true;
            for (const auto& path : texturePaths) {
                auto status = textureManager_->GetTextureStatus(path);
                if (status == Texture::LoadingStatus::Loading || status == Texture::LoadingStatus::Pending) {
                    allTexturesReady = false;
                    break;
                }
            }
            if (!allTexturesReady) {
                std::this_thread::yield(); // 他のロードタスク（TextureManager側）に CPU を譲る
            }
        }

        managedModel->status.store(ManagedModel::LoadingStatus::Loaded);
        OutputDebugStringA(std::format("[ModelManager] [Thread:{}] Worker FINISH: {}\n", GetCurrentThreadId(), key).c_str());
    } catch (...) {
        managedModel->status.store(ManagedModel::LoadingStatus::Failed);
        OutputDebugStringA(std::format("[ModelManager] [Thread:{}] Worker FAILED: {}\n", GetCurrentThreadId(), key).c_str());
    }
}

bool ModelManager::IsCurrentSceneInitializing() const {
    if (!dxCommon_) return false;
    auto engine = dxCommon_->GetEngine();
    if (!engine) return false;
    auto sceneManager = engine->GetSceneManager();
    if (!sceneManager) return false;
    return sceneManager->IsInitializing();
}

void ModelManager::PreloadAllUnder(const std::string& relativeFolder) {
    namespace fs = std::filesystem;
    const std::string rootBase = rootDir_.empty() ? "resources/model" : rootDir_;
    fs::path start = fs::path(rootBase) / relativeFolder;
    if (!fs::exists(start)) { return; }

    for (auto& entry : fs::recursive_directory_iterator(start)) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
            GetModel(p.filename().string()); // ファイル名のみを渡す
        }
    }
}

std::vector<std::string> ModelManager::GetCachedKeys() const {
    std::vector<std::string> out;
    std::lock_guard<std::mutex> lock(mutex_);
    out.reserve(cache_.size());
    for (auto& kv : cache_) {
        if (!kv.second.expired()) {
            out.push_back(kv.first);
        }
    }
    return out;
}

void ModelManager::RefreshAvailableModels() {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(mutex_);
    availableModelsCache_.clear();
    
    const fs::path rootPath = rootDir_.empty() ? "resources/model" : rootDir_;
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        isAvailableModelsCached_ = true;
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            
                if (ext == ".obj" || ext == ".gltf" || ext == ".fbx" || ext == ".glb") {
                    // 同名ファイル対策として、ルートディレクトリからの相対パスでリスト化する
                    std::string relPath = std::filesystem::relative(entry.path(), rootDir_).string();
                    std::replace(relPath.begin(), relPath.end(), '\\', '/');
                    availableModelsCache_.push_back(relPath);
                }
        }
    }
    isAvailableModelsCached_ = true;
}

std::vector<std::string> ModelManager::GetAvailableModels() const {
    if (!isAvailableModelsCached_) {
        const_cast<ModelManager*>(this)->RefreshAvailableModels();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return availableModelsCache_;
}

void ModelManager::CollectGarbage() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.expired()) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void ModelManager::ClearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    filePathCache_.clear();
}

std::string ModelManager::NormalizeAndResolve(const std::string& filename) const {
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

bool ModelManager::StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), s.begin());
}

std::pair<std::string, std::string> ModelManager::SplitDirectoryAndFile(const std::string& full) {
    auto pos = full.find_last_of('/');
    if (pos == std::string::npos) return { ".", full };
    return { full.substr(0, pos), full.substr(pos + 1) };
}

void ModelManager::DebugLogLoad(const std::string& key, size_t meshCount) {
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    std::string msg = "[ModelManager] Loaded GPU resources for: " + key +
        " meshes=" + std::to_string(meshCount) + "\n";
    OutputDebugStringA(msg.c_str());
#endif
}

std::string ModelManager::FindFileRecursive(const std::string& filename) const {
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

//======================
// 静的ロード関数群(旧 Function.h 移植)
//======================

MaterialData ModelManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string filename) {
    // 1. 中で必要となる変数の宣言
    // 2. ファイルを開く
    // 3. 実際にファイルを読み、MaterialDataを構築していく
    // 4. MaterialDataを返す

    ///1.2. 必要な宣言とファイルを開く

    MaterialData materialData;
    std::string line; //ファイルから読んだ1行を格納するもの
    std::ifstream file(directoryPath + "/" + filename); //ファイルを開く
#ifdef EditorMode
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + directoryPath + "/" + filename);
    }
#else
    assert(file.is_open() && "[ModelManager] Failed to open file in LoadMaterialTemplateFile.");
#endif

    ///3. ファイルを読み、MaterialDataを構築

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        // identifierに応じた処理
        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            //連結してファイルパスにする
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
    return materialData;
}

ModelData ModelManager::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    // 1. 中で必要となる変数の宣言
    // 2. ファイルを開く
    // 3. 実際にファイルを読み、ModelDataを構築していく
    // 4. ModelDataを返す

    /// 1.2.必要な変数の宣言とファイルを開く

    ModelData modelData; //構築するModelData
    std::vector<Vector4> positions; //位置
    std::vector<Vector3> normals; //法線
    std::vector<Vector2> texcoords; //テクスチャ座標
    std::string line; //ファイルから読んだ1行を格納するもの

    std::ifstream file(directoryPath + "/" + filename); //ファイルを開く
#ifdef EditorMode
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + directoryPath + "/" + filename);
    }
#else
    assert(file.is_open() && "[ModelManager] Failed to open file in LoadObjFile.");
#endif

    ///3.ファイルを読み、ModelDataを構築
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier; //先頭の識別子を読む


        //identifierに応じた処理

        ///頂点情報を読む
        if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }

        ///三角形を作る

        else if (identifier == "f") {
            VertexData triangle[3];
            //面は三角形限定。その他は未対応
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                //頂点の要素のIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
                std::istringstream v(vertexDefinition);
                uint32_t elementIndices[3];
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    std::getline(v, index, '/'); //区切りでインデックスを読んでいく
                    elementIndices[element] = std::stoi(index);
                }
                //要素へのIndexから、実際の要素の値を取得して、頂点を構築する
                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];
                //VertexData vertex = { position,texcoord,normal };
                //modelData.vertices.push_back(vertex);

                ///右手系から左手系へ

                position.x *= -1.0f;
                normal.x *= -1.0f;

                ///Texture座標の原点

                texcoord.y = 1.0f - texcoord.y;

                ///右手系から左手系へ

                triangle[faceVertex] = { position,texcoord,normal };

            }

            //頂点を逆順で登録することで、回り順を逆にする
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);
        }

        ///obj読み込みにmaterial読み込みを追加

        else if (identifier == "mtllib") {
            //materialTemplateLibraryファイルの名前を取得する
            std::string materialFilename;
            s >> materialFilename;
            //基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイルを渡す
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
    }

    modelData.rootNode = Node{};

    return modelData;
}

// f行の頂点データを安全にパースする関数例
bool ModelManager::ParseObjFaceToken(const std::string& token, int& posIdx, int& uvIdx, int& normIdx) {
    posIdx = uvIdx = normIdx = -1; // デフォルト値(0開始なら0に)

    size_t firstSlash = token.find('/');
    size_t secondSlash = token.find('/', firstSlash + 1);

    // 位置インデックス
    if (firstSlash == std::string::npos) {
        // 例: "1"
        if (!token.empty()) posIdx = std::stoi(token);
    } else {
        // 例: "1/2/3", "1//3", "1/2"
        if (firstSlash > 0) posIdx = std::stoi(token.substr(0, firstSlash));
        // UVインデックス
        if (secondSlash != std::string::npos) {
            // "1/2/3"
            if (secondSlash > firstSlash + 1) uvIdx = std::stoi(token.substr(firstSlash + 1, secondSlash - firstSlash - 1));
            // 法線インデックス
            if (token.size() > secondSlash + 1) normIdx = std::stoi(token.substr(secondSlash + 1));
        } else {
            // "1/2"
            if (token.size() > firstSlash + 1) uvIdx = std::stoi(token.substr(firstSlash + 1));
        }
    }
    return true;
}

ObjModel ModelManager::LoadObjFileM(const std::string& directoryPath, const std::string& filename) {
    ObjModel objModel;
    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::map<std::string, ObjMaterial> materialMap;

    std::ifstream file(directoryPath + "/" + filename);
#ifdef EditorMode
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + directoryPath + "/" + filename);
    }
#else
    assert(file.is_open() && "[ModelManager] Failed to open file in LoadObjFileM.");
#endif

    std::string line;
    ObjMesh currentMesh;

    while (std::getline(file, line)) {
        std::istringstream s(line);
        std::string id;
        s >> id;

        if (id == "v") {
            Vector4 pos;
            s >> pos.x >> pos.y >> pos.z;
            pos.w = 1.0f;
            // 左手系変換はここだけ
            pos.x *= -1.0f;
            positions.push_back(pos);
        } else if (id == "vt") {
            Vector2 uv;
            s >> uv.x >> uv.y;
            // y反転のみここで
            uv.y = 1.0f - uv.y;
            texcoords.push_back(uv);
        } else if (id == "vn") {
            Vector3 n;
            s >> n.x >> n.y >> n.z;
            // 左手系変換はここだけ
            n.x *= -1.0f;
            normals.push_back(n);
        } else if (id == "f") {
            VertexData tri[3];
            for (int i = 0; i < 3; ++i) {
                std::string def;
                s >> def;
                int pIdx = -1, tIdx = -1, nIdx = -1;
                ParseObjFaceToken(def, pIdx, tIdx, nIdx);

                Vector4 position = (pIdx > 0) ? positions[pIdx - 1] : Vector4{};
                Vector2 texcoord = (tIdx > 0) ? texcoords[tIdx - 1] : Vector2{ 0.5f, 0.5f };
                Vector3 normal = (nIdx > 0) ? normals[nIdx - 1] : Vector3{};

                tri[i] = { position, texcoord, normal };
            }
            // 三角形の回り順は逆にしている(必要な場合のみ)
            currentMesh.vertices.push_back(tri[2]);
            currentMesh.vertices.push_back(tri[1]);
            currentMesh.vertices.push_back(tri[0]);
        } else if (id == "usemtl") {
            if (!currentMesh.vertices.empty()) {
                objModel.meshes.push_back(currentMesh);
                currentMesh = ObjMesh();
            }
            std::string matName;
            s >> matName;
            if (materialMap.count(matName)) {
                currentMesh.material = materialMap[matName];
            } else {
                currentMesh.material = ObjMaterial(); // デフォルト値
            }
        } else if (id == "mtllib") {
            std::string mtlFilename;
            s >> mtlFilename;
            std::ifstream mtlFile(directoryPath + "/" + mtlFilename);
#ifdef EditorMode
            if (!mtlFile.is_open()) {
                throw std::runtime_error("Failed to open mtl file: " + directoryPath + "/" + mtlFilename);
            }
#else
            assert(mtlFile.is_open() && "[ModelManager] Failed to open mtl file in LoadObjFileM.");
#endif

            std::string mtlLine, currentName;
            while (std::getline(mtlFile, mtlLine)) {
                std::istringstream ms(mtlLine);
                std::string mtlId;
                ms >> mtlId;

                if (mtlId == "newmtl") {
                    ms >> currentName;
                    materialMap[currentName] = ObjMaterial();
                } else if (mtlId == "Kd") {
                    ms >> materialMap[currentName].color.x
                        >> materialMap[currentName].color.y
                        >> materialMap[currentName].color.z;
                    materialMap[currentName].color.w = 1.0f;
                } else if (mtlId == "Ka") {
                    ms >> materialMap[currentName].ambient.x
                        >> materialMap[currentName].ambient.y
                        >> materialMap[currentName].ambient.z;
                } else if (mtlId == "Ks") {
                    ms >> materialMap[currentName].specular.x
                        >> materialMap[currentName].specular.y
                        >> materialMap[currentName].specular.z;
                } else if (mtlId == "Ns") {
                    float ns = 0.0f;
                    ms >> ns;
                    // Ns (0-1000) を Roughness (0-1) に変換 (簡易的な近似)
                    materialMap[currentName].roughness = std::clamp(1.0f - (ns / 1000.0f), 0.0f, 1.0f);
                } else if (mtlId == "d" || mtlId == "Tr") {
                    ms >> materialMap[currentName].alpha;
                } else if (mtlId == "map_Kd") {
                    std::string token;
                    bool hasTransform = false;
                    // テクスチャオプション対応
                    while (ms >> token) {
                        if (token == "-o") {
                            ms >> materialMap[currentName].uvTransform.m[3][0]
                                >> materialMap[currentName].uvTransform.m[3][1];
                            hasTransform = true;
                        } else if (token == "-s") {
                            ms >> materialMap[currentName].uvTransform.m[0][0]
                                >> materialMap[currentName].uvTransform.m[1][1];
                            hasTransform = true;
                        } else {
                            materialMap[currentName].textureFilePath = directoryPath + "/" + token;
                            break;
                        }
                    }
                    // デフォルト値セット
                    if (!hasTransform) {
                        materialMap[currentName].uvTransform = Math::MakeAffineMatrix(
                            { 1.0f, 1.0f, 1.0f }, Vector3{ 0,0,0 }, { 0,0,0 });
                    }
                } else if (mtlId == "map_Bump" || mtlId == "bump") {
                    std::string token;
                    bool hasTransform = false;
                    // テクスチャオプション対応
                    while (ms >> token) {
                        if (token == "-o" || token == "-s" || token == "-bm") {
                            // UVTransformやバンプ係数の読み飛ばし
                            std::string dummy;
                            ms >> dummy;
                            if (token != "-bm") ms >> dummy; // -o, -sは2要素
                        } else {
                            materialMap[currentName].normalMapFilePath = directoryPath + "/" + token;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (!currentMesh.vertices.empty()) {
        objModel.meshes.push_back(currentMesh);
    }

    // 手書きパーサでは階層情報はないため空 Node
    objModel.rootNode = Node{};

    // 境界球を計算
    CalculateBoundingSphere(objModel);

    return objModel;
}

ModelData ModelManager::LoadModelFile(const std::string& directoryPath, const std::string& filename) {

    ModelData modelData; //構築するModelData

    /*いろんなフォーマットのモデルが読みたい*/

    /// assimpでobjを読む

    // ファイルからassimpのSceneを構築する
    // assimpのデータ構造 → https://learnopengl.com/Model-Loading/Assimp
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    // assimpでは読み込む際にオプションを指定することができる
    // 今回はobjからDirectX12の形式に合わせるために
    // ・ aiProcess_FlipWindingOrder : 三角形の並び順を逆にする
    // ・ aiProcess_FlipUVs : UVをフリップする(texcoord.y = 1.0f - texcoord.y;の処理)
    // を指定した。
    // ほかのオプション → https://github.com/assimp/assimp/blob/master/include/assimp/postprocess.h#L60
    const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

    /// meshを解析する

    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        assert(mesh->HasNormals()); // 法線がないMeshは今回は非対応
        assert(mesh->HasTextureCoords(0)); // TexcoordがないMeshは今回は非対応
        // ここからMeshの中身(Face)の解析を行っていく

        /// vertexを解析する
        modelData.vertices.resize(mesh->mNumVertices);
        for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            aiVector3D& position = mesh->mVertices[vertexIndex];
            aiVector3D& normal = mesh->mNormals[vertexIndex];
            aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
            // 右手系->左手系への変換を忘れずに
            modelData.vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };
            modelData.vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
            modelData.vertices[vertexIndex].texcoord = { texcoord.x, texcoord.y };
        }

        /*DrawIndexed*/

        /// Indexを解析する
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            aiFace& face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3); // 三角形のみサポート

            for (uint32_t element = 0; element < face.mNumIndices; ++element) {
                uint32_t vertexIndex = face.mIndices[element];
                modelData.indices.push_back(vertexIndex);
            }
        }

        /*Skinning*/


        /// SkinCluster構築用のデータ取得を追加

        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {

            /// Jointごとの格納領域を作る

            // meshに関連付けられたJointから情報を取得する
            // assimpではJointをBoneと呼び、Skinningに必要なデータが保持されている
            aiBone* bone = mesh->mBones[boneIndex];
            std::string jointName = bone->mName.C_Str();
            JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

            /// InverseBindPoseMatrixの抽出

            // assimpでは、JointのInverseBindPoseMatrixはmOffsetMatrixによって保持される。
            // assimpは右手系の列ベクトルなので、左手系で直接使用することは適さない。
            // したがって、BindPose時の各成分を抽出し、必要な変換を施す必要がある
            aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
            aiVector3D scale, translate;
            aiQuaternion rotate;
            bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
            Matrix4x4 bindPoseMatrix = Math::MakeAffineMatrix(Vector3{ scale.x,scale.y,scale.z }, Quaternion{ rotate.x,-rotate.y,-rotate.z,rotate.w }, Vector3{ -translate.x,translate.y,translate.z });
            jointWeightData.inverseBindPoseMatrix = Math::Inverse(bindPoseMatrix);

            /// Weight情報を取り出す

            // Jointに関連付けられた頂点のweightとその頂点のindexを取り出して格納する
            // mVertexIdは該当Mesh内でのIndexである
            //  MultiMesh/MultiMaterial対応する際にはこのまま保存するのではなく、全体を通して改良が必要である
            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                jointWeightData.vertexWeights.push_back({ bone->mWeights[weightIndex].mWeight,bone->mWeights[weightIndex].mVertexId });
            }
        }
    }

    /*いろんなフォーマットのモデルが読みたい*/

    /// materialを解析する

    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        aiMaterial* material = scene->mMaterials[materialIndex];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString textureFilePath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
            modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
        }
    }

    /*glTFを読み込んでみよう*/

    /// assimpでNodを解析する

    modelData.rootNode = ReadNode(scene->mRootNode);

    return modelData;

}

// ノードとメッシュの関連を解析するヘルパー関数
void ProcessNode(aiNode* node, const aiScene* scene, std::vector<ObjMesh>& meshes) {
    // 現在のノードが持つメッシュを処理
    for (UINT i = 0; i < node->mNumMeshes; i++) {
        UINT meshIndex = node->mMeshes[i];
        if (meshIndex < meshes.size()) {
            meshes[meshIndex].nodeName = node->mName.C_Str();
        }
    }
    // 子ノードを再帰的に処理
    for (UINT i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, meshes);
    }
}

// ObjModel Node 対応 Assimp 版
ObjModel ModelManager::LoadModelFromFile(const std::string& directoryPath, const std::string& filename) {
    ObjModel objModel;

    /* いろんなフォーマットのモデルが読みたい */

    /// assimpでobj(glTF等も含む汎用)を読む

    Assimp::Importer importer;
    const std::string filePath = directoryPath + "/" + filename;

    // 読み込み時オプション:
    // ・ aiProcess_Triangulate        : 非三角形ポリゴンを三角化
    // ・ aiProcess_FlipWindingOrder  : 三角形の並び順を逆にして表裏判定を左手系用に合わせる
    // ・ aiProcess_FlipUVs           : UVのV(y)成分を反転
    // ・ aiProcess_MakeLeftHanded    : 右手座標系から左手座標系へ変換(Z反転、行列の調整など全て行う)
    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_FlipWindingOrder |
        aiProcess_FlipUVs |
        aiProcess_MakeLeftHanded; // このフラグを追加

    const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
#ifdef EditorMode
    if (!scene || !scene->HasMeshes()) {
        throw std::runtime_error("Assimp failed to load model or no meshes found: " + std::string(importer.GetErrorString()));
    }
#else
    assert(scene && scene->HasMeshes() && "[ModelManager] Assimp failed to load model or no meshes found in LoadModelFromFile.");
#endif

    /// material(assimpのaiMaterial)をObjMaterialへ変換

    std::vector<ObjMaterial> convertedMaterials(scene->mNumMaterials);

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        const aiMaterial* m = scene->mMaterials[i];
        ObjMaterial out{};

        // デフォルト初期化 (※ 読み込めなかったパラメータを安全値で埋める)
        out.textureFilePath = "";
        out.color = { 1.0f,1.0f,1.0f,1.0f };
        out.ambient = { 0.0f,0.0f,0.0f };
        out.specular = { 0.0f,0.0f,0.0f };
        out.roughness = 0.5f;
        out.metallic = 0.0f;
        out.alpha = 1.0f;
        out.enableLighting = true;
        out.lightingMode = 3; // PBR
        out.environmentCoefficient = 0.0f;
        out.uvTransform = Math::MakeAffineMatrix({ 1.0f,1.0f,1.0f }, Vector3{ 0,0,0 }, { 0,0,0 });

        // Diffuse テクスチャ (埋め込み "*0" 等は今回は未対応)
        if (m->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            if (m->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS) {
                std::string p = texPath.C_Str();
                if (!p.empty() && p[0] != '*') {
                    // テクスチャのパスをモデルファイルからの相対パスとして解決
                    std::filesystem::path modelPath(filePath);
                    std::filesystem::path texturePath = modelPath.parent_path() / p;
                    out.textureFilePath = texturePath.string();
                    std::replace(out.textureFilePath.begin(), out.textureFilePath.end(), '\\', '/');
                }
            }
        }

        // Normal テクスチャ
        if (m->GetTextureCount(aiTextureType_NORMALS) > 0 || m->GetTextureCount(aiTextureType_HEIGHT) > 0) {
            aiString texPath;
            // NORMALSを優先し、なければHEIGHT(Bump)を取得
            if (m->GetTexture(aiTextureType_NORMALS, 0, &texPath) == aiReturn_SUCCESS ||
                m->GetTexture(aiTextureType_HEIGHT,  0, &texPath) == aiReturn_SUCCESS) {
                std::string p = texPath.C_Str();
                if (!p.empty() && p[0] != '*') {
                    std::filesystem::path modelPath(filePath);
                    std::filesystem::path texturePath = modelPath.parent_path() / p;
                    out.normalMapFilePath = texturePath.string();
                    std::replace(out.normalMapFilePath.begin(), out.normalMapFilePath.end(), '\\', '/');
                }
            }
        }

        // 色/光沢/不透明度 (取得できたもののみ上書き)
        aiColor3D kd;
        if (m->Get(AI_MATKEY_COLOR_DIFFUSE, kd) == aiReturn_SUCCESS) {
            out.color.x = kd.r; out.color.y = kd.g; out.color.z = kd.b; out.color.w = 1.0f;
        }
        aiColor3D ka;
        if (m->Get(AI_MATKEY_COLOR_AMBIENT, ka) == aiReturn_SUCCESS) {
            out.ambient.x = ka.r; out.ambient.y = ka.g; out.ambient.z = ka.b;
        }
        aiColor3D ks;
        if (m->Get(AI_MATKEY_COLOR_SPECULAR, ks) == aiReturn_SUCCESS) {
            out.specular.x = ks.r; out.specular.y = ks.g; out.specular.z = ks.b;
        }
        float shininess = 0.0f;
        if (m->Get(AI_MATKEY_SHININESS, shininess) == aiReturn_SUCCESS) {
            // Shininess (Blinn-Phong) から Roughness への変換
            // Roughness = sqrt(2 / (shininess + 2)) が一般的な近似
            out.roughness = std::clamp(std::sqrt(2.0f / (shininess + 2.0f)), 0.0f, 1.0f);
        }
        float opacity = 1.0f;
        if (m->Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS) {
            out.alpha = opacity;
            out.color.w = opacity;
        }

        convertedMaterials[i] = out;
    }

    /// mesh(aiMesh)を解析し ObjMesh を構築

    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        ObjMesh outMesh;

        // マテリアル割り当て (安全に index 範囲内か確認)
        if (mesh->mMaterialIndex < convertedMaterials.size()) {
            outMesh.material = convertedMaterials[mesh->mMaterialIndex];
        }

        // 頂点データの読み込み
        outMesh.vertices.resize(mesh->mNumVertices);
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
            const aiVector3D& p = mesh->mVertices[i];
            const aiVector3D& n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
            const aiVector3D& t = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0.5f, 0.5f, 0);

            VertexData& v = outMesh.vertices[i];
            // Assimpが変換してくれるので、手動での反転は不要になる
            v.position = { p.x, p.y, p.z, 1.0f };
            v.normal = { n.x, n.y, n.z };
            v.texcoord = { t.x, t.y };
        }

        // インデックスデータの読み込み
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3);
            outMesh.indices.push_back(face.mIndices[0]);
            outMesh.indices.push_back(face.mIndices[1]);
            outMesh.indices.push_back(face.mIndices[2]);
        }

        // SkinCluster構築用のデータ取得を追加
        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string jointName = bone->mName.C_Str();
            JointWeightData& jointWeightData = objModel.skinClusterData[jointName];

            // InverseBindPoseMatrixの抽出
            aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
            aiVector3D scale, translate;
            aiQuaternion rotate;
            bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
            // Assimpは左手座標系変換済みなので、そのままMatrixを作成
            Matrix4x4 bindPoseMatrix = Math::MakeAffineMatrix({ scale.x, scale.y, scale.z }, { rotate.x, rotate.y, rotate.z, rotate.w }, { translate.x, translate.y, translate.z });
            jointWeightData.inverseBindPoseMatrix = Math::Inverse(bindPoseMatrix);

            // Weight情報を取り出す
            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                jointWeightData.vertexWeights.push_back({ bone->mWeights[weightIndex].mWeight, bone->mWeights[weightIndex].mVertexId });
            }
        }

        objModel.meshes.push_back(std::move(outMesh));
    }

    // ノードとメッシュの関連付けを解析
    ProcessNode(scene->mRootNode, scene, objModel.meshes);

    /// Node 階層(structure)を解析 (シーンルートから再帰構築)

    objModel.rootNode = ReadNode(scene->mRootNode);

    // 境界球を計算
    CalculateBoundingSphere(objModel);

    return objModel;
}


/*glTFを読み込んでみよう*/

/// 前準備

Node ModelManager::ReadNode(aiNode* node) {
    Node result;

    // aiProcess_MakeLeftHandedフラグにより、Assimpが座標系変換をすでに行っている。
    // そのため、ここでの手動変換は不要。
    // Assimpから渡される行列をそのままローカル行列として使用する。
    aiMatrix4x4 aiLocalMatrix = node->mTransformation; // nodeのlocalMatrixを取得
    aiLocalMatrix.Transpose(); // Assimpの列ベクトル形式を行ベクトル形式に転置

    // Matrix4x4にコピー
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            result.localMatrix.m[r][c] = aiLocalMatrix[r][c];
        }
    }

    // SRTの分解もAssimpの変換後の値から行う
    aiVector3D scale, translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale, rotate, translate);
    result.transform.scale = { scale.x, scale.y, scale.z };
    result.transform.rotate = { rotate.x, rotate.y, rotate.z, rotate.w };
    result.transform.translate = { translate.x, translate.y, translate.z };

    result.name = node->mName.C_Str(); // Node名を格納
    result.children.resize(node->mNumChildren); // 子供の数だけメモリを確保
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        // 再帰的にReadNodeを呼び出し、階層構造を構築する
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }
    return result;
}

void ModelManager::CalculateBoundingSphere(ObjModel& model) {
    Vector3 minV = { (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() };
    Vector3 maxV = { (std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)() };
    bool hasVertices = false;

    // 1pass: 全メッシュの頂点を走査してAABBを求める
    for (const auto& mesh : model.meshes) {
        for (const auto& vertex : mesh.vertices) {
            minV.x = (std::min)(minV.x, vertex.position.x);
            minV.y = (std::min)(minV.y, vertex.position.y);
            minV.z = (std::min)(minV.z, vertex.position.z);
            maxV.x = (std::max)(maxV.x, vertex.position.x);
            maxV.y = (std::max)(maxV.y, vertex.position.y);
            maxV.z = (std::max)(maxV.z, vertex.position.z);
            hasVertices = true;
        }
    }

    if (!hasVertices) {
        model.boundingBox = AABB{ {0,0,0}, {0,0,0} };
        model.boundingSphere.center = { 0, 0, 0 };
        model.boundingSphere.radius = 0.0f;
        return;
    }

    model.boundingBox = AABB{ minV, maxV };

    // 中心点をAABBの重心とする
    model.boundingSphere.center = (minV + maxV) * 0.5f;

    // 2pass: 中心点から最も遠い頂点までの距離を半径とする
    float maxDistSq = 0.0f;
    for (const auto& mesh : model.meshes) {
        for (const auto& vertex : mesh.vertices) {
            Vector3 pos = { vertex.position.x, vertex.position.y, vertex.position.z };
            Vector3 diff = pos - model.boundingSphere.center;
            float distSq = Math::Dot(diff, diff);
            maxDistSq = (std::max)(maxDistSq, distSq);
        }
    }

    model.boundingSphere.radius = std::sqrt(maxDistSq);
}

namespace {
    // レイと三角形の交差判定
    bool IntersectRayTriangle(const Vector3& origin, const Vector3& direction,
        const Vector3& v0, const Vector3& v1, const Vector3& v2,
        float& t) {
        const float kEpsilon = 1e-6f;
        Vector3 edge1 = Math::Subtract(v1, v0);
        Vector3 edge2 = Math::Subtract(v2, v0);
        Vector3 h = Math::Cross(direction, edge2);
        float a = Math::Dot(edge1, h);
        if (a > -kEpsilon && a < kEpsilon)
            return false; // レイは三角形と平行

        float f = 1.0f / a;
        Vector3 s = Math::Subtract(origin, v0);
        float u = f * Math::Dot(s, h);
        if (u < 0.0f || u > 1.0f)
            return false;

        Vector3 q = Math::Cross(s, edge1);
        float v = f * Math::Dot(direction, q);
        if (v < 0.0f || u + v > 1.0f)
            return false;

        t = f * Math::Dot(edge2, q);
        return (t > kEpsilon);
    }

    // 点と三角形の最近接点を求める
    Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c) {
        Vector3 ab = b - a;
        Vector3 ac = c - a;
        Vector3 ap = p - a;
        float d1 = Math::Dot(ab, ap);
        float d2 = Math::Dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        Vector3 bp = p - b;
        float d3 = Math::Dot(ab, bp);
        float d4 = Math::Dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return b;

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            float v = d1 / (d1 - d3);
            return a + v * ab;
        }

        Vector3 cp = p - c;
        float d5 = Math::Dot(ab, cp);
        float d6 = Math::Dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            float w = d2 / (d2 - d6);
            return a + w * ac;
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            return b + w * (c - b);
        }

        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        return a + ab * v + ac * w;
    }

    // 重心座標を計算
    Vector3 Barycentric(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c) {
        Vector3 v0 = b - a, v1 = c - a, v2 = p - a;
        float d00 = Math::Dot(v0, v0);
        float d01 = Math::Dot(v0, v1);
        float d11 = Math::Dot(v1, v1);
        float d20 = Math::Dot(v2, v0);
        float d21 = Math::Dot(v2, v1);
        float denom = d00 * d11 - d01 * d01;
        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        float u = 1.0f - v - w;
        return { u, v, w };
    }
}

VoxelizedModel ModelManager::VoxelizeModel(const ObjModel& model, const Vector3Int& resolution, TextureManager* textureManager) {
    VoxelizedModel result;
    result.resolution = resolution;

    // 1. AABB(バウンディングボックス)の計算
    result.aabbMin = { (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() };
    result.aabbMax = { (std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)() };

    for (const auto& mesh : model.meshes) {
        for (const auto& vertex : mesh.vertices) {
            result.aabbMin.x = (std::min)(result.aabbMin.x, vertex.position.x);
            result.aabbMin.y = (std::min)(result.aabbMin.y, vertex.position.y);
            result.aabbMin.z = (std::min)(result.aabbMin.z, vertex.position.z);
            result.aabbMax.x = (std::max)(result.aabbMax.x, vertex.position.x);
            result.aabbMax.y = (std::max)(result.aabbMax.y, vertex.position.y);
            result.aabbMax.z = (std::max)(result.aabbMax.z, vertex.position.z);
        }
    }

    Vector3 aabbSize = {
        result.aabbMax.x - result.aabbMin.x,
        result.aabbMax.y - result.aabbMin.y,
        result.aabbMax.z - result.aabbMin.z
    };
    Vector3 voxelSize = { aabbSize.x / resolution.x, aabbSize.y / resolution.y, aabbSize.z / resolution.z };

    // 2. 全てのボクセルをループ処理
    for (int z = 0; z < resolution.z; ++z) {
        for (int y = 0; y < resolution.y; ++y) {
            for (int x = 0; x < resolution.x; ++x) {
                // 3. 各ボクセルの中心座標を計算
                Vector3 voxelCenter = {
                    result.aabbMin.x + (x + 0.5f) * voxelSize.x,
                    result.aabbMin.y + (y + 0.5f) * voxelSize.y,
                    result.aabbMin.z + (z + 0.5f) * voxelSize.z
                };

                int intersections = 0;

                // 3方向にレイを飛ばして多数決で内外判定 (1方向だと法線平行のポリゴンで誤判定しやすい)
                const Vector3 rayDirs[3] = {
                    { 1.0f, 0.0f, 0.0f }, // X+
                    { 0.0f, 1.0f, 0.0f }, // Y+
                    { 0.0f, 0.0f, 1.0f }, // Z+
                };
                int intersectionPerDir[3] = { 0, 0, 0 };

                float minDistance = (std::numeric_limits<float>::max)();
                const ObjMesh* closestMesh = nullptr;
                VertexData closestTri[3];

                // 4. メッシュとの交差判定と最も近いポリゴンの探索
                for (const auto& mesh : model.meshes) {
                    size_t faceCount = mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size();
                    for (size_t i = 0; i < faceCount; i += 3) {
                        VertexData v0 = mesh.indices.empty() ? mesh.vertices[i] : mesh.vertices[mesh.indices[i]];
                        VertexData v1 = mesh.indices.empty() ? mesh.vertices[i + 1] : mesh.vertices[mesh.indices[i + 1]];
                        VertexData v2 = mesh.indices.empty() ? mesh.vertices[i + 2] : mesh.vertices[mesh.indices[i + 2]];

                        Vector3 p0 = { v0.position.x, v0.position.y, v0.position.z };
                        Vector3 p1 = { v1.position.x, v1.position.y, v1.position.z };
                        Vector3 p2 = { v2.position.x, v2.position.y, v2.position.z };

                        // 3方向それぞれ独立にカウント
                        for (int d = 0; d < 3; ++d) {
                            float t;
                            if (IntersectRayTriangle(voxelCenter, rayDirs[d], p0, p1, p2, t) && t > 0.0f) {
                                intersectionPerDir[d]++;
                            }
                        }

                        // 最も近いポリゴンを見つける
                        Vector3 closestPoint = ClosestPointOnTriangle(voxelCenter, p0, p1, p2);
                        Vector3 diff = closestPoint - voxelCenter;
                        float distanceSq = Math::Dot(diff, diff);

                        if (distanceSq < minDistance) {
                            minDistance = distanceSq;
                            closestMesh = &mesh;
                            closestTri[0] = v0;
                            closestTri[1] = v1;
                            closestTri[2] = v2;
                        }
                    }
                }

                // 5. 内外判定：各方向の交差回数が奇数なら「内部」→ 2/3以上で内部と判断（多数決）
                int insideVotes = 0;
                for (int d = 0; d < 3; ++d) {
                    if (intersectionPerDir[d] % 2 != 0) {
                        insideVotes++;
                    }
                }

                // 内部のボクセルのみ生成（2/3方向以上が内部判定で採用）
                if (insideVotes >= 2)
                {
                    Voxel newVoxel;
                    newVoxel.position = voxelCenter;
                    newVoxel.normal = { 0.0f, 1.0f, 0.0f };      // 初期法線
                    newVoxel.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 初期色
                    newVoxel.uv = { 0.0f, 0.0f };                // 初期UV

                    if (closestMesh != nullptr) {
                        Vector3 p0 = { closestTri[0].position.x, closestTri[0].position.y, closestTri[0].position.z };
                        Vector3 p1 = { closestTri[1].position.x, closestTri[1].position.y, closestTri[1].position.z };
                        Vector3 p2 = { closestTri[2].position.x, closestTri[2].position.y, closestTri[2].position.z };

                        // Barycentric(重心座標)の計算
                        Vector3 closestPoint = ClosestPointOnTriangle(voxelCenter, p0, p1, p2);
                        Vector3 uvw = Barycentric(closestPoint, p0, p1, p2);

                        // ==========================================
                        // 法線(Normal)の補間と設定
                        // ==========================================
                        Vector3 n0 = closestTri[0].normal;
                        Vector3 n1 = closestTri[1].normal;
                        Vector3 n2 = closestTri[2].normal;

                        Vector3 interpolatedNormal = {
                            n0.x * uvw.x + n1.x * uvw.y + n2.x * uvw.z,
                            n0.y * uvw.x + n1.y * uvw.y + n2.y * uvw.z,
                            n0.z * uvw.x + n1.z * uvw.y + n2.z * uvw.z
                        };
                        newVoxel.normal = Math::Normalize(interpolatedNormal); // 正規化してボクセルに保存

                        // UVの取得
                        Vector2 uv0 = closestTri[0].texcoord;
                        Vector2 uv1 = closestTri[1].texcoord;
                        Vector2 uv2 = closestTri[2].texcoord;

                        Vector2 interpolatedUV = {
                            uv0.x * uvw.x + uv1.x * uvw.y + uv2.x * uvw.z,
                            uv0.y * uvw.x + uv1.y * uvw.y + uv2.y * uvw.z
                        };
                        newVoxel.uv = interpolatedUV;

                        // ==========================================
                        // 法線マップからの詳細な法線の計算・焼き付け
                        // ==========================================
                        if (!closestMesh->material.normalMapFilePath.empty() && textureManager) {
                            const DirectX::ScratchImage* nimg = textureManager->GetScratchImage(closestMesh->material.normalMapFilePath);
                            if (nimg) {
                                int nwidth = static_cast<int>(nimg->GetMetadata().width);
                                int nheight = static_cast<int>(nimg->GetMetadata().height);

                                int ntexX = static_cast<int>(interpolatedUV.x * nwidth) % nwidth;
                                int ntexY = static_cast<int>(interpolatedUV.y * nheight) % nheight;
                                if (ntexX < 0) ntexX += nwidth;
                                if (ntexY < 0) ntexY += nheight;

                                const DirectX::Image* nimage = nimg->GetImage(0, 0, 0);
                                if (nimage) {
                                    uint8_t* npixels = nimage->pixels;
                                    size_t nrowPitch = nimage->rowPitch;
                                    size_t npixelStride = DirectX::BitsPerPixel(nimg->GetMetadata().format) / 8;
                                    uint8_t* npixel = npixels + (ntexY * nrowPitch) + (ntexX * npixelStride);

                                    // 1. サンプリングしたRGB[0, 255]を[-1.0, 1.0]のベクトルに変換
                                    Vector3 sampledNormal;
                                    sampledNormal.x = (npixel[0] / 255.0f) * 2.0f - 1.0f;
                                    sampledNormal.y = (npixel[1] / 255.0f) * 2.0f - 1.0f;
                                    sampledNormal.z = (npixel[2] / 255.0f) * 2.0f - 1.0f;

                                    // 2. 接空間ベクトル (Tangent, Bitangent) の計算
                                    Vector3 edge1 = p1 - p0;
                                    Vector3 edge2 = p2 - p0;
                                    Vector2 deltaUV1 = uv1 - uv0;
                                    Vector2 deltaUV2 = uv2 - uv0;

                                    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
                                    
                                    Vector3 tangent;
                                    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                                    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                                    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                                    tangent = Math::Normalize(tangent);

                                    // グラム・シュミットの直交化を用いてTangentを再直交化
                                    tangent = Math::Normalize(tangent - newVoxel.normal * Math::Dot(tangent, newVoxel.normal));

                                    // Bitangentの計算 (NormalとTangentの外積に、UV方向による符号を掛ける)
                                    Vector3 bitangent = Math::Cross(newVoxel.normal, tangent);
                                    if (f < 0.0f) {
                                        bitangent.x *= -1.0f;
                                        bitangent.y *= -1.0f;
                                        bitangent.z *= -1.0f;
                                    }

                                    // 3. Tangent SpaceからLocal Spaceへの変換行列で合成
                                    // Matrix TBN( tangent, bitangent, newVoxel.normal )
                                    Vector3 localNormal;
                                    localNormal.x = tangent.x * sampledNormal.x + bitangent.x * sampledNormal.y + newVoxel.normal.x * sampledNormal.z;
                                    localNormal.y = tangent.y * sampledNormal.x + bitangent.y * sampledNormal.y + newVoxel.normal.y * sampledNormal.z;
                                    localNormal.z = tangent.z * sampledNormal.x + bitangent.z * sampledNormal.y + newVoxel.normal.z * sampledNormal.z;

                                    newVoxel.normal = Math::Normalize(localNormal);
                                }
                            }
                        }
                        // ==========================================
                        // UVからテクスチャカラーをサンプリング
                        // ==========================================

                        if (!closestMesh->material.textureFilePath.empty() && textureManager) {
                            // GetScratchImage を使用する
                            const DirectX::ScratchImage* img = textureManager->GetScratchImage(closestMesh->material.textureFilePath);

                            if (img) {
                                int width = static_cast<int>(img->GetMetadata().width);
                                int height = static_cast<int>(img->GetMetadata().height);

                                int texX = static_cast<int>(interpolatedUV.x * width) % width;
                                int texY = static_cast<int>(interpolatedUV.y * height) % height;
                                if (texX < 0) texX += width;
                                if (texY < 0) texY += height;

                                // 元のコードに合わせて GetImage(0, 0, 0) からピクセルデータを取得
                                const DirectX::Image* image = img->GetImage(0, 0, 0);
                                if (image && !DirectX::IsCompressed(img->GetMetadata().format)) {
                                    uint8_t* pixels = image->pixels;
                                    size_t rowPitch = image->rowPitch;
                                    size_t pixelStride = DirectX::BitsPerPixel(img->GetMetadata().format) / 8;
                                    
                                    // RGBA8 系統の場合のみ安全に読み取れる（雑な実装なため）
                                    if (pixelStride >= 4) {
                                      uint8_t *pixel =
                                          pixels + (texY * rowPitch) + (texX * pixelStride);
                                      newVoxel.color.x = pixel[0] / 255.0f;
                                      newVoxel.color.y = pixel[1] / 255.0f;
                                      newVoxel.color.z = pixel[2] / 255.0f;
                                      newVoxel.color.w = pixel[3] / 255.0f;
                                    } else {
                                      newVoxel.color = closestMesh->material.color;
                                    }
                                } else {
                                    // 圧縮テクスチャや不明な形式の場合はマテリアルカラーで代用
                                    newVoxel.color = closestMesh->material.color;
                                }
                            } else {
                                newVoxel.color = closestMesh->material.color;
                            }
                        } else {
                            newVoxel.color = closestMesh->material.color;
                        }
                    }

                    result.voxels.push_back(newVoxel);
                }
            }
        }
    }
    return result;
}

std::shared_ptr<VoxelizedModel> ModelManager::GetVoxelizedModel(const std::string& filename, const Vector3Int& resolution) {
    auto managedModel = GetModel(filename);
    if (!managedModel || !managedModel->cpuModel) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(managedModel->voxelMutex);
    
    // 既に同じ解像度でボクセル化されていれば、それを返す
    for (const auto& cached : managedModel->cachedVoxelModels) {
        if (cached->resolution.x == resolution.x &&
            cached->resolution.y == resolution.y &&
            cached->resolution.z == resolution.z) {
            return cached;
        }
    }

    // 見つからなければ新規計算して、キャッシュリストに追加
    auto vModel = std::make_shared<VoxelizedModel>(
        VoxelizeModel(*managedModel->cpuModel, resolution, textureManager_)
    );
    managedModel->cachedVoxelModels.push_back(vModel);
    
    return vModel;
}