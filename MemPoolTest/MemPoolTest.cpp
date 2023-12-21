//=================================================================================================
//
//  D3D12 Memory Pool Performance Test
//  by MJP
//  https://therealmjp.github.io/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#include <PCH.h>

#include <Window.h>
#include <Input.h>
#include <Utility.h>
#include <Graphics/SwapChain.h>
#include <Graphics/ShaderCompilation.h>
#include <Graphics/Profiler.h>
#include <Graphics/DX12.h>
#include <Graphics/DX12_Helpers.h>
#include <ImGui/ImGui.h>
#include <ImGuiHelper.h>
#include <EnkiTS/TaskScheduler_c.h>

#include "MemPoolTest.h"
#include "SharedTypes.h"
#include "AppSettings.h"

using namespace SampleFramework12;

static const uint32 NumBenchmarkWarmupFrames = 8;
static const uint32 NumBenchmarkMeasureFrames = 64;
static const uint32 NumBenchmarkTotalFrames = NumBenchmarkWarmupFrames + NumBenchmarkMeasureFrames;

static const uint32 MaxCBufferSize = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;

static bool32 GPUUploadHeapAvailable = false;

static bool IsInputBufferCPUWritable()
{
    if(AppSettings::HeapType == HeapTypes::Upload || AppSettings::HeapType == HeapTypes::GPUUpload)
        return true;

    if(AppSettings::HeapType == HeapTypes::Custom && AppSettings::CPUPageProperty != CPUPageProperties::NotAvailable)
        return true;

    return false;
}

static bool IsInputBufferCPUWritable(const BenchmarkConfig& config)
{
    if(config.HeapType == HeapTypes::Upload || config.HeapType == HeapTypes::GPUUpload)
        return true;

    if(config.HeapType == HeapTypes::Custom && config.CPUPageProperty != CPUPageProperties::NotAvailable)
        return true;

    return false;
}

static RawBuffer* backgroundUploadBufferPtr = nullptr;

static void BackgroundUploadTask(uint32 start, uint32 end, uint32 threadnum, void* args)
{
    ID3D12Resource* targetResource = backgroundUploadBufferPtr->Resource();
    while(backgroundUploadBufferPtr != nullptr)
    {
        const uint32 size = AppSettings::BackgroundUploadSize * 1024 * 1024;
        if(size > 0)
        {
            UploadContext uploadContext = DX12::ResourceUploadBegin(size);

            // memset(uploadContext.CPUAddress, 0, size);

            uploadContext.CmdList->CopyBufferRegion(targetResource, 0, uploadContext.Resource, uploadContext.ResourceOffset, size);

            // Don't have the main graphics queue sync on this upload since it's simulating a "background" streaming task
            const bool syncOnGraphicsQueue = false;
            DX12::ResourceUploadEnd(uploadContext, syncOnGraphicsQueue);

            const int32 waitTime = AppSettings::BackgroundUploadWaitTime;
            if(waitTime > 0)
            {
                Timer timer;
                while(timer.ElapsedMilliseconds() < waitTime)
                {
                    timer.Update();
                }
            }
        }
        else
        {
            Sleep(15);
        }
    }
}

MemPoolTest::MemPoolTest(const wchar* cmdLine) : App(L"DX12 Memory Pool Test", cmdLine)
{
    minFeatureLevel = D3D_FEATURE_LEVEL_12_0;
}

void MemPoolTest::BeforeReset()
{
}

void MemPoolTest::AfterReset()
{
}

void MemPoolTest::Initialize()
{
    // Check if GPU upload heaps are supported
    D3D12_FEATURE_DATA_D3D12_OPTIONS16 opts16 = { };
    DXCall(DX12::Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &opts16, sizeof(opts16)));
    GPUUploadHeapAvailable = opts16.GPUUploadHeapSupported;
    if(GPUUploadHeapAvailable == false)
        AppSettings::HeapType.ClampNumValues(uint32(HeapTypes::NumValues) - 1);

    ID3D12InfoQueue* infoQueue = nullptr;
    DX12::Device->QueryInterface(IID_PPV_ARGS(&infoQueue));
    if(infoQueue != nullptr)
    {
        // Disable performance warnings about creating write-combined heaps so that we can
        // see just how bad it gets
        D3D12_MESSAGE_ID disabledMessages[] = { D3D12_MESSAGE_ID_WRITE_COMBINE_PERFORMANCE_WARNING };

        D3D12_INFO_QUEUE_FILTER filter = { };
        filter.DenyList.NumIDs = ArraySize_(disabledMessages);
        filter.DenyList.pIDList = disabledMessages;
        infoQueue->AddStorageFilterEntries(&filter);

        DX12::Release(infoQueue);
    }

    CreateBuffers();
    CompileComputeJob();

    Profiler::GlobalProfiler.SetAlwaysEnableGPUProfiling(true);

    DXCall(DX12::Device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &architectureData, sizeof(architectureData)));
    if(architectureData.UMA)
        AppSettings::MemoryPool.ClampNumValues(1);

    // Initialize this buffer with dummy data to ensure the upload ring buffer resizes to fit our max upload size
    Array<uint8> initData(uint64(AppSettings::BackgroundUploadSize.MaxValue()) * 1024 * 1024, 0);
    backgroundUploadBuffer.Initialize({
        .NumElements = uint64(AppSettings::BackgroundUploadSize.MaxValue()) * 1024 * 1024 / 4,
        .InitData = initData.Data(),
        .Name = L"Background Upload Buffer",
    });
    backgroundUploadBufferPtr = &backgroundUploadBuffer;

    taskScheduler = enkiNewTaskScheduler();
    enkiInitTaskSchedulerNumThreads(taskScheduler, 2);

    taskSet = enkiCreateTaskSet(taskScheduler, BackgroundUploadTask);
    enkiAddTaskSet(taskScheduler, taskSet);

    InitBenchmark();
}

