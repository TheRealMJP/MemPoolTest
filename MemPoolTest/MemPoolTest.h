//=================================================================================================
//
//  D3D12 Memory Pool Performance Test
//  by MJP
//  https://therealmjp.github.io/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#pragma once

#include <PCH.h>

#include <App.h>
#include <Graphics/GraphicsTypes.h>
#include "AppSettings.h"

struct enkiTaskScheduler;
struct enkiTaskSet;

using namespace SampleFramework12;

struct BenchmarkConfig
{
    HeapTypes HeapType = HeapTypes::Upload;
    CPUPageProperties CPUPageProperty = CPUPageProperties::NotAvailable;
    MemoryPools MemoryPool = MemoryPools::L0;
    BufferTypes InputBufferType = BufferTypes::Raw;
    uint32 NumThreadGroups = 0;
    uint64 InputBufferSize = 0;
    uint32 ElemsPerThread = 0;
    uint32 ThreadElemStride = 0;
    uint32 GroupElemOffset = 0;
    uint32 ThreadElemOffset = 0;
};

struct BenchmarkResults
{
    double ComputeJobTime = 0.0;
    double CPUTimeUpdatingBuffer = 0.0;
    double CPUTimeReadingBuffer = 0.0;
};

class MemPoolTest : public App
{

protected:

    D3D12_FEATURE_DATA_ARCHITECTURE architectureData = { };

    CompiledShaderPtr computeJobCS;
    ID3D12PipelineState* computeJobPSO = nullptr;

    ID3D12Heap* inputBufferHeap = nullptr;
    D3D12_HEAP_PROPERTIES customHeapProps = { };
    RawBuffer inputBuffer;
    DescriptorIndex inputBufferSRV;
    Buffer uploadBuffer;
    RawBuffer outputBuffer;
    Array<Float4> inputBufferShadowMem;
    Array<Float4> readbackMem;
    uint32 numComputeJobThreads = 0;

    RawBuffer backgroundUploadBuffer;
    enkiTaskScheduler* taskScheduler = nullptr;
    enkiTaskSet* taskSet = nullptr;

    bool32 stablePowerState = false;
    bool32 driverThreads = false;

    bool32 copyTestInfoToClipboard = false;

    uint32 benchmarkConfigIdx = uint32(-1);
    uint32 numBenchmarks = 0;
    uint32 benchmarkFrameIdx = 0;

    List<BenchmarkConfig> benchmarkConfigs;
    Array<BenchmarkResults> benchmarkSamples;
    Array<BenchmarkResults> benchmarkResults;
    char benchmarkCSVName[256] = "Benchmark.csv";

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void Render(const Timer& timer) override;
    virtual void Update(const Timer& timer) override;

    virtual void BeforeReset() override;
    virtual void AfterReset() override;

    virtual void CreatePSOs() override;
    virtual void DestroyPSOs() override;

    void CreateBuffers();
    void CompileComputeJob();
    void InitBenchmark();
    void UpdateBuffer();
    void RunCompute();
    void RenderHUD(const Timer& timer);
    void TickBenchmark();

public:

    MemPoolTest(const wchar* cmdLine);
};
