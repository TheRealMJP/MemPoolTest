enum HeapTypes
{
    Upload,
    Default,
    Custom,

    [EnumLabel("GPU Upload")]
    GPUUpload,
}

enum CPUPageProperties
{
    [EnumLabel("Not Available (No CPU Access)")]
    NotAvailable,

    [EnumLabel("Write-Combined (Uncached)")]
    WriteCombine,

    [EnumLabel("Write-Back (Cached)")]
    WriteBack
}

enum MemoryPools
{
    [EnumLabel("L0 (CPU RAM)")]
    L0,

    [EnumLabel("L1 (VRAM)")]
    L1
}

enum BufferTypes
{
    Raw,
    Formatted,
    Structured,
    Constant,
}

enum BufferUploadPaths
{
    [EnumLabel("DIRECT Queue")]
    DirectQueue,

    [EnumLabel("Upload COPY Queue")]
    UploadCopyQueue,

    [EnumLabel("Fast Upload COPY Queue")]
    FastUploadCopyQueue,
}

public class Settings
{
    const uint ThreadGroupSize = 256;

    public class TestConfig
    {
        HeapTypes HeapType = HeapTypes.Upload;

        [DisplayName("Heap CPUPageProperty")]
        [Visible(false)]
        [UseAsShaderConstant(false)]
        CPUPageProperties CPUPageProperty = CPUPageProperties.NotAvailable;

        [DisplayName("Heap MemoryPool")]
        [Visible(false)]
        [UseAsShaderConstant(false)]
        MemoryPools MemoryPool = MemoryPools.L0;

        [UseAsShaderConstant(false)]
        BufferTypes InputBufferType = BufferTypes.Raw;

        [MinValue(0)]
        [MaxValue(256)]
        [UseAsShaderConstant(false)]
        int InputBufferSizeMB = 16;

        [MinValue(0)]
        [MaxValue(1024)]
        [UseAsShaderConstant(false)]
        int InputBufferSizeKB = 0;

        [MinValue(0)]
        [MaxValue(1024)]
        [UseAsShaderConstant(false)]
        int InputBufferSizeBytes = 0;

        [MinValue(1)]
        [MaxValue(64)]
        [UseAsShaderConstant(false)]
        int ElemsPerThread = 1;

        [MinValue(1)]
        [MaxValue(64)]
        [UseAsShaderConstant(false)]
        int ThreadElemStride = 1;

        [MinValue(0)]
        [MaxValue(16)]
        [UseAsShaderConstant(false)]
        int GroupElemOffset = 1;

        [MinValue(0)]
        [MaxValue(16)]
        [UseAsShaderConstant(false)]
        int ThreadElemOffset = 1;

        [MinValue(1)]
        [MaxValue(65535)]
        [UseAsShaderConstant(false)]
        int NumThreadGroups = 4096;

        [DisplayName("Read From GPU Memory")]
        [UseAsShaderConstant(false)]
        bool ReadFromGPUMem = false;

        [UseAsShaderConstant(false)]
        BufferUploadPaths BufferUploadPath = BufferUploadPaths.FastUploadCopyQueue;

        [DisplayName("Background Upload Size (MB)")]
        [UseAsShaderConstant(false)]
        [MinValue(0)]
        [MaxValue(256)]
        int BackgroundUploadSize = 0;

        [DisplayName("Background Upload Size Wait Time (ms)")]
        [UseAsShaderConstant(false)]
        [MinValue(0)]
        [MaxValue(100)]
        int BackgroundUploadWaitTime = 0;

        [Visible(false)]
        [UseAsShaderConstant(false)]
        int NumInputBufferElems = 0;

        [Visible(false)]
        int InputBufferIdx = -1;

        [Visible(false)]
        int OutputBufferIdx = -1;
    }

    [ExpandGroup(true)]
    public class Debug
    {
        [UseAsShaderConstant(false)]
        [DisplayName("Enable VSync")]
        [HelpText("Enables or disables vertical sync during Present")]
        bool EnableVSync = true;

        [UseAsShaderConstant(false)]
        [HelpText("Enables the stable power state, which stabilizes GPU clocks for more consistent performance")]
        bool StablePowerState = true;

        [UseAsShaderConstant(false)]
        bool EnableDriverBackgroundThreads = false;
    }
}