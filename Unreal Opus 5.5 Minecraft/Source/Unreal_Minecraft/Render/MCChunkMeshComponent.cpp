#include "Render/MCChunkMeshComponent.h"

TArray<TObjectPtr<UMaterialInterface>>* MCRender::GVoxelMaterials = nullptr;
TArray<TObjectPtr<UMaterialInterface>>* MCRender::GItemMaterials = nullptr;
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshResources.h"
#include "LocalVertexFactory.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "RayTracingInstance.h"
#include "RayTracingGeometry.h"
#include "RenderUtils.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MCChunkMeshComponent)

static TAutoConsoleVariable<int32> CVarOpus55ChunkRayTracing(
	TEXT("opus55.ChunkRayTracing"), 1,
	TEXT("Include voxel chunk geometry in hardware ray tracing (Lumen HWRT, RT shadows)."));

namespace
{
	/** Layers that are included in the ray tracing representation. */
	FORCEINLINE bool LayerInRayTracing(int32 L)
	{
		return L == 0 || L == 1 || L == 4; // opaque, cutout, lava
	}
}

class FMCChunkSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FMCChunkSceneProxy(UMCChunkMeshComponent* Component, TUniquePtr<FMCChunkMeshData>&& InData)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FMCChunkSceneProxy")
		, Data(MoveTemp(InData))
	{
		const EShaderPlatform Platform = GetScene().GetShaderPlatform();
		for (int32 L = 0; L < FMCChunkMeshData::NumLayers; ++L)
		{
			UMaterialInterface* M = Component->GetMaterial(L);
			if (!M) M = UMaterial::GetDefaultMaterial(MD_Surface);
			Materials[L] = M;
			if (Data.IsValid() && !Data->Layers[L].IsEmpty())
			{
				MaterialRelevance |= M->GetRelevance_Concurrent(Platform);
			}
		}
#if RHI_RAYTRACING
		bSupportRayTracing = IsRayTracingEnabled() && CVarOpus55ChunkRayTracing.GetValueOnAnyThread() != 0;
#endif
		bVerifyUsedMaterials = false;
	}

	virtual ~FMCChunkSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
#if RHI_RAYTRACING
		RayTracingGeometry.ReleaseResource();
#endif
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{
		if (!Data.IsValid()) return;
		int32 NumVerts = 0, NumIndices = 0;
		for (const FMCMeshLayerData& L : Data->Layers) { NumVerts += L.Vertices.Num(); NumIndices += L.Indices.Num(); }
		if (NumVerts == 0 || NumIndices == 0) { Data.Reset(); return; }

		VertexBuffers.PositionVertexBuffer.Init(NumVerts);
		VertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
		VertexBuffers.StaticMeshVertexBuffer.Init(NumVerts, 3);
		VertexBuffers.ColorVertexBuffer.Init(NumVerts);
		IndexBuffer.Indices.SetNumUninitialized(NumIndices);

		int32 VBase = 0, IBase = 0;
		for (int32 L = 0; L < FMCChunkMeshData::NumLayers; ++L)
		{
			const FMCMeshLayerData& Layer = Data->Layers[L];
			FLayerRange& R = Ranges[L];
			R.FirstIndex = IBase;
			R.NumIndices = Layer.Indices.Num();
			R.MinVertex = VBase;
			R.MaxVertex = VBase + FMath::Max(0, Layer.Vertices.Num() - 1);
			for (int32 i = 0; i < Layer.Vertices.Num(); ++i)
			{
				const FMCMeshVertex& V = Layer.Vertices[i];
				const int32 Dst = VBase + i;
				VertexBuffers.PositionVertexBuffer.VertexPosition(Dst) = V.Pos;
				const FVector3f TangentY = FVector3f::CrossProduct(V.Normal, V.Tangent) * V.TangentSign;
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(Dst, V.Tangent, TangentY, V.Normal);
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(Dst, 0, V.UV0);
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(Dst, 1, V.UV1);
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(Dst, 2, V.UV2);
				VertexBuffers.ColorVertexBuffer.VertexColor(Dst) = V.Color;
			}
			for (int32 i = 0; i < Layer.Indices.Num(); ++i)
			{
				IndexBuffer.Indices[IBase + i] = Layer.Indices[i] + VBase;
			}
			VBase += Layer.Vertices.Num();
			IBase += Layer.Indices.Num();
		}
		Data.Reset(); // free CPU copy

#if RHI_RAYTRACING
		if (bSupportRayTracing)
		{
			EnumAddFlags(IndexBuffer.UsageFlags, EBufferUsageFlags::ShaderResource);
		}
#endif
		VertexBuffers.PositionVertexBuffer.InitResource(RHICmdList);
		VertexBuffers.StaticMeshVertexBuffer.InitResource(RHICmdList);
		VertexBuffers.ColorVertexBuffer.InitResource(RHICmdList);
		IndexBuffer.InitResource(RHICmdList);

		FLocalVertexFactory::FDataType VFData;
		VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&VertexFactory, VFData);
		VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&VertexFactory, VFData);
		VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&VertexFactory, VFData);
		VertexBuffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(&VertexFactory, VFData, 0);
		VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(&VertexFactory, VFData);
		VertexFactory.SetData(RHICmdList, VFData);
		VertexFactory.InitResource(RHICmdList);

