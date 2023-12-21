//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#if CPP_
    #define ShaderScalar_(T, name, init)        \
        struct name                             \
        {                                       \
            T Value = init;                     \
                                                \
            constexpr name()                    \
            {                                   \
            }                                   \
                                                \
            constexpr name(T val) : Value(val)  \
            {                                   \
            }                                   \
                                                \
            constexpr name& operator=(T val)    \
            {                                   \
                Value = val;                    \
                return *this;                   \
            }                                   \
                                                \
            constexpr operator T() const        \
            {                                   \
                return Value;                   \
            }                                   \
        }                                       \

    ShaderScalar_(uint32_t, ShaderUint, 0);
    ShaderScalar_(uint16_t, ShaderUint16, 0);
    ShaderScalar_(int32_t, ShaderInt, 0);
    ShaderScalar_(int16_t, ShaderInt16, 0);
    ShaderScalar_(float, ShaderFloat, 0.0f);
    ShaderScalar_(uint16_t, ShaderFloat16, 0);
    ShaderScalar_(uint32_t, ShaderBool, 0);
    ShaderScalar_(uint32_t, DescriptorIndex, uint32_t(-1));

    typedef SampleFramework12::Float2 ShaderFloat2;
    typedef SampleFramework12::Float3 ShaderFloat3;
    typedef SampleFramework12::Float4 ShaderFloat4;

    typedef SampleFramework12::Uint2 ShaderUint2;
    typedef SampleFramework12::Uint3 ShaderUint3;
    typedef SampleFramework12::Uint4 ShaderUint4;

    typedef SampleFramework12::Int2 ShaderInt2;
    typedef SampleFramework12::Int3 ShaderInt3;
    typedef SampleFramework12::Int4 ShaderInt4;

    typedef SampleFramework12::Float3x3 ShaderFloat3x3;
    typedef SampleFramework12::Float4x4 ShaderFloat4x4;

    #define row_major
    #define SharedConstant_ constexpr
#else
    typedef uint32_t ShaderUint;
    typedef uint16_t ShaderUint16;
    typedef int32_t ShaderInt;
    typedef int16_t ShaderInt16;
    typedef float ShaderFloat;
    typedef uint16_t ShaderFloat16;
    typedef bool ShaderBool;
    typedef uint32_t DescriptorIndex;

    typedef float2 ShaderFloat2;
    typedef float3 ShaderFloat3;
    typedef float4 ShaderFloat4;

    typedef uint2 ShaderUint2;
    typedef uint3 ShaderUint3;
    typedef uint4 ShaderUint4;

    typedef int2 ShaderInt2;
    typedef int3 ShaderInt3;
    typedef int4 ShaderInt4;

    typedef float3x3 ShaderFloat3x3;
    typedef float4x4 ShaderFloat4x4;

    #define SharedConstant_ static const
#endif

SharedConstant_ DescriptorIndex InvalidDescriptorIndex = uint32_t(-1);