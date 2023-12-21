//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"
#include "DX12.h"
#include "DX12_Upload.h"
#include "DX12_Helpers.h"
#include "GraphicsTypes.h"

#if Debug_
    #define UseDebugDevice_ 1
    #define BreakOnDXError_ (UseDebugDevice_ && 1)
    #define UseGPUValidation_ 0
#else
    #define UseDebugDevice_ 0
    #define BreakOnDXError_ 0
    #define UseGPUValidation_ 0
#endif

// Make sure we've got the right version of d3d12.h from the DX Agility SDK
StaticAssert_(D3D_SHADER_FEATURE_RESOURCE_DESCRIPTOR_HEAP_INDEXING);

// Add our magic exports so that the D3D12 loader finds D3D12Core.dll
#if EnablePreviewDX12SDK_
    extern "C" { _declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_PREVIEW_SDK_VERSION;}
#else
    extern "C" { _declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;}
#endif

extern "C" { _declspec(dllexport) extern const char8_t* D3D12SDKPath = u8".\\D3D12\\"; }

namespace SampleFramework12
{

namespace DX12
{

ID3D12Device10* Device = nullptr;
#if EnableWorkGraphsPreview_
    ID3D12GraphicsCommandListExperimental* CmdList = nullptr;
#else
    ID3D12GraphicsCommandList7* CmdList = nullptr;
#endif
ID3D12CommandQueue* GfxQueue = nullptr;
D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
IDXGIFactory4* Factory = nullptr;
IDXGIAdapter1* Adapter = nullptr;

uint64 CurrentCPUFrame = 0;
uint64 CurrentGPUFrame = 0;
uint64 CurrFrameIdx = 0;

static const uint64 NumCmdAllocators = RenderLatency;

static ID3D12CommandAllocator* CmdAllocators[NumCmdAllocators] = { };
static Fence FrameFence;

static List<IUnknown*> DeferredReleases[RenderLatency];
static bool ShuttingDown = false;

struct DeferredSRVCreate
{
    ID3D12Resource* Resource = nullptr;
    D3D12_SHADER_RESOURCE_VIEW_DESC Desc = { };
    uint32 DescriptorIdx = uint32(-1);
};

static Array<DeferredSRVCreate> DeferredSRVCreates[RenderLatency];
static volatile uint64 DeferredSRVCreateCount[RenderLatency] = { };

static void ProcessDeferredReleases(uint64 frameIdx)
{
    for(uint64 i = 0; i < DeferredReleases[frameIdx].Count(); ++i)
        DeferredReleases[frameIdx][i]->Release();
    DeferredReleases[frameIdx].RemoveAll();
}

static void ProcessDeferredSRVCreates(uint64 frameIdx)
{
    uint64 createCount = DeferredSRVCreateCount[frameIdx];
    for(uint64 i = 0; i < createCount; ++i)
    {
        DeferredSRVCreate& create = DeferredSRVCreates[frameIdx][i];
        Assert_(create.Resource != nullptr);

        D3D12_CPU_DESCRIPTOR_HANDLE handle = SRVDescriptorHeap.CPUHandleFromIndex(create.DescriptorIdx, frameIdx);
        Device->CreateShaderResourceView(create.Resource, &create.Desc, handle);

        create.Resource = nullptr;
        create.DescriptorIdx = uint32(-1);
    }

    DeferredSRVCreateCount[frameIdx] = 0;
}

void Initialize(D3D_FEATURE_LEVEL minFeatureLevel, uint32 adapterIdx)
{
    ShuttingDown = false;

    #if EnableWorkGraphsPreview_
        const UUID experimentalFeatures[2] = { D3D12ExperimentalShaderModels,D3D12StateObjectsExperiment };
        DXCall(D3D12EnableExperimentalFeatures(ArraySize_(experimentalFeatures), experimentalFeatures, nullptr, nullptr));
    #endif

    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&Factory));
    if(FAILED(hr))
        throw Exception(L"Unable to create a DXGI 1.4 device.\n "
                        L"Make sure that your OS and driver support DirectX 12");

    LARGE_INTEGER umdVersion = { };
    Factory->EnumAdapters1(adapterIdx, &Adapter);

    if(Adapter == nullptr)
        throw Exception(L"Unable to locate a DXGI 1.4 adapter that supports a D3D12 device.\n"
                        L"Make sure that your OS and driver support DirectX 12");

    DXGI_ADAPTER_DESC1 desc = { };
    Adapter->GetDesc1(&desc);
    WriteLog("Creating DX12 device on adapter '%ls'", desc.Description);

