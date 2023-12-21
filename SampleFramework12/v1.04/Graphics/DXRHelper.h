//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "../PCH.h"
#include "../Containers.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

class Model;

// Helper for building a ray tracing PSO (state object)
struct StateObjectBuilder
{
    Array<uint8> SubObjectData;
    Array<D3D12_STATE_SUBOBJECT> SubObjects;
    uint64 NumSubObjects = 0;
    uint64 MaxSubObjects = 0;

    void Init(uint64 maxSubObjects);

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_STATE_OBJECT_CONFIG& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_GLOBAL_ROOT_SIGNATURE& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_LOCAL_ROOT_SIGNATURE& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_NODE_MASK& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_DXIL_LIBRARY_DESC& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_EXISTING_COLLECTION_DESC& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_RAYTRACING_SHADER_CONFIG& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_RAYTRACING_PIPELINE_CONFIG& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_HIT_GROUP_DESC& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP);
    }

    #if EnableWorkGraphsPreview_
        const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_WORK_GRAPH_DESC& subObjDesc)
        {
            return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH);
        }
    #endif

    const D3D12_STATE_SUBOBJECT* AddSubObject(const void* subObjDesc, uint64 subObjDescSize, D3D12_STATE_SUBOBJECT_TYPE type);

    void BuildDesc(D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc);

    ID3D12StateObject* CreateStateObject(D3D12_STATE_OBJECT_TYPE type);
};

// Helper for embedding shader indentifiers in shader records inside of ray tracing shader table
struct ShaderIdentifier
{
    uint8 Data[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES] = { };

    ShaderIdentifier() = default;
    explicit ShaderIdentifier(const void* idPointer)
    {
        memcpy(Data, idPointer, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
};

// Per-geometry data put into a buffer during the acceleration structure build process
struct GeometryInfo
{
    uint32 VtxOffset = 0;
    uint32 IdxOffset = 0;
    uint32 MaterialIdx = 0;
    uint32 PadTo16Bytes = 0;
};

// Data output by BuildModelAccelStructure
struct ModelAccelStructure
{
    RTAccelStructure BottomLevelAccelStructure;
    RTAccelStructure TopLevelAccelStructure;
    StructuredBuffer GeoInfoBuffer;
    DescriptorIndex VertexBufferSRV;
    DescriptorIndex IndexBufferSRV;

    void Shutdown()
    {
        BottomLevelAccelStructure.Shutdown();

        TopLevelAccelStructure.Shutdown();
        GeoInfoBuffer.Shutdown();
    }
};

void BuildModelAccelStructure(ID3D12GraphicsCommandList7* cmdList, const Model& model, float sceneScale, ModelAccelStructure& output);


// Data output by BuildSceneAccelStructure
struct SceneAccelStructure
{
    Array<ModelAccelStructure> ModelAccelStructures;
    RTAccelStructure TopLevelAccelStructure;

    void Shutdown()
    {
        for(ModelAccelStructure& modelAS : ModelAccelStructures)
            modelAS.Shutdown();
        TopLevelAccelStructure.Shutdown();
    }
};

void BuildSceneAccelStructure(ID3D12GraphicsCommandList7* cmdList, const Model* const* models, uint32 numModels, SceneAccelStructure& output);

} // namespace SampleFramework12