void MemPoolTest::Shutdown()
{
    backgroundUploadBufferPtr = nullptr;
    enkiWaitForTaskSet(taskScheduler, taskSet);
    enkiDeleteTaskSet(taskScheduler, taskSet);
    taskSet = nullptr;
    enkiDeleteTaskScheduler(taskScheduler);
    taskScheduler = nullptr;
    backgroundUploadBuffer.Shutdown();

    inputBuffer.Shutdown();
    uploadBuffer.Shutdown();
    outputBuffer.Shutdown();
    DX12::Release(inputBufferHeap);
    DX12::SRVDescriptorHeap.FreePersistent(inputBufferSRV);
}

void MemPoolTest::CreatePSOs()
{
    {
        // Compute job PSO
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.CS = computeJobCS.ByteCode();
        psoDesc.pRootSignature = DX12::UniversalRootSignature;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        DXCall(DX12::Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computeJobPSO)));
    }
}

void MemPoolTest::DestroyPSOs()
{
    DX12::DeferredRelease(computeJobPSO);
}

void MemPoolTest::Update(const Timer& timer)
{
    CPUProfileBlock cpuProfileBlock("Update");

    // Toggle VSYNC
    swapChain.SetVSYNCEnabled(AppSettings::EnableVSync ? true : false);

    TickBenchmark();

    // Toggle stable power state
    if(AppSettings::StablePowerState != stablePowerState)
    {
        DX12::Device->SetStablePowerState(AppSettings::StablePowerState);
        stablePowerState = AppSettings::StablePowerState;
    }

    if(AppSettings::EnableDriverBackgroundThreads != driverThreads)
    {
        driverThreads = AppSettings::EnableDriverBackgroundThreads;
        DX12::Device->SetBackgroundProcessingMode(driverThreads ? D3D12_BACKGROUND_PROCESSING_MODE_ALLOWED : D3D12_BACKGROUND_PROCESSING_MODE_DISABLE_PROFILING_BY_SYSTEM,
                                                  D3D12_MEASUREMENTS_ACTION_DISCARD_PREVIOUS, NULL, nullptr);

    }

    const bool customHeap = AppSettings::HeapType == HeapTypes::Custom;
    AppSettings::CPUPageProperty.SetVisible(customHeap);
    AppSettings::MemoryPool.SetVisible(customHeap);

    if(architectureData.UMA == false && AppSettings::MemoryPool == MemoryPools::L1)
        AppSettings::CPUPageProperty.ClampNumValues(uint32(CPUPageProperties::NotAvailable) + 1);
    else
        AppSettings::CPUPageProperty.ClampNumValues(uint32(CPUPageProperties::NumValues));

    // Rebuild buffers
    const Setting* rebuildBufferSettings[] =
    {
        &AppSettings::InputBufferSizeMB,
        &AppSettings::InputBufferSizeKB,
        &AppSettings::InputBufferSizeBytes,
        &AppSettings::NumThreadGroups,
        &AppSettings::HeapType,
        &AppSettings::CPUPageProperty,
        &AppSettings::MemoryPool,
        &AppSettings::InputBufferType,
    };

    for(const Setting* setting : rebuildBufferSettings)
    {
        if(setting->Changed())
        {
            CreateBuffers();
            break;
        }
    }

    const Setting* recompileSettings[] =
    {
        &AppSettings::InputBufferSizeMB,
        &AppSettings::InputBufferSizeKB,
        &AppSettings::InputBufferSizeBytes,
        &AppSettings::ElemsPerThread,
        &AppSettings::ThreadElemStride,
        &AppSettings::GroupElemOffset,
        &AppSettings::ThreadElemOffset,
        &AppSettings::InputBufferType,
    };

    for(const Setting* setting : recompileSettings)
    {
        if(setting->Changed())
        {
            CompileComputeJob();
            DestroyPSOs();
            CreatePSOs();
            break;
        }
    }

    if(AppSettings::BufferUploadPath.Changed())
    {
        // We can't have two different queues writing to
        // the same buffer, so make sure that we flush the GPU before
        // switching our uploading from one queue to the other
        DX12::FlushGPU();
    }
}