#if RHI_RAYTRACING
		if (bSupportRayTracing)
		{
			FRayTracingGeometryInitializer Initializer;
			Initializer.DebugName = FDebugName(TEXT("Opus55Chunk"));
			Initializer.IndexBuffer = IndexBuffer.IndexBufferRHI;
			Initializer.GeometryType = RTGT_Triangles;
			Initializer.bFastBuild = true;
			Initializer.bAllowUpdate = false;
			uint32 Total = 0;
			for (int32 L = 0; L < FMCChunkMeshData::NumLayers; ++L)
			{
				const FLayerRange& R = Ranges[L];
				if (R.NumIndices == 0 || !LayerInRayTracing(L)) continue;
				FRayTracingGeometrySegment Segment;
				Segment.VertexBuffer = VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
				Segment.VertexBufferElementType = VET_Float3;
				Segment.VertexBufferStride = sizeof(FVector3f);
				Segment.VertexBufferOffset = 0;
				Segment.MaxVertices = VertexBuffers.PositionVertexBuffer.GetNumVertices();
				Segment.FirstPrimitive = R.FirstIndex / 3;
				Segment.NumPrimitives = R.NumIndices / 3;
				Initializer.Segments.Add(Segment);
				RTSegmentLayers.Add(L);
				Total += Segment.NumPrimitives;
			}
			Initializer.TotalPrimitiveCount = Total;
			if (Total > 0)
			{
				RayTracingGeometry.SetInitializer(Initializer);
				RayTracingGeometry.InitResource(RHICmdList);
			}
		}
#endif
	}

	void BuildBatch(FMeshBatch& Mesh, int32 L) const
	{
		const FLayerRange& R = Ranges[L];
		FMeshBatchElement& E = Mesh.Elements[0];
		E.IndexBuffer = &IndexBuffer;
		E.FirstIndex = R.FirstIndex;
		E.NumPrimitives = R.NumIndices / 3;
		E.MinVertexIndex = R.MinVertex;
		E.MaxVertexIndex = R.MaxVertex;
		Mesh.VertexFactory = &VertexFactory;
		Mesh.MaterialRenderProxy = Materials[L]->GetRenderProxy();
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.bDisableBackfaceCulling = false;
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.LODIndex = 0;
		Mesh.SegmentIndex = (uint8)L;
		Mesh.CastShadow = (L != 3 && L != 5 && L != 6); // no shadows from water / portals
		Mesh.bCanApplyViewModeOverrides = true;
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
		if (!VertexFactory.IsInitialized()) return;
		PDI->ReserveMemoryForMeshes(FMCChunkMeshData::NumLayers);
		for (int32 L = 0; L < FMCChunkMeshData::NumLayers; ++L)
		{
			if (Ranges[L].NumIndices == 0) continue;
			FMeshBatch Mesh;
			BuildBatch(Mesh, L);
			PDI->DrawMesh(Mesh, FLT_MAX);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		if (!VertexFactory.IsInitialized()) return;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex))) continue;
			for (int32 L = 0; L < FMCChunkMeshData::NumLayers; ++L)
			{
				if (Ranges[L].NumIndices == 0) continue;
				FMeshBatch& Mesh = Collector.AllocateMesh();
				BuildBatch(Mesh, L);
				Mesh.bUseWireframeSelectionColoring = IsSelected() ? 1 : 0;
				Collector.AddMesh(ViewIndex, Mesh);
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		if (IsRichView(*View->Family) || View->Family->EngineShowFlags.Bounds || View->Family->EngineShowFlags.Collision || IsSelected() || IsHovered())
		{
			Result.bDynamicRelevance = true;
		}
		else
		{
			Result.bStaticRelevance = true;
		}
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
		return Result;
	}

	virtual bool CanBeOccluded() const override { return !MaterialRelevance.bDisableDepthTest; }
	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
	uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

