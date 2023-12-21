#pragma once

#include <PCH.h>
#include <Settings.h>
#include <Graphics\GraphicsTypes.h>

using namespace SampleFramework12;

enum class HeapTypes
{
    Upload = 0,
    Default = 1,
    Custom = 2,
    GPUUpload = 3,

    NumValues
};

extern const char* HeapTypesLabels[uint32(HeapTypes::NumValues)];

extern const HeapTypes HeapTypesValues[uint32(HeapTypes::NumValues)];

typedef EnumSettingT<HeapTypes> HeapTypesSetting;

enum class CPUPageProperties
{
    NotAvailable = 0,
    WriteCombine = 1,
    WriteBack = 2,

    NumValues
};

extern const char* CPUPagePropertiesLabels[uint32(CPUPageProperties::NumValues)];

extern const CPUPageProperties CPUPagePropertiesValues[uint32(CPUPageProperties::NumValues)];

typedef EnumSettingT<CPUPageProperties> CPUPagePropertiesSetting;

enum class MemoryPools
{
    L0 = 0,
    L1 = 1,

    NumValues
};

extern const char* MemoryPoolsLabels[uint32(MemoryPools::NumValues)];

extern const MemoryPools MemoryPoolsValues[uint32(MemoryPools::NumValues)];

typedef EnumSettingT<MemoryPools> MemoryPoolsSetting;

enum class BufferTypes
{
    Raw = 0,
    Formatted = 1,
    Structured = 2,
    Constant = 3,

    NumValues
};

extern const char* BufferTypesLabels[uint32(BufferTypes::NumValues)];

extern const BufferTypes BufferTypesValues[uint32(BufferTypes::NumValues)];

typedef EnumSettingT<BufferTypes> BufferTypesSetting;

enum class BufferUploadPaths
{
    DirectQueue = 0,
    UploadCopyQueue = 1,
    FastUploadCopyQueue = 2,

    NumValues
};

extern const char* BufferUploadPathsLabels[uint32(BufferUploadPaths::NumValues)];

extern const BufferUploadPaths BufferUploadPathsValues[uint32(BufferUploadPaths::NumValues)];

typedef EnumSettingT<BufferUploadPaths> BufferUploadPathsSetting;

namespace AppSettings
{
    static const uint64 ThreadGroupSize = 256;

    extern HeapTypesSetting HeapType;
    extern CPUPagePropertiesSetting CPUPageProperty;
    extern MemoryPoolsSetting MemoryPool;
    extern BufferTypesSetting InputBufferType;
    extern IntSetting InputBufferSizeMB;
    extern IntSetting InputBufferSizeKB;
    extern IntSetting InputBufferSizeBytes;
    extern IntSetting ElemsPerThread;
    extern IntSetting ThreadElemStride;
    extern IntSetting GroupElemOffset;
    extern IntSetting ThreadElemOffset;
    extern IntSetting NumThreadGroups;
    extern BoolSetting ReadFromGPUMem;
    extern BufferUploadPathsSetting BufferUploadPath;
    extern IntSetting BackgroundUploadSize;
    extern IntSetting BackgroundUploadWaitTime;
    extern IntSetting NumInputBufferElems;
    extern IntSetting InputBufferIdx;
    extern IntSetting OutputBufferIdx;
    extern BoolSetting EnableVSync;
    extern BoolSetting StablePowerState;
    extern BoolSetting EnableDriverBackgroundThreads;

    struct AppSettingsCBuffer
    {
        int32 HeapType;
        int32 InputBufferIdx;
        int32 OutputBufferIdx;
    };

    extern ConstantBuffer CBuffer;
    const extern uint32 CBufferRegister;

    void Initialize();
    void Shutdown();
    void Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix);
    void UpdateCBuffer();
    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
    void GetShaderCompileOptions(CompileOptions& opts);
    bool ShaderCompileOptionsChanged();
};

// ================================================================================================

namespace AppSettings
{
}