void MemPoolTest::Render(const Timer& timer)
{
    ID3D12GraphicsCommandList5* cmdList = DX12::CmdList;

    CPUProfileBlock cpuProfileBlock("Render");
    ProfileBlock gpuProfileBlock(cmdList, "Render Total");

    UpdateBuffer();
    RunCompute();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
    cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

    float clearColor[4] = { 0.2f, 0.4f, 0.8f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);

    DX12::SetViewport(cmdList, swapChain.Width(), swapChain.Height());

    RenderHUD(timer);
}

void MemPoolTest::CreateBuffers()
{
    inputBuffer.Shutdown();
    uploadBuffer.Shutdown();
    outputBuffer.Shutdown();

    DX12::FlushGPU();

    DX12::Release(inputBufferHeap);

    DX12::SRVDescriptorHeap.FreePersistent(inputBufferSRV);

    const uint32 inputBufferAlignment = (AppSettings::InputBufferType == BufferTypes::Constant) ? DX12::ConstantBufferAlignment : 1;
    const uint32 maxInputBufferSize = (AppSettings::InputBufferType == BufferTypes::Constant) ? MaxCBufferSize : (1 * 1024 * 1024 * 1024);
    const uint32 inputBufferSize = AlignTo(Clamp<uint32>(AppSettings::InputBufferSizeMB * (1024 * 1024) + AppSettings::InputBufferSizeKB * 1024 + AppSettings::InputBufferSizeBytes, 1u, maxInputBufferSize), inputBufferAlignment);
    const uint32 numInputElems = inputBufferSize / 16;
    const uint32 totalInputBufferSize = AlignTo(inputBufferSize * uint32(DX12::RenderLatency), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

    const uint32 numTotalThreads = AppSettings::ThreadGroupSize * AppSettings::NumThreadGroups;
    numComputeJobThreads = numTotalThreads;

    static const D3D12_CPU_PAGE_PROPERTY cpuPageProps[] =
    {
        D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE,
        D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE,
        D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
    };
    StaticAssert_(ArraySize_(cpuPageProps) == uint32(CPUPageProperties::NumValues));

    static const D3D12_MEMORY_POOL memPools[] =
    {
        D3D12_MEMORY_POOL_L0,
        D3D12_MEMORY_POOL_L1,
    };
    StaticAssert_(ArraySize_(memPools) == uint32(MemoryPools::NumValues));

    {
        D3D12_HEAP_PROPERTIES heapProps = { };

        if(AppSettings::HeapType == HeapTypes::Upload)
        {
            heapProps = *DX12::GetUploadHeapProps();
            customHeapProps = DX12::Device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
        }
        else if(AppSettings::HeapType == HeapTypes::Default)
        {
            heapProps = *DX12::GetDefaultHeapProps();
            customHeapProps = DX12::Device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_DEFAULT);
        }
        else if(AppSettings::HeapType == HeapTypes::Custom)
        {
            customHeapProps =
            {
                .Type = D3D12_HEAP_TYPE_CUSTOM,
                .CPUPageProperty = cpuPageProps[uint32(AppSettings::CPUPageProperty)],
                .MemoryPoolPreference = memPools[uint32(AppSettings::MemoryPool)],
                .CreationNodeMask = 1,
                .VisibleNodeMask = 1,
            };

            heapProps = customHeapProps;
        }
        else if(AppSettings::HeapType == HeapTypes::GPUUpload)
        {
            heapProps = *DX12::GetGPUUploadHeapProps();
            customHeapProps = DX12::Device->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_GPU_UPLOAD);
        }
        else
            AssertMsg_(false, "Unhandled heap type");

        D3D12_HEAP_DESC heapDesc =
        {
            .SizeInBytes = totalInputBufferSize,
            .Properties = heapProps,
            .Alignment = 0,
            .Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
        };

        DX12::Device->CreateHeap(&heapDesc, IID_PPV_ARGS(&inputBufferHeap));
    }

    const bool cpuWritable = IsInputBufferCPUWritable();

    inputBuffer.Initialize({
        .NumElements = numInputElems * 4,
        .Dynamic = true,
        .CPUAccessible = cpuWritable,
        .Heap = inputBufferHeap,
        .HeapOffset = 0,
        .Name = L"Input Buffer",
    });

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    inputBufferSRV = srvAlloc.Index;

    if(AppSettings::InputBufferType == BufferTypes::Raw)
    {
        for(uint32 i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = cpuWritable ? uint32(inputBuffer.NumElements) * i : 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            srvDesc.Buffer.NumElements = uint32(inputBuffer.NumElements);
            DX12::Device->CreateShaderResourceView(inputBuffer.Resource(), &srvDesc, srvAlloc.Handles[i]);
        }
    }
    else if(AppSettings::InputBufferType == BufferTypes::Structured)
    {
        for(uint32 i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = cpuWritable ? numInputElems * i : 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.NumElements = numInputElems;
            srvDesc.Buffer.StructureByteStride = 16;
            DX12::Device->CreateShaderResourceView(inputBuffer.Resource(), &srvDesc, srvAlloc.Handles[i]);
        }
    }
    else if(AppSettings::InputBufferType == BufferTypes::Formatted)
    {
        for(uint32 i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = cpuWritable ? numInputElems * i : 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.NumElements = numInputElems;
            DX12::Device->CreateShaderResourceView(inputBuffer.Resource(), &srvDesc, srvAlloc.Handles[i]);
        }
    }

    AppSettings::InputBufferIdx.SetValue(inputBufferSRV);
    AppSettings::OutputBufferIdx.SetValue(outputBuffer.UAV);
    AppSettings::NumInputBufferElems.SetValue(numInputElems);

    if(cpuWritable == false)
    {
        uploadBuffer.Initialize({
            .Size = inputBuffer.InternalBuffer.Size,
            .Alignment = 4,
            .Dynamic = true,
            .CPUAccessible = true,
            .Name = L"Upload Buffer"
        });
    }

    outputBuffer.Initialize({
        .NumElements = numTotalThreads,
        .CreateUAV = true,        
        .Name = L"Output Buffer",
    });

    inputBufferShadowMem.Init(numInputElems, Float4(1.0f, 1.0f, 1.0f, 1.0f));
    readbackMem.Init(numInputElems, Float4());
}

