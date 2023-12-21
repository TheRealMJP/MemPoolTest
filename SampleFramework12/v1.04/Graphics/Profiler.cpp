//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"
#include "Profiler.h"
#include "DX12.h"
#include "..\\Utility.h"
#include "..\\ImGui\ImGui.h"

using std::wstring;
using std::map;

namespace SampleFramework12
{

// == Profiler ====================================================================================

Profiler Profiler::GlobalProfiler;

static const uint64 MaxProfiles = 64;

struct ProfileData
{
    const char* Name = nullptr;

    bool QueryStarted = false;
    bool QueryFinished = false ;
    bool Active = false;

    bool CPUProfile = false;
    int64 StartTime = 0;
    int64 EndTime = 0;

    static const uint64 FilterSize = 64;
    double TimeSamples[FilterSize] = { };
    uint64 CurrSample = 0;

    double AverageTime = 0.0;
    double MaxTime = 0.0;
};

void Profiler::Initialize()
{
    Shutdown();

    enableGPUProfiling = true;

    D3D12_QUERY_HEAP_DESC heapDesc = { };
    heapDesc.Count = MaxProfiles * 2;
    heapDesc.NodeMask = 0;
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    DX12::Device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&queryHeap));

    for(uint32 i = 0; i < DX12::RenderLatency; ++i)
    {
        readbackBuffers[i].Initialize(MaxProfiles * 2 * sizeof(uint64));
        readbackBuffers[i].Resource->SetName(MakeString(L"Query Readback Buffer %u", i).c_str());
    }

    profiles.Init(MaxProfiles);
    cpuProfiles.Init(MaxProfiles);
}

void Profiler::Shutdown()
{
    DX12::DeferredRelease(queryHeap);
    for(uint32 i = 0; i < DX12::RenderLatency; ++i)
        readbackBuffers[i].Shutdown();
    profiles.Shutdown();
    cpuProfiles.Shutdown();
    numProfiles = 0;
}

uint64 Profiler::StartProfile(ID3D12GraphicsCommandList* cmdList, const char* name)
{
    Assert_(name != nullptr);
    if(enableGPUProfiling == false)
        return uint64(-1);

    uint64 profileIdx = uint64(-1);
    for(uint64 i = 0; i < numProfiles; ++i)
    {
        if(profiles[i].Name == name)
        {
            profileIdx = i;
            break;
        }
    }

    if(profileIdx == uint64(-1))
    {
        Assert_(numProfiles < MaxProfiles);
        profileIdx = numProfiles++;
        profiles[profileIdx].Name = name;
    }

    ProfileData& profileData = profiles[profileIdx];
    Assert_(profileData.QueryStarted == false);
    Assert_(profileData.QueryFinished == false);
    profileData.CPUProfile = false;
    profileData.Active = true;

    // Insert the start timestamp
    const uint32 startQueryIdx = uint32(profileIdx * 2);
    cmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx);

    profileData.QueryStarted = true;

    return profileIdx;
}

void Profiler::EndProfile(ID3D12GraphicsCommandList* cmdList, uint64 idx)
{
    if(enableGPUProfiling == false)
        return;

    Assert_(idx < numProfiles);

    ProfileData& profileData = profiles[idx];
    Assert_(profileData.QueryStarted == true);
    Assert_(profileData.QueryFinished == false);

    // Insert the end timestamp
    const uint32 startQueryIdx = uint32(idx * 2);
    const uint32 endQueryIdx = startQueryIdx + 1;
    cmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, endQueryIdx);

    // Resolve the data
    const uint64 dstOffset = startQueryIdx * sizeof(uint64);
    cmdList->ResolveQueryData(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx, 2, readbackBuffers[DX12::CurrFrameIdx].Resource, dstOffset);

    profileData.QueryStarted = false;
    profileData.QueryFinished = true;
}