    #if UseDebugDevice_
        ID3D12DebugPtr d3d12debug;
        DXCall(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12debug)));
        d3d12debug->EnableDebugLayer();

        #if UseGPUValidation_
            ID3D12Debug1Ptr debug1;
            d3d12debug->QueryInterface(IID_PPV_ARGS(&debug1));
            debug1->SetEnableGPUBasedValidation(true);
        #endif
    #endif

    DXCall(D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Device)));

    // Check the maximum feature level, and make sure it's above our minimum
    D3D_FEATURE_LEVEL featureLevelsArray[5];
    featureLevelsArray[0] = D3D_FEATURE_LEVEL_11_0;
    featureLevelsArray[1] = D3D_FEATURE_LEVEL_11_1;
    featureLevelsArray[2] = D3D_FEATURE_LEVEL_12_0;
    featureLevelsArray[3] = D3D_FEATURE_LEVEL_12_1;
    featureLevelsArray[4] = D3D_FEATURE_LEVEL_12_2;
    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels = { };
    featureLevels.NumFeatureLevels = ArraySize_(featureLevelsArray);
    featureLevels.pFeatureLevelsRequested = featureLevelsArray;
    DXCall(Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels)));
    FeatureLevel = featureLevels.MaxSupportedFeatureLevel;

    if(FeatureLevel < minFeatureLevel)
    {
        throw Exception(MakeString(L"The device doesn't support the minimum feature level required to run this sample (FL%i.%i)",
                                   minFeatureLevel >> 12, (minFeatureLevel >> 8) & 0xF));
    }

    const D3D_SHADER_MODEL requiredShaderModel = D3D_SHADER_MODEL_6_6;
    const char* requiredShaderModelStr = "SM 6.6";

    // Check the required shader model
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModelFeature = { requiredShaderModel };
    DXCall(Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModelFeature, sizeof(shaderModelFeature)));
    if(shaderModelFeature.HighestShaderModel < requiredShaderModel)
        throw Exception(MakeString(L"The device does not support the minimum shader model required to run this sample (%s)", requiredShaderModelStr));

    // Check the required resource binding tier
    D3D12_FEATURE_DATA_D3D12_OPTIONS features = { };
    Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features));
    if(features.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3)
        throw Exception("The does not support the minimum resource binding tier required to run this sample (D3D12_RESOURCE_BINDING_TIER_3)");

    D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = { };
    Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12));
    if(options12.EnhancedBarriersSupported == false)
        throw Exception(L"The device does not support enhanced barriers, which is required to run this sample.");

    #if EnableDXR_
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = { };
        DXCall(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5)));
        if(opts5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1)
            throw Exception(L"The device does not support DXR 1.1, which is required to run this sample.");
    #endif

    #if EnableWorkGraphsPreview_
        D3D12_FEATURE_DATA_D3D12_OPTIONS_EXPERIMENTAL experimentalOpts = { };
        DXCall(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS_EXPERIMENTAL, &experimentalOpts, sizeof(experimentalOpts)));
        if(experimentalOpts.WorkGraphsTier == D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED)
            throw Exception(L"The device does not support work graphs");
    #endif

    #if UseDebugDevice_
        ID3D12InfoQueuePtr infoQueue;
        DXCall(Device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

        D3D12_MESSAGE_ID disabledMessages[] =
        {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,

            // These happen when capturing with VS diagnostics
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,

            #if EnableWorkGraphsPreview_
                D3D12_MESSAGE_ID_NON_RETAIL_SHADER_MODEL_WONT_VALIDATE,
            #endif
        };

        D3D12_INFO_QUEUE_FILTER filter = { };
        filter.DenyList.NumIDs = ArraySize_(disabledMessages);
        filter.DenyList.pIDList = disabledMessages;
        infoQueue->AddStorageFilterEntries(&filter);
    #endif

    #if BreakOnDXError_
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    #endif

    for(uint64 i = 0; i < NumCmdAllocators; ++i)
        DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAllocators[i])));

    DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CmdAllocators[0], nullptr, IID_PPV_ARGS(&CmdList)));
    DXCall(CmdList->Close());
    CmdList->SetName(L"Primary Graphics Command List");

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    DXCall(Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&GfxQueue)));
    GfxQueue->SetName(L"Main Gfx Queue");

    CurrFrameIdx = CurrentCPUFrame % NumCmdAllocators;
    DXCall(CmdAllocators[CurrFrameIdx]->Reset());
    DXCall(CmdList->Reset(CmdAllocators[CurrFrameIdx], nullptr));

    FrameFence.Init(0);

    for(uint64 i = 0; i < ArraySize_(DeferredSRVCreates); ++i)
        DeferredSRVCreates[i].Init(1024);

    Initialize_Helpers();
    Initialize_Upload();
}