void MemPoolTest::CompileComputeJob()
{
    CompileOptions opts;
    opts.Add("ElemsPerThread_", AppSettings::ElemsPerThread);
    opts.Add("ThreadElemOffset_", AppSettings::ThreadElemOffset);
    opts.Add("GroupElemOffset_", AppSettings::GroupElemOffset);
    opts.Add("NumInputBufferElems_", AppSettings::NumInputBufferElems);
    opts.Add("ThreadElemStride_", AppSettings::ThreadElemStride);
    opts.Add("RawBuffer_", AppSettings::InputBufferType == BufferTypes::Raw);
    opts.Add("FormattedBuffer_", AppSettings::InputBufferType == BufferTypes::Formatted);
    opts.Add("StructuredBuffer_", AppSettings::InputBufferType == BufferTypes::Structured);
    opts.Add("ConstantBuffer_", AppSettings::InputBufferType == BufferTypes::Constant);
    computeJobCS = CompileFromFile(L"ComputeJob.hlsl", "ComputeJob", ShaderType::Compute, opts);
}

void MemPoolTest::InitBenchmark()
{
    benchmarkSamples.Init(NumBenchmarkMeasureFrames);

    uint32 threadGroupCounts[] =
    {
        // 1,
        // 16,
        // 1024,
        // 16 * 1024*

        // 4 * 1024,

        // 16 * 1024,

        32 * 1024,
    };
    uint32 inputBufferSizes[] =
    {
        // 1 * 1024,
        // 64 * 1024,
        // 1 * 1024 * 1024,
        // 16 * 1024 * 1024,

        // 64 * 1024 * 1024,

        32 * 1024 * 1024,
        64 * 1024 * 1024,
        96 * 1024 * 1024,
        128 * 1024 * 1024,

        /*1 * 1024 * 1024,
        2 * 1024 * 1024,
        3 * 1024 * 1024,
        4 * 1024 * 1024,
        5 * 1024 * 1024,
        6 * 1024 * 1024,
        7 * 1024 * 1024,
        8 * 1024 * 1024,
        9 * 1024 * 1024,
        10 * 1024 * 1024,
        11 * 1024 * 1024,
        12 * 1024 * 1024,
        13 * 1024 * 1024,
        14 * 1024 * 1024,
        15 * 1024 * 1024,
        16 * 1024 * 1024,*/
    };
    uint32 elemsPerThreadCounts[] =
    {
        1,
        // 4,
        // 16,
    };
    uint32 threadElemStrides[] =
    {
        1,
        // 4,
        // 16,
    };
    uint32 groupElemOffsets[] =
    {
        // 0,
        1,
        // 4,
    };
    uint32 threadElemOffsets[] =
    {
        // 0,
        1,
        // 4,
    };

    for(HeapTypes heapType : HeapTypesValues)
    {
        // Can't use this heap type unless it's supported on the device
        if(heapType == HeapTypes::GPUUpload && GPUUploadHeapAvailable == false)
            continue;

        if(heapType == HeapTypes::Custom)
            continue;

        for(CPUPageProperties cpuPageProperty : CPUPagePropertiesValues)
        {
            // We don't need to modulate this for built-in heap types
            if(heapType != HeapTypes::Custom && cpuPageProperty != CPUPageProperties::NotAvailable)
                continue;

            for(MemoryPools memoryPool : MemoryPoolsValues)
            {
                // We don't need to modulate this for built-in heap types
                if(heapType != HeapTypes::Custom && memoryPool != MemoryPools::L0)
                    continue;

                // This is invalid for UMA which only has a single memory pool
                if(architectureData.UMA && memoryPool == MemoryPools::L1)
                    continue;

                // Can't enable cached CPU pages for NUMA
                if(architectureData.UMA == false && memoryPool == MemoryPools::L1 && cpuPageProperty == CPUPageProperties::WriteBack)
                    continue;

                // for(BufferTypes bufferType : BufferTypesValues)
                {
                    const BufferTypes bufferType = BufferTypes::Raw;

                    for(uint32 numThreadGroups : threadGroupCounts)
                    {
                        for(uint32 inputBufferSize : inputBufferSizes)
                        {
                            // Constant buffers have a max size that we need to respect
                            if(bufferType == BufferTypes::Constant && inputBufferSize > MaxCBufferSize)
                                continue;

                            for(uint32 elemsPerThread : elemsPerThreadCounts)
                            {
                                for(uint32 threadElemStride : threadElemStrides)
                                {
                                    for(uint32 groupElemOffset : groupElemOffsets)
                                    {
                                        for(uint32 threadElemOffset : threadElemOffsets)
                                        {
                                            BenchmarkConfig& config = benchmarkConfigs.Add();
                                            config.HeapType = heapType;
                                            config.CPUPageProperty = cpuPageProperty;
                                            config.MemoryPool = memoryPool;
                                            config.InputBufferType = bufferType;
                                            config.NumThreadGroups = numThreadGroups;
                                            config.InputBufferSize = inputBufferSize;
                                            config.ElemsPerThread = elemsPerThread;
                                            config.ThreadElemStride = threadElemStride;
                                            config.GroupElemOffset = groupElemOffset;
                                            config.ThreadElemOffset = threadElemOffset;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    numBenchmarks = uint32(benchmarkConfigs.Count());
    benchmarkResults.Init(numBenchmarks);
}

void MemPoolTest::UpdateBuffer()
{
    {
        CPUProfileBlock cpuProfileBlock("Update Buffer");

        if(IsInputBufferCPUWritable())
        {
            inputBuffer.MapAndSetData(inputBufferShadowMem.Data(), inputBuffer.NumElements);
        }
        else
        {
            if(AppSettings::BufferUploadPath == BufferUploadPaths::FastUploadCopyQueue)
            {
                // Queue an async upload with the dedicated "fast" COPY queue
                MapResult mapResult = uploadBuffer.MapAndSetData(inputBufferShadowMem.Data(), uploadBuffer.Size);
                inputBuffer.QueueUpload(mapResult.Resource, mapResult.ResourceOffset, inputBuffer.NumElements, 0);
            }
            else if(AppSettings::BufferUploadPath == BufferUploadPaths::UploadCopyQueue)
            {
                // Use the standard resource uploader path
                const uint64 size = uploadBuffer.Size;
                UploadContext uploadContext = DX12::ResourceUploadBegin(size);

                memcpy(uploadContext.CPUAddress, inputBufferShadowMem.Data(), size);

                const uint64 dstOffset = inputBuffer.CycleBuffer();
                uploadContext.CmdList->CopyBufferRegion(inputBuffer.Resource(), dstOffset, uploadContext.Resource, uploadContext.ResourceOffset, size);

                DX12::ResourceUploadEnd(uploadContext);
            }
            else
            {
                // Synchronously upload using the main DIRECT quueue
                ID3D12GraphicsCommandList7* cmdList = DX12::CmdList;

                ProfileBlock gpuProfileBlock(cmdList, "Upload Buffer");
                PIXMarker pixMarker(cmdList, "Upload Buffer");

                MapResult mapResult = uploadBuffer.MapAndSetData(inputBufferShadowMem.Data(), uploadBuffer.Size);

                const uint64 dstOffset = inputBuffer.CycleBuffer();
                cmdList->CopyBufferRegion(inputBuffer.Resource(), dstOffset, mapResult.Resource, mapResult.ResourceOffset, uploadBuffer.Size);

                DX12::Barrier(cmdList, inputBuffer.InternalBuffer.WriteToReadBarrier({ .SyncBefore = D3D12_BARRIER_SYNC_COPY, .AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST }));
            }
        }
    }

    if(IsInputBufferCPUWritable() && AppSettings::ReadFromGPUMem)
    {
        CPUProfileBlock cpuProfileBlock("Read From Buffer");
        memcpy(readbackMem.Data(), inputBuffer.InternalBuffer.CPUAddress, readbackMem.Size());
    }
}

void MemPoolTest::RunCompute()
{
    ID3D12GraphicsCommandList7* cmdList = DX12::CmdList;

    PIXMarker pixMarker(cmdList, "Compute Job");
    ProfileBlock profileBlock(cmdList, "Compute Job");

    cmdList->SetPipelineState(computeJobPSO);
    cmdList->SetComputeRootSignature(DX12::UniversalRootSignature);

    if(AppSettings::InputBufferType == BufferTypes::Constant)
        cmdList->SetComputeRootConstantBufferView(URS_ConstantBuffers + 0, inputBuffer.GPUAddress);

    AppSettings::BindCBufferCompute(cmdList, URS_AppSettings);

    cmdList->Dispatch(AppSettings::NumThreadGroups, 1, 1);

    DX12::Barrier(cmdList, outputBuffer.InternalBuffer.WriteToWriteBarrier());
}

static double ToMB(uint64 numBytes)
{
    return numBytes / (1024.0 * 1024.0);
}

void MemPoolTest::RenderHUD(const Timer& timer)
{
    float width = float(swapChain.Width());
    float height = float(swapChain.Height());

    const float windowPercentage = 0.55f;
    Float2 windowSize = Float2(width, height) * windowPercentage;
    Float2 windowPos = Float2(width, height) * (1.0f - windowPercentage) * 0.5f;
    Float2 windowEnd = windowPos + windowSize;

    ImGui::SetNextWindowPos(ToImVec2(windowPos), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ToImVec2(windowSize), ImGuiCond_FirstUseEver);
    ImGui::Begin("Test Info", nullptr);

    if(copyTestInfoToClipboard)
        ImGui::LogToClipboard();

    DXGI_ADAPTER_DESC1 adapterDesc = { };
    DX12::Adapter->GetDesc1(&adapterDesc);
    ImGui::Text("Adapter: %ls", adapterDesc.Description);
    ImGui::Text("UMA: %s", architectureData.UMA ? "Yes" : "No");
    ImGui::Text("Cache-Coherent UMA: %s", architectureData.CacheCoherentUMA ? "Yes" : "No");

    IDXGIAdapter3* adapter = nullptr;
    DX12::Adapter->QueryInterface(IID_PPV_ARGS(&adapter));

    DXGI_QUERY_VIDEO_MEMORY_INFO localMemInfo = { };
    DXGI_QUERY_VIDEO_MEMORY_INFO nonLocalMemInfo = { };
    adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localMemInfo);
    adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocalMemInfo);

    ImGui::Text("Local Memory Usage: %.2f / %.2f MB", ToMB(localMemInfo.CurrentUsage), ToMB(localMemInfo.Budget));
    ImGui::Text("Non-Local Memory Usage: %.2f / %.2f MB", ToMB(nonLocalMemInfo.CurrentUsage), ToMB(nonLocalMemInfo.Budget));

    DX12::Release(adapter);

    ImGui::Separator();

    ImGui::Text("Heap Type: %s", HeapTypesLabels[uint32(AppSettings::HeapType)]);
    ImGui::Text("CPU-Writable Heap: %s", IsInputBufferCPUWritable() ? "Yes" : "No");

    static const char* pageProps [] =
    {
        "UNKNOWN",
        "NOT_AVAILABLE",
        "WRITE_COMBINE",
        "WRITE_BACK",
    };

    static const char* memPools[] =
    {
        "UNKNOWN",
        "L0",
        "L1",
    };

    ImGui::Text("Heap CPUPageProperty: %s", pageProps[customHeapProps.CPUPageProperty]);
    ImGui::Text("Heap MemoryPoolPreference: %s", memPools[customHeapProps.MemoryPoolPreference]);

    ImGui::Separator();

    const uint64 inputBufferSize = inputBuffer.NumElements * 4ull;
    if(inputBufferSize >= (1024 * 1024))
        ImGui::Text("Input Buffer Size: %.2f MB", inputBufferSize / (1024.0 * 1024.0f));
    else if(inputBufferSize >= 1024)
        ImGui::Text("Input Buffer Size: %.2f KB", inputBufferSize / 1024.0);
    else
        ImGui::Text("Input Buffer Size: %llu B", inputBufferSize);

    const uint64 bufferBytesRead = uint64(AppSettings::ElemsPerThread) * 16ull * numComputeJobThreads;
    if(bufferBytesRead >= (1024 * 1024))
        ImGui::Text("Total Bytes Read: %.2f MB", bufferBytesRead / (1024.0 * 1024.0f));
    else if(bufferBytesRead >= 1024)
        ImGui::Text("Total Bytes Read: %.2f KB", bufferBytesRead / 1024.0);
    else
        ImGui::Text("Total Bytes Read: %llu B", bufferBytesRead);

    ImGui::Text("Total Num Threads: %u", numComputeJobThreads);

    ImGui::Separator();

    const double computeJobTime = Profiler::GlobalProfiler.GPUProfileTimingAvg("Compute Job");
    const double maxEffectiveBandwidth = (bufferBytesRead / (1024.0 * 1024.0)) / (computeJobTime / 1000.0);

    ImGui::Text("Total Frame Time: %.2f ms", avgFrameTime * 1000.0);
    ImGui::Text("GPU Time Reading Buffer: %.2f ms", computeJobTime);
    ImGui::Text("CPU Time Updating Buffer: %.2f ms", Profiler::GlobalProfiler.CPUProfileTimingAvg("Update Buffer"));
    if(IsInputBufferCPUWritable() && AppSettings::ReadFromGPUMem)
        ImGui::Text("CPU Time Reading Buffer: %.2f ms", Profiler::GlobalProfiler.CPUProfileTimingAvg("Read From Buffer"));
    ImGui::Text("Max Effective Bandwidth: %.2f MB/s", maxEffectiveBandwidth);

    if(copyTestInfoToClipboard)
        ImGui::LogFinish();

    ImGui::Separator();

    if(benchmarkConfigIdx >= numBenchmarks)
    {
        copyTestInfoToClipboard = ImGui::Button("Copy To Clipboard");
        if(ImGui::Button("Run Benchmark"))
        {
            benchmarkConfigIdx = 0;
            benchmarkFrameIdx = NumBenchmarkTotalFrames;
        }

        ImGui::InputText("Benchmark CSV Name", benchmarkCSVName, ArraySize_(benchmarkCSVName));
    }
    else
    {
        ImGui::Text("Running benchmark %u of %u", benchmarkConfigIdx, numBenchmarks);
    }

    ImGui::End();
}

void MemPoolTest::TickBenchmark()
{
    if(benchmarkConfigIdx >= numBenchmarks)
        return;

    if(benchmarkFrameIdx >= NumBenchmarkTotalFrames)
    {
        // Apply the settings for this benchmark run
        const BenchmarkConfig& config = benchmarkConfigs[benchmarkConfigIdx];
        AppSettings::HeapType.SetValue(config.HeapType);
        AppSettings::CPUPageProperty.SetValue(config.CPUPageProperty);
        AppSettings::MemoryPool.SetValue(config.MemoryPool);
        AppSettings::InputBufferType.SetValue(config.InputBufferType);
        AppSettings::NumThreadGroups.SetValue(config.NumThreadGroups);
        AppSettings::ElemsPerThread.SetValue(config.ElemsPerThread);
        AppSettings::ThreadElemStride.SetValue(config.ThreadElemStride);
        AppSettings::GroupElemOffset.SetValue(config.GroupElemOffset);
        AppSettings::ThreadElemOffset.SetValue(config.ThreadElemOffset);

        const uint32 inputBufferBytes = uint32(config.InputBufferSize % 1024);
        const uint32 inputBufferKB = uint32(config.InputBufferSize % (1024 * 1024)) / 1024;
        const uint32 inputBufferMB = uint32(config.InputBufferSize / (1024 * 1024));

        AppSettings::InputBufferSizeMB.SetValue(inputBufferMB);
        AppSettings::InputBufferSizeKB.SetValue(inputBufferKB);
        AppSettings::InputBufferSizeBytes.SetValue(inputBufferBytes);

        // AppSettings::ReadFromGPUMem.SetValue(false);
        AppSettings::EnableVSync.SetValue(false);

        benchmarkFrameIdx = 0;
        return;
    }

    if(benchmarkFrameIdx >= NumBenchmarkWarmupFrames)
    {
        const uint32 sampleIdx = benchmarkFrameIdx - NumBenchmarkWarmupFrames;
        BenchmarkResults& sample = benchmarkSamples[sampleIdx];
        sample.ComputeJobTime = Profiler::GlobalProfiler.GPUProfileTiming("Compute Job");
        sample.CPUTimeUpdatingBuffer = Profiler::GlobalProfiler.CPUProfileTiming("Update Buffer");
        if(IsInputBufferCPUWritable() && AppSettings::ReadFromGPUMem)
            sample.CPUTimeReadingBuffer = Profiler::GlobalProfiler.CPUProfileTiming("Read From Buffer");
        else
            sample.CPUTimeReadingBuffer = 0;
    }

    benchmarkFrameIdx += 1;
    if(benchmarkFrameIdx == NumBenchmarkTotalFrames)
    {
        BenchmarkResults& results = benchmarkResults[benchmarkConfigIdx];
        results = BenchmarkResults();

        for(uint32 sampleIdx = 0; sampleIdx < NumBenchmarkMeasureFrames; ++sampleIdx)
        {
            results.ComputeJobTime += benchmarkSamples[sampleIdx].ComputeJobTime;
            results.CPUTimeUpdatingBuffer += benchmarkSamples[sampleIdx].CPUTimeUpdatingBuffer;
            results.CPUTimeReadingBuffer += benchmarkSamples[sampleIdx].CPUTimeReadingBuffer;
        }

        results.ComputeJobTime /= NumBenchmarkMeasureFrames;
        results.CPUTimeUpdatingBuffer /= NumBenchmarkMeasureFrames;
        results.CPUTimeReadingBuffer /= NumBenchmarkMeasureFrames;

        benchmarkConfigIdx += 1;
    }

    if(benchmarkConfigIdx == numBenchmarks)
    {
        std::string csv = "HeapType, CPUPageProperty, MemoryPool, InputBufferType, NumThreadGroups, InputBufferSize, ElemsPerThread, ThreadElemStride, GroupElemOffset, ThreadElemOffset, ";
        csv += "Total Num Threads, CPU-Writable Heap, Total Bytes Read, Unique Bytes Read, ";
        csv += "Compute Job Time (ms), CPU Time Updating Buffer (ms), CPU Time Reading Buffer (ms), Max Effective Bandwidth (MB) \n";

        for(uint32 benchmarkIdx = 0; benchmarkIdx < numBenchmarks; ++benchmarkIdx)
        {
            const BenchmarkConfig& config = benchmarkConfigs[benchmarkIdx];
            const BenchmarkResults& results = benchmarkResults[benchmarkIdx];

            const uint32 numTotalThreads = AppSettings::ThreadGroupSize * config.NumThreadGroups;
            const uint64 bufferBytesRead = uint64(config.ElemsPerThread) * 16ull * numTotalThreads;
            const uint64 uniqueBytesPerGroup = config.ElemsPerThread * 16ull * (config.ThreadElemOffset > 0 ? AppSettings::ThreadGroupSize : 1);
            const uint64 uniqueBytesRead = Min(uniqueBytesPerGroup * (config.GroupElemOffset > 0 ? config.NumThreadGroups : 1), config.InputBufferSize);

            const double maxEffectiveBandwidth = (bufferBytesRead / (1024.0 * 1024.0)) / (results.ComputeJobTime / 1000.0);

            csv += MakeString("%s, ", HeapTypesLabels[uint32(config.HeapType)]);
            csv += MakeString("%s, ", CPUPagePropertiesLabels[uint32(config.CPUPageProperty)]);
            csv += MakeString("%s, ", MemoryPoolsLabels[uint32(config.MemoryPool)]);
            csv += MakeString("%s, ", BufferTypesLabels[uint32(config.InputBufferType)]);
            csv += MakeString("%u, ", config.NumThreadGroups);
            csv += MakeString("%llu, ", config.InputBufferSize);
            csv += MakeString("%u, ", config.ElemsPerThread);
            csv += MakeString("%u, ", config.ThreadElemStride);
            csv += MakeString("%u, ", config.GroupElemOffset);
            csv += MakeString("%u, ", config.ThreadElemOffset);

            csv += MakeString("%u, ", numTotalThreads);
            csv += IsInputBufferCPUWritable(config) ? "Yes, " : "No, ";
            csv += MakeString("%llu, ", bufferBytesRead);
            csv += MakeString("%llu, ", uniqueBytesRead);

            csv += MakeString("%f, ", results.ComputeJobTime);
            csv += MakeString("%f, ", results.CPUTimeUpdatingBuffer);
            csv += MakeString("%f, ", results.CPUTimeReadingBuffer);
            csv += MakeString("%f, ", maxEffectiveBandwidth);

            csv += "\n";
        }

        WriteStringAsFile(AnsiToWString(benchmarkCSVName).c_str(), csv);
    }
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    MemPoolTest app(lpCmdLine);
    app.Run();
}