uint64 Profiler::StartCPUProfile(const char* name)
{
    Assert_(name != nullptr);

    uint64 profileIdx = uint64(-1);
    for(uint64 i = 0; i < numCPUProfiles; ++i)
    {
        if(cpuProfiles[i].Name == name)
        {
            profileIdx = i;
            break;
        }
    }

    if(profileIdx == uint64(-1))
    {
        Assert_(numCPUProfiles < MaxProfiles);
        profileIdx = numCPUProfiles++;
        cpuProfiles[profileIdx].Name = name;
    }

    ProfileData& profileData = cpuProfiles[profileIdx];
    Assert_(profileData.QueryStarted == false);
    Assert_(profileData.QueryFinished == false);
    profileData.CPUProfile = true;
    profileData.Active = true;

    timer.Update();
    profileData.StartTime = timer.ElapsedMicroseconds();

    profileData.QueryStarted = true;

    return profileIdx;
}

void Profiler::EndCPUProfile(uint64 idx)
{
    Assert_(idx < numCPUProfiles);

    ProfileData& profileData = cpuProfiles[idx];
    Assert_(profileData.QueryStarted == true);
    Assert_(profileData.QueryFinished == false);

    timer.Update();
    profileData.EndTime = timer.ElapsedMicroseconds();

    profileData.QueryStarted = false;
    profileData.QueryFinished = true;
}

static void UpdateProfile(ProfileData& profile, uint64 profileIdx, bool drawText, uint64 gpuFrequency, const uint64* frameQueryData)
{
    profile.QueryFinished = false;

    double time = 0.0f;
    if(profile.CPUProfile)
    {
        time = double(profile.EndTime - profile.StartTime) / 1000.0;
    }
    else if(frameQueryData)
    {
        Assert_(frameQueryData != nullptr);

        // Get the query data
        uint64 startTime = frameQueryData[profileIdx * 2 + 0];
        uint64 endTime = frameQueryData[profileIdx * 2 + 1];

        if(endTime > startTime)
        {
            uint64 delta = endTime - startTime;
            double frequency = double(gpuFrequency);
            time = (delta / frequency) * 1000.0;
        }
    }

    profile.TimeSamples[profile.CurrSample] = time;
    profile.CurrSample = (profile.CurrSample + 1) % ProfileData::FilterSize;

    double maxTime = 0.0;
    double avgTime = 0.0;
    uint64 avgTimeSamples = 0;
    for(UINT i = 0; i < ProfileData::FilterSize; ++i)
    {
        if(profile.TimeSamples[i] <= 0.0)
            continue;
        maxTime = Max(profile.TimeSamples[i], maxTime);
        avgTime += profile.TimeSamples[i];
        ++avgTimeSamples;
    }

    if(avgTimeSamples > 0)
        avgTime /= double(avgTimeSamples);

    if(profile.Active && drawText)
        ImGui::Text("%s: %.2fms (%.2fms max)", profile.Name, avgTime, maxTime);

    profile.AverageTime = avgTime;
    profile.MaxTime = maxTime;

    profile.Active = false;
}