void Shutdown()
{
    Assert_(CurrentCPUFrame == CurrentGPUFrame);
    ShuttingDown = true;

    for(uint64 i = 0; i < ArraySize_(DeferredReleases); ++i)
        ProcessDeferredReleases(i);

    FrameFence.Shutdown();

    for(uint64 i = 0; i < RenderLatency; ++i)
        Release(CmdAllocators[i]);

    Release(CmdList);
    Release(GfxQueue);
    Release(Factory);
    Release(Adapter);

    Shutdown_Helpers();
    Shutdown_Upload();

    #if BreakOnDXError_
        if(Device != nullptr)
        {
            ID3D12InfoQueuePtr infoQueue;
            DXCall(Device->QueryInterface(IID_PPV_ARGS(&infoQueue)));
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
        }
    #endif

    #if UseDebugDevice_ && 0
        if(Device != nullptr)
        {
            ID3D12DebugDevicePtr debugDevice;
            DXCall(Device->QueryInterface(IID_PPV_ARGS(&debugDevice)));
            debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
        }
    #endif

    Release(Device);
}

void BeginFrame()
{
    Assert_(Device);

    SetDescriptorHeaps(CmdList);
}

void EndFrame(IDXGISwapChain4* swapChain, uint32 syncIntervals)
{
    Assert_(Device);

    DXCall(CmdList->Close());

    EndFrame_Upload();

    ID3D12CommandList* commandLists[] = { CmdList };
    GfxQueue->ExecuteCommandLists(ArraySize_(commandLists), commandLists);

    // Present the frame.
    if(swapChain)
        DXCall(swapChain->Present(syncIntervals, syncIntervals == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0));

    ++CurrentCPUFrame;

    // Signal the fence with the current frame number, so that we can check back on it
    FrameFence.Signal(GfxQueue, CurrentCPUFrame);

    // Wait for the GPU to catch up before we stomp an executing command buffer
    const uint64 gpuLag = DX12::CurrentCPUFrame - DX12::CurrentGPUFrame;
    Assert_(gpuLag <= DX12::RenderLatency);
    if(gpuLag >= DX12::RenderLatency)
    {
        // Make sure that the previous frame is finished
        FrameFence.Wait(DX12::CurrentGPUFrame + 1);
        ++DX12::CurrentGPUFrame;
    }

    CurrFrameIdx = DX12::CurrentCPUFrame % NumCmdAllocators;

    // Prepare the command buffers to be used for the next frame
    DXCall(CmdAllocators[CurrFrameIdx]->Reset());
    DXCall(CmdList->Reset(CmdAllocators[CurrFrameIdx], nullptr));

    EndFrame_Helpers();

    // See if we have any deferred releases to process
    ProcessDeferredReleases(CurrFrameIdx);
    ProcessDeferredSRVCreates(CurrFrameIdx);
}

void FlushGPU()
{
    Assert_(Device);

    DX12::Flush_Upload();

    // Wait for the GPU to fully catch up with the CPU
    Assert_(CurrentCPUFrame >= CurrentGPUFrame);
    if(CurrentCPUFrame > CurrentGPUFrame)
    {
        FrameFence.Wait(CurrentCPUFrame);
        CurrentGPUFrame = CurrentCPUFrame;
    }

    // Process anything that was deferred
    for(uint64 i = 0; i < RenderLatency; ++i)
    {
        ProcessDeferredReleases(i);
        ProcessDeferredSRVCreates(i);
    }
}

void DeferredRelease_(IUnknown* resource)
{
    if(resource == nullptr)
        return;

    if(ShuttingDown || Device == nullptr)
    {
        // Free-for-all!
        resource->Release();
        return;
    }

    DeferredReleases[CurrFrameIdx].Add(resource);
}

void DeferredCreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, uint32 descriptorIdx)
{
    for(uint64 i = 1; i < RenderLatency; ++i)
    {
        uint64 frameIdx = (CurrentCPUFrame + i) % RenderLatency;
        uint64 writeIdx = InterlockedIncrement(&DeferredSRVCreateCount[frameIdx]) - 1;
        DeferredSRVCreate& create = DeferredSRVCreates[frameIdx][writeIdx];
        create.Resource = resource;
        create.Desc = desc;
        create.DescriptorIdx = descriptorIdx;
    }
}

ID3D12CommandAllocator* CurrentCmdAllocator()
{
    return CmdAllocators[CurrFrameIdx];
}

} // namespace DX12

} // namespace SampleFramework12