#if RHI_RAYTRACING
	virtual bool IsRayTracingRelevant() const override { return true; }
	virtual bool HasRayTracingRepresentation() const override { return bSupportRayTracing; }
	virtual bool IsRayTracingStaticRelevant() const override { return false; }

	virtual void GetDynamicRayTracingInstances(FRayTracingInstanceCollector& Collector) override
	{
		if (!bSupportRayTracing || !VertexFactory.IsInitialized() || !RayTracingGeometry.IsInitialized() || RTSegmentLayers.Num() == 0)
		{
			return;
		}
		if (CVarOpus55ChunkRayTracing.GetValueOnRenderThread() == 0) return;

		TConstArrayView<const FSceneView*> Views = Collector.GetViews();
		const uint32 VisibilityMap = Collector.GetVisibilityMap();
		const int32 FirstActiveViewIndex = FMath::CountTrailingZeros(VisibilityMap);
		if (!Views.IsValidIndex(FirstActiveViewIndex)) return;
		const FSceneView* FirstActiveView = Views[FirstActiveViewIndex];

		FRayTracingInstance RayTracingInstance;
		if (bNeedsRTCacheUpdate)
		{
			CachedRTMaterials.Reset();
			for (int32 S = 0; S < RTSegmentLayers.Num(); ++S)
			{
				const int32 L = RTSegmentLayers[S];
				FMeshBatch& MeshBatch = CachedRTMaterials.AddDefaulted_GetRef();
				BuildBatch(MeshBatch, L);
				MeshBatch.SegmentIndex = (uint8)S;
				MeshBatch.MeshIdInPrimitive = 0;
				MeshBatch.CastRayTracedShadow = IsShadowCast(FirstActiveView);
			}
			bNeedsRTCacheUpdate = false;
		}
		else
		{
			RayTracingInstance.bInstanceMaskAndFlagsDirty = false;
		}

		if (RayTracingGeometry.IsEvicted())
		{
			Collector.AddReferencedGeometry(RayTracingGeometry.GetGeometryHandle());
			return;
		}

		RayTracingInstance.MaterialsView = MakeArrayView(CachedRTMaterials);
		RayTracingInstance.Geometry = &RayTracingGeometry;
		CachedLocalToWorld = GetLocalToWorld();
		RayTracingInstance.InstanceTransformsView = MakeArrayView(&CachedLocalToWorld, 1);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0) continue;
			Collector.AddRayTracingInstance(ViewIndex, RayTracingInstance);
		}
	}
#endif

private:
	struct FLayerRange
	{
		int32 FirstIndex = 0;
		int32 NumIndices = 0;
		int32 MinVertex = 0;
		int32 MaxVertex = 0;
	};

	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	FLayerRange Ranges[FMCChunkMeshData::NumLayers];
	UMaterialInterface* Materials[FMCChunkMeshData::NumLayers];
	FMaterialRelevance MaterialRelevance;
	TUniquePtr<FMCChunkMeshData> Data;
#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
	TArray<FMeshBatch> CachedRTMaterials;
	TArray<int32> RTSegmentLayers;
	FMatrix CachedLocalToWorld;
	bool bSupportRayTracing = false;
	bool bNeedsRTCacheUpdate = true;
#endif
};

// ---------------------------------------------------------------------------------------------------------------------

UMCChunkMeshComponent::UMCChunkMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CastShadow = true;
	bCastDynamicShadow = true;
	bUseAsOccluder = true;
	bVisibleInRayTracing = true;
	bAffectDynamicIndirectLighting = true;
	bAffectDistanceFieldLighting = false;
	SetCanEverAffectNavigation(false);
	Mobility = EComponentMobility::Static;
	bNeverDistanceCull = false;
}

void UMCChunkMeshComponent::SetMeshData(TUniquePtr<FMCChunkMeshData>&& InData)
{
	if (!InData.IsValid() || InData->IsEmpty())
	{
		ClearMesh();
		return;
	}
	LocalBounds = InData->Bounds.ExpandBy(1.0);
	LastTriangles = InData->NumTriangles();
	if (bRetainData) RetainedData = MakeUnique<FMCChunkMeshData>(*InData);
	PendingData = MoveTemp(InData);
	bHasGeometry = true;
	UpdateBounds();
	MarkRenderStateDirty();
}

void UMCChunkMeshComponent::ClearMesh()
{
	PendingData.Reset();
	RetainedData.Reset();
	LastTriangles = 0;
	if (bHasGeometry)
	{
		bHasGeometry = false;
		MarkRenderStateDirty();
	}
}

FPrimitiveSceneProxy* UMCChunkMeshComponent::CreateSceneProxy()
{
	if (!PendingData.IsValid() && RetainedData.IsValid()) PendingData = MakeUnique<FMCChunkMeshData>(*RetainedData);
	if (!PendingData.IsValid())
	{
		if (bHasGeometry) bNeedsRebuild = true;
		return nullptr;
	}
	bNeedsRebuild = false;
	return new FMCChunkSceneProxy(this, MoveTemp(PendingData));
}

FBoxSphereBounds UMCChunkMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(LocalBounds.TransformBy(LocalToWorld));
}

int32 UMCChunkMeshComponent::GetNumMaterials() const
{
	return FMCChunkMeshData::NumLayers;
}

UMaterialInterface* UMCChunkMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (SharedMaterials && SharedMaterials->IsValidIndex(ElementIndex)) return (*SharedMaterials)[ElementIndex];
	return nullptr;
}

void UMCChunkMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (!SharedMaterials) return;
	for (UMaterialInterface* M : *SharedMaterials) if (M) OutMaterials.Add(M);
}
