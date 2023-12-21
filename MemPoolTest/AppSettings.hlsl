#pragma once

struct AppSettings_CBLayout
{
    int HeapType;
    int InputBufferIdx;
    int OutputBufferIdx;
};

ConstantBuffer<AppSettings_CBLayout> AppSettingsCB : register(b12);

struct AppSettings_Values
{
    int HeapType;
    int InputBufferIdx;
    int OutputBufferIdx;
};

static const AppSettings_Values AppSettings =
{
    AppSettingsCB.HeapType,
    AppSettingsCB.InputBufferIdx,
    AppSettingsCB.OutputBufferIdx,
};

enum HeapTypes
{
    Upload = 0,
    Default = 1,
    Custom = 2,
    GPUUpload = 3,
};

enum CPUPageProperties
{
    NotAvailable = 0,
    WriteCombine = 1,
    WriteBack = 2,
};

enum MemoryPools
{
    L0 = 0,
    L1 = 1,
};

enum BufferTypes
{
    Raw = 0,
    Formatted = 1,
    Structured = 2,
    Constant = 3,
};

enum BufferUploadPaths
{
    DirectQueue = 0,
    UploadCopyQueue = 1,
    FastUploadCopyQueue = 2,
};

static const uint ThreadGroupSize = 256;
