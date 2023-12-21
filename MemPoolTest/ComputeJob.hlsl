//=================================================================================================
//
//  D3D12 Memory Pool Performance Test
//  by MJP
//  https://therealmjp.github.io/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

//=================================================================================================
// Includes
//=================================================================================================
#include "AppSettings.hlsl"

#if ConstantBuffer_
    struct CB
    {
        float4 Elems[NumInputBufferElems_];
    };
    ConstantBuffer<CB> CBuffer : register(b0);
#endif

[numthreads(ThreadGroupSize, 1, 1)]
void ComputeJob(in uint3 dispatchThreadID : SV_DispatchThreadID, in uint3 groupThreadID : SV_GroupThreadID,
                in uint3 groupID : SV_GroupID)
{
    const uint localThreadIndex = groupThreadID.x;
    const uint globalThreadIndex = dispatchThreadID.x;
    const uint groupIndex = groupID.x;

    #if RawBuffer_
        ByteAddressBuffer inputBuffer = ResourceDescriptorHeap[AppSettings.InputBufferIdx];
    #elif FormattedBuffer_
        Buffer<float4> inputBuffer = ResourceDescriptorHeap[AppSettings.InputBufferIdx];
    #elif StructuredBuffer_
        StructuredBuffer<float4> inputBuffer = ResourceDescriptorHeap[AppSettings.InputBufferIdx];
    #endif

    float sum = 0.0f;

    uint elemIdx = (ElemsPerThread_ * ThreadGroupSize * GroupElemOffset_ * groupIndex) +
                    (ElemsPerThread_ * ThreadElemOffset_ * localThreadIndex);
    for(uint i = 0; i < ElemsPerThread_; ++i)
    {
        const uint loadIdx = elemIdx % NumInputBufferElems_;

        #if RawBuffer_
            const float4 data = asfloat(inputBuffer.Load4(loadIdx * 16));
        #elif StructuredBuffer_ || FormattedBuffer_
            const float4 data = inputBuffer[loadIdx];
        #elif ConstantBuffer_
            const float4 data = CBuffer.Elems[loadIdx];
        #endif

        sum += dot(data, 1.0f);

        elemIdx += ThreadElemStride_;
    }

    if(sum < 0.0f)
    {
        RWByteAddressBuffer outputBuffer = ResourceDescriptorHeap[AppSettings.OutputBufferIdx];
        outputBuffer.Store(globalThreadIndex * 4, sum);
    }
}