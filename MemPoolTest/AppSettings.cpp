#include <PCH.h>
#include <Graphics\ShaderCompilation.h>
#include "AppSettings.h"

using namespace SampleFramework12;

const char* HeapTypesLabels[uint32(HeapTypes::NumValues)] =
{
    "Upload",
    "Default",
    "Custom",
    "GPU Upload",
};

const HeapTypes HeapTypesValues[uint32(HeapTypes::NumValues)] =
{
    HeapTypes::Upload,
    HeapTypes::Default,
    HeapTypes::Custom,
    HeapTypes::GPUUpload,
};

const char* CPUPagePropertiesLabels[uint32(CPUPageProperties::NumValues)] =
{
    "Not Available (No CPU Access)",
    "Write-Combined (Uncached)",
    "Write-Back (Cached)",
};

const CPUPageProperties CPUPagePropertiesValues[uint32(CPUPageProperties::NumValues)] =
{
    CPUPageProperties::NotAvailable,
    CPUPageProperties::WriteCombine,
    CPUPageProperties::WriteBack,
};

const char* MemoryPoolsLabels[uint32(MemoryPools::NumValues)] =
{
    "L0 (CPU RAM)",
    "L1 (VRAM)",
};

const MemoryPools MemoryPoolsValues[uint32(MemoryPools::NumValues)] =
{
    MemoryPools::L0,
    MemoryPools::L1,
};

const char* BufferTypesLabels[uint32(BufferTypes::NumValues)] =
{
    "Raw",
    "Formatted",
    "Structured",
    "Constant",
};

const BufferTypes BufferTypesValues[uint32(BufferTypes::NumValues)] =
{
    BufferTypes::Raw,
    BufferTypes::Formatted,
    BufferTypes::Structured,
    BufferTypes::Constant,
};

const char* BufferUploadPathsLabels[uint32(BufferUploadPaths::NumValues)] =
{
    "DIRECT Queue",
    "Upload COPY Queue",
    "Fast Upload COPY Queue",
};

const BufferUploadPaths BufferUploadPathsValues[uint32(BufferUploadPaths::NumValues)] =
{
    BufferUploadPaths::DirectQueue,
    BufferUploadPaths::UploadCopyQueue,
    BufferUploadPaths::FastUploadCopyQueue,
};

namespace AppSettings
{
    static SettingsContainer Settings;

    HeapTypesSetting HeapType;
    CPUPagePropertiesSetting CPUPageProperty;
    MemoryPoolsSetting MemoryPool;
    BufferTypesSetting InputBufferType;
    IntSetting InputBufferSizeMB;
    IntSetting InputBufferSizeKB;
    IntSetting InputBufferSizeBytes;
    IntSetting ElemsPerThread;
    IntSetting ThreadElemStride;
    IntSetting GroupElemOffset;
    IntSetting ThreadElemOffset;
    IntSetting NumThreadGroups;
    BoolSetting ReadFromGPUMem;
    BufferUploadPathsSetting BufferUploadPath;
    IntSetting BackgroundUploadSize;
    IntSetting BackgroundUploadWaitTime;
    IntSetting NumInputBufferElems;
    IntSetting InputBufferIdx;
    IntSetting OutputBufferIdx;
    BoolSetting EnableVSync;
    BoolSetting StablePowerState;
    BoolSetting EnableDriverBackgroundThreads;

    ConstantBuffer CBuffer;
    const uint32 CBufferRegister = 12;