void Profiler::EndFrame(uint32 displayWidth, uint32 displayHeight, uint32 avgFPS, double avgFrameTime)
{
    uint64 gpuFrequency = 0;
    const uint64* frameQueryData = nullptr;
    if(enableGPUProfiling)
    {
        DX12::GfxQueue->GetTimestampFrequency(&gpuFrequency);

        frameQueryData = readbackBuffers[DX12::CurrFrameIdx].Map<uint64>();
    }

    bool drawText = false;
    if(showUI == false)
    {
        ImGui::SetNextWindowSize(ImVec2(75.0f, 25.0f));
        ImGui::SetNextWindowPos(ImVec2(25.0f, 50.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
        if(ImGui::Begin("profiler_button", nullptr, flags))
        {
            if(ImGui::Button("Timing"))
                showUI = true;
        }

        ImGui::PopStyleVar();
    }
    else
    {
        ImVec2 initialSize = ImVec2(displayWidth * 0.5f, float(displayHeight) * 0.25f);
        ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);

        drawText = ImGui::Begin("Timing", &showUI);

        if(logToClipboard)
            ImGui::LogToClipboard();
    }

    if(drawText)
    {
        ImGui::Text("Total Frame Time: %.2f ms (%u FPS)", avgFrameTime * 1000.0, avgFPS);
        ImGui::Separator();
        ImGui::Text("");

        ImGui::Text("GPU Timing");
        ImGui::Separator();
    }

    // Iterate over all of the profiles
    for(uint64 profileIdx = 0; profileIdx < numProfiles; ++profileIdx)
        UpdateProfile(profiles[profileIdx], profileIdx, drawText, gpuFrequency, frameQueryData);

    if(drawText)
    {
        ImGui::Text(" ");
        ImGui::Text("CPU Timing");
        ImGui::Separator();
    }

    for(uint64 profileIdx = 0; profileIdx < numCPUProfiles; ++profileIdx)
        UpdateProfile(cpuProfiles[profileIdx], profileIdx, drawText, gpuFrequency, frameQueryData);

    if(showUI)
    {
        if(logToClipboard)
            ImGui::LogFinish();

        ImGui::Text(" ");
        logToClipboard = ImGui::Button("Copy To Clipboard");
    }
    else
        logToClipboard = false;

    ImGui::End();

    if(enableGPUProfiling)
        readbackBuffers[DX12::CurrFrameIdx].Unmap();

    enableGPUProfiling = showUI || alwaysEnableGPUProfiling;
}

double Profiler::GPUProfileTiming(const char* name) const
{
    uint64 profileIdx = uint64(-1);
    for(uint64 i = 0; i < numProfiles; ++i)
    {
        if(profiles[i].Name == name)
        {
            profileIdx = i;
            break;
        }
    }

    if(profileIdx == uint64(-1))
        return 0.0;

    uint64 gpuFrequency = 0;
    DX12::GfxQueue->GetTimestampFrequency(&gpuFrequency);

    const uint64* frameQueryData = readbackBuffers[DX12::CurrFrameIdx].Map<uint64>();

    // Get the query data
    uint64 startTime = frameQueryData[profileIdx * 2 + 0];
    uint64 endTime = frameQueryData[profileIdx * 2 + 1];

    double time = 0.0;
    if(endTime > startTime)
    {
        uint64 delta = endTime - startTime;
        double frequency = double(gpuFrequency);
        time = (delta / frequency) * 1000.0;
    }

    readbackBuffers[DX12::CurrFrameIdx].Unmap();

    return time;
}

double Profiler::CPUProfileTiming(const char* name) const
{
    for(uint64 i = 0; i < numCPUProfiles; ++i)
    {
        if(cpuProfiles[i].Name == name)
            return (cpuProfiles[i].EndTime - cpuProfiles[i].StartTime) / 1000.0;
    }

    return 0.0;
}

double Profiler::GPUProfileTimingAvg(const char* name) const
{
    for(uint64 i = 0; i < numProfiles; ++i)
    {
        if(profiles[i].Name == name)
            return profiles[i].AverageTime;
    }

    return 0.0;
}

double Profiler::CPUProfileTimingAvg(const char* name) const
{
    for(uint64 i = 0; i < numCPUProfiles; ++i)
    {
        if(cpuProfiles[i].Name == name)
            return cpuProfiles[i].AverageTime;
    }

    return 0.0;
}

void Profiler::SetAlwaysEnableGPUProfiling(bool enable)
{
    alwaysEnableGPUProfiling = enable;
}

// == ProfileBlock ================================================================================

ProfileBlock::ProfileBlock(ID3D12GraphicsCommandList* cmdList_, const char* name) : cmdList(cmdList_)
{
    idx = Profiler::GlobalProfiler.StartProfile(cmdList, name);
}

ProfileBlock::~ProfileBlock()
{
    Profiler::GlobalProfiler.EndProfile(cmdList, idx);
}

// == CPUProfileBlock =============================================================================

CPUProfileBlock::CPUProfileBlock(const char* name)
{
    idx = Profiler::GlobalProfiler.StartCPUProfile(name);
}

CPUProfileBlock::~CPUProfileBlock()
{
    Profiler::GlobalProfiler.EndCPUProfile(idx);
}

}