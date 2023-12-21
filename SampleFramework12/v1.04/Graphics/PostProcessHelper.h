//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "..\\PCH.h"

#include "..\\InterfacePointers.h"
#include "..\\SF12_Math.h"
#include "ShaderCompilation.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

struct TempRenderTarget
{
    RenderTexture RT;
    uint32 Width() const { return RT.Texture.Width; }
    uint32 Height() const { return RT.Texture.Height; }
    DXGI_FORMAT Format() const { return RT.Texture.Format; }
    bool32 InUse = false;
};

class PostProcessHelper
{

public:

    PostProcessHelper();
    ~PostProcessHelper();

    void Initialize();
    void Shutdown();

    void ClearCache();

    TempRenderTarget* GetTempRenderTarget(uint32 width, uint32 height, DXGI_FORMAT format, bool useAsUAV = false);

    void Begin(ID3D12GraphicsCommandList* cmdList);
    void End();

    void PostProcess(CompiledShaderPtr pixelShader, const char* name, const RenderTexture& input, const RenderTexture& output);
    void PostProcess(CompiledShaderPtr pixelShader, const char* name, const RenderTexture& input, const TempRenderTarget* output);
    void PostProcess(CompiledShaderPtr pixelShader, const char* name, const TempRenderTarget* input, const RenderTexture& output);
    void PostProcess(CompiledShaderPtr pixelShader, const char* name, const TempRenderTarget* input, const TempRenderTarget* output);
    void PostProcess(CompiledShaderPtr pixelShader, const char* name, const uint32* inputs, uint64 numInputs,
                     const RenderTexture*const* outputs, uint64 numOutputs);

protected:


    struct CachedPSO
    {
        ID3D12PipelineState* PSO = nullptr;
        Hash Hash;
    };

    List<TempRenderTarget*> tempRenderTargets;
    List<CachedPSO> pipelineStates;

    CompiledShaderPtr fullScreenTriVS;

    ID3D12GraphicsCommandList* cmdList = nullptr;
};

}