    void Initialize()
    {

        Settings.Initialize(2);

        Settings.AddGroup("Test Config", true);

        Settings.AddGroup("Debug", true);

        HeapType.Initialize("HeapType", "Test Config", "Heap Type", "", HeapTypes::Upload, 4, HeapTypesLabels);
        Settings.AddSetting(&HeapType);

        CPUPageProperty.Initialize("CPUPageProperty", "Test Config", "Heap CPUPageProperty", "", CPUPageProperties::NotAvailable, 3, CPUPagePropertiesLabels);
        Settings.AddSetting(&CPUPageProperty);
        CPUPageProperty.SetVisible(false);

        MemoryPool.Initialize("MemoryPool", "Test Config", "Heap MemoryPool", "", MemoryPools::L0, 2, MemoryPoolsLabels);
        Settings.AddSetting(&MemoryPool);
        MemoryPool.SetVisible(false);

        InputBufferType.Initialize("InputBufferType", "Test Config", "Input Buffer Type", "", BufferTypes::Raw, 4, BufferTypesLabels);
        Settings.AddSetting(&InputBufferType);

        InputBufferSizeMB.Initialize("InputBufferSizeMB", "Test Config", "Input Buffer Size MB", "", 16, 0, 256);
        Settings.AddSetting(&InputBufferSizeMB);

        InputBufferSizeKB.Initialize("InputBufferSizeKB", "Test Config", "Input Buffer Size KB", "", 0, 0, 1024);
        Settings.AddSetting(&InputBufferSizeKB);

        InputBufferSizeBytes.Initialize("InputBufferSizeBytes", "Test Config", "Input Buffer Size Bytes", "", 0, 0, 1024);
        Settings.AddSetting(&InputBufferSizeBytes);

        ElemsPerThread.Initialize("ElemsPerThread", "Test Config", "Elems Per Thread", "", 1, 1, 64);
        Settings.AddSetting(&ElemsPerThread);

        ThreadElemStride.Initialize("ThreadElemStride", "Test Config", "Thread Elem Stride", "", 1, 1, 64);
        Settings.AddSetting(&ThreadElemStride);

        GroupElemOffset.Initialize("GroupElemOffset", "Test Config", "Group Elem Offset", "", 1, 0, 16);
        Settings.AddSetting(&GroupElemOffset);

        ThreadElemOffset.Initialize("ThreadElemOffset", "Test Config", "Thread Elem Offset", "", 1, 0, 16);
        Settings.AddSetting(&ThreadElemOffset);

        NumThreadGroups.Initialize("NumThreadGroups", "Test Config", "Num Thread Groups", "", 4096, 1, 65535);
        Settings.AddSetting(&NumThreadGroups);

        ReadFromGPUMem.Initialize("ReadFromGPUMem", "Test Config", "Read From GPU Memory", "", false);
        Settings.AddSetting(&ReadFromGPUMem);

        BufferUploadPath.Initialize("BufferUploadPath", "Test Config", "Buffer Upload Path", "", BufferUploadPaths::FastUploadCopyQueue, 3, BufferUploadPathsLabels);
        Settings.AddSetting(&BufferUploadPath);

        BackgroundUploadSize.Initialize("BackgroundUploadSize", "Test Config", "Background Upload Size (MB)", "", 0, 0, 256);
        Settings.AddSetting(&BackgroundUploadSize);

        BackgroundUploadWaitTime.Initialize("BackgroundUploadWaitTime", "Test Config", "Background Upload Size Wait Time (ms)", "", 0, 0, 100);
        Settings.AddSetting(&BackgroundUploadWaitTime);

        NumInputBufferElems.Initialize("NumInputBufferElems", "Test Config", "Num Input Buffer Elems", "", 0, -2147483648, 2147483647);
        Settings.AddSetting(&NumInputBufferElems);
        NumInputBufferElems.SetVisible(false);

        InputBufferIdx.Initialize("InputBufferIdx", "Test Config", "Input Buffer Idx", "", -1, -2147483648, 2147483647);
        Settings.AddSetting(&InputBufferIdx);
        InputBufferIdx.SetVisible(false);

        OutputBufferIdx.Initialize("OutputBufferIdx", "Test Config", "Output Buffer Idx", "", -1, -2147483648, 2147483647);
        Settings.AddSetting(&OutputBufferIdx);
        OutputBufferIdx.SetVisible(false);

        EnableVSync.Initialize("EnableVSync", "Debug", "Enable VSync", "Enables or disables vertical sync during Present", true);
        Settings.AddSetting(&EnableVSync);

        StablePowerState.Initialize("StablePowerState", "Debug", "Stable Power State", "Enables the stable power state, which stabilizes GPU clocks for more consistent performance", true);
        Settings.AddSetting(&StablePowerState);

        EnableDriverBackgroundThreads.Initialize("EnableDriverBackgroundThreads", "Debug", "Enable Driver Background Threads", "", false);
        Settings.AddSetting(&EnableDriverBackgroundThreads);

        ConstantBufferInit cbInit;
        cbInit.Size = sizeof(AppSettingsCBuffer);
        cbInit.Dynamic = true;
        cbInit.Name = L"AppSettings Constant Buffer";
        CBuffer.Initialize(cbInit);
    }

    void Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix)
    {
        Settings.Update(displayWidth, displayHeight, viewMatrix);

    }

    void UpdateCBuffer()
    {
        AppSettingsCBuffer cbData;
        cbData.HeapType = HeapType;
        cbData.InputBufferIdx = InputBufferIdx;
        cbData.OutputBufferIdx = OutputBufferIdx;

        CBuffer.MapAndSetData(cbData);
    }

    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
    {
        CBuffer.SetAsGfxRootParameter(cmdList, rootParameter);
    }

    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
    {
        CBuffer.SetAsComputeRootParameter(cmdList, rootParameter);
    }

    void GetShaderCompileOptions(CompileOptions& opts)
    {
    }

    bool ShaderCompileOptionsChanged()
    {
        bool changed = false;
        return changed;
    }

    void Shutdown()
    {
        CBuffer.Shutdown();
    }
}

// ================================================================================================

namespace AppSettings
{
}