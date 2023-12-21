//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#ifndef MESHVERTEX_HLSL_
#define MESHVERTEX_HLSL_

struct MeshVertex
{
    float3 Position;
    float3 Normal;
    float2 UV;
    float3 Tangent;
    float3 Bitangent;
};

#endif // MESHVERTEX_HLSL_