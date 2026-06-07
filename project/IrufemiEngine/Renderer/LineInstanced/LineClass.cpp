#include "LineClass.h"

#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Manager/DrawManager.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/Graphics/DirectX/DescriptorPool.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/IrufemiEngine.h"

DirectXCommon* Line3DRegion::dx_ = nullptr;
DrawManager* Line3DRegion::drawManager_ = nullptr;
DescriptorPool* Line3DRegion::s_srvAllocator_ = nullptr;
IrufemiEngine* Line3DRegion::engine_ = nullptr;

Line3DRegion::~Line3DRegion() {
    if (s_srvAllocator_ && dx_) {
        for (uint32_t& idx : instancingSrvIndex_) {
            if (idx != UINT32_MAX) {
                s_srvAllocator_->FreeAfterFence(idx, dx_->GetFenceValue());
                idx = UINT32_MAX;
            }
        }
    }
}

void Line3DRegion::Initialize() {
    instances_.resize(maxInstances_);

    baseLineResource_ = std::make_unique<LineResource>();
    baseLineResource_->CreateResource();
    baseLineResource_->Map();

    // 基準となる線の頂点データ (0,0,0) -> (1,0,0)
    // VertexData: position, texcoord, normal, color
    baseLineResource_->vertexData_[0] = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} };
    baseLineResource_->vertexData_[1] = { {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} };

    baseLineResource_->indexData_[0] = 0;
    baseLineResource_->indexData_[1] = 1;
}

void Line3DRegion::Update() {
    activeCount_ = 0;
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (instances_[i].active) {
            if (activeCount_ < i) {
                instances_[activeCount_] = instances_[i];
            }
            activeCount_++;
        }
    }
}

void Line3DRegion::AddInstance(const Vector3& start, const Vector3& end, const Vector4& color, float life) {
    if (activeCount_ < maxInstances_) {
        auto& instance = instances_[activeCount_];
        instance.start = start;
        instance.end = end;
        instance.color = color;
        instance.life = life;
        instance.age = 0.0f;
        instance.active = true;
        activeCount_++;
    }
}

void Line3DRegion::ClearInstances() {
    for (size_t i = 0; i < activeCount_; ++i) {
        instances_[i].active = false;
    }
    activeCount_ = 0;
}

void Line3DRegion::BuildInstanceBuffer(bool force) {
    if (activeCount_ == 0 && !force) return;

    CreateOrResizeInstanceBuffer(static_cast<uint32_t>(activeCount_));
    uint32_t frameIndex = dx_->GetFrameIndex();
    lastUpdateFrameIndex_ = frameIndex;
    if (!instanceBuffer_[frameIndex] || !instanceData_[frameIndex] || !engine_) return;

    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) return;

    const Matrix4x4& viewProjection = Math::Multiply(activeCam->GetViewMatrix(), activeCam->GetPerspectiveFovMatrix());

    for (size_t i = 0; i < activeCount_; ++i) {
        const auto& inst = instances_[i];
        Vector3 vec = inst.end - inst.start;
        float length = Math::Length(vec);
        if (length < 1e-6f) { // ゼロ除算を避ける
            length = 1e-6f;
        }
        Matrix4x4 scale = Math::MakeScaleMatrix({ length, 1.0f, 1.0f });

        Vector3 up = { 0.0f, 1.0f, 0.0f };
        Vector3 dir = Math::Normalize(vec);
        if (abs(Math::Dot(dir, up)) > 0.999f) {
            up = { 1.0f, 0.0f, 0.0f };
        }
        Matrix4x4 rotate = Math::DirectionToDirection({ 1.0f, 0.0f, 0.0f }, dir);

        Matrix4x4 translate = Math::MakeTranslateMatrix(inst.start);
        Matrix4x4 world = scale * rotate * translate;

        instanceData_[frameIndex][i].WVP = world * viewProjection;
        instanceData_[frameIndex][i].color = inst.color;
    }
}

void Line3DRegion::SyncBeforeDraw() {
    if (isDirty_) {
        BuildInstanceBuffer();
    }
}

void Line3DRegion::Draw() {
    if (activeCount_ == 0) return;
    BuildInstanceBuffer();
    baseLineResource_->SyncBeforeDraw();
    drawManager_->SubmitLineInstanced(baseLineResource_.get(), GetInstancingSrvHandleGPU(), GetInstanceCountU32());
}

void Line3DRegion::CreateOrResizeInstanceBuffer(uint32_t instanceCount) {
    if (instanceCount == 0) return;
    uint32_t frameIndex = dx_->GetFrameIndex();
    
    if (instanceCount > instanceCapacity_[frameIndex]) {
        if (instanceBuffer_[frameIndex]) {
            instanceBuffer_[frameIndex]->Unmap(0, nullptr);
            instanceData_[frameIndex] = nullptr;
            instanceBuffer_[frameIndex].Reset();
        }
        instanceCapacity_[frameIndex] = instanceCount;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = sizeof(InstanceData) * instanceCapacity_[frameIndex];
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = dx_->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(instanceBuffer_[frameIndex].GetAddressOf()));
        if (FAILED(hr)) {
            instanceBuffer_[frameIndex].Reset();
            instanceCapacity_[frameIndex] = 0;
            return;
        }

        instanceBuffer_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_[frameIndex]));
        EnsureInstancingSRV();
    }
}

void Line3DRegion::EnsureInstancingSRV() {
    uint32_t frameIndex = dx_->GetFrameIndex();
    lastUpdateFrameIndex_ = frameIndex;
    if (!instanceBuffer_[frameIndex]) return;

    if (instancingSrvIndex_[frameIndex] == UINT32_MAX) {
        instancingSrvIndex_[frameIndex] = s_srvAllocator_->Allocate();
        instancingSrvCPU_[frameIndex] = s_srvAllocator_->GetCPUHandle(instancingSrvIndex_[frameIndex]);
        instancingSrvGPU_[frameIndex] = s_srvAllocator_->GetGPUHandle(instancingSrvIndex_[frameIndex]);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = instanceCapacity_[frameIndex];
    srvDesc.Buffer.StructureByteStride = sizeof(InstanceData);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    dx_->GetDevice()->CreateShaderResourceView(instanceBuffer_[frameIndex].Get(), &srvDesc, instancingSrvCPU_[frameIndex]);
}