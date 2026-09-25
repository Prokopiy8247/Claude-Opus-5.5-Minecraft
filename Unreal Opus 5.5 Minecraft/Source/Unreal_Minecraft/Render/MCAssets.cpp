#include "Render/MCAssets.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

namespace
{
	TMap<FString, UObject*> GCache;

	template<typename T>
	T* LoadCached(const TCHAR* Path)
	{
		if (UObject** Found = GCache.Find(Path)) return Cast<T>(*Found);
		T* Obj = nullptr;
		if (FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(Path))))
		{
			Obj = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		if (Obj) Obj->AddToRoot();
		GCache.Add(Path, Obj);
		return Obj;
	}
}

namespace MCAssets
{
	UStaticMesh* Mesh(const TCHAR* Path) { return LoadCached<UStaticMesh>(Path); }
	UMaterialInterface* Material(const TCHAR* Path) { return LoadCached<UMaterialInterface>(Path); }

	UStaticMesh* Cube() { return Mesh(TEXT("/Engine/BasicShapes/Cube.Cube")); }
	UStaticMesh* Sphere() { return Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")); }
	UStaticMesh* Cylinder() { return Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
	UStaticMesh* Plane() { return Mesh(TEXT("/Engine/BasicShapes/Plane.Plane")); }

	UMaterialInterface* EntityMaterial()
	{
		if (UMaterialInterface* M = Material(TEXT("/Game/Opus55Minecraft/Materials/M_MCEntity.M_MCEntity"))) return M;
		return Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	UMaterialInterface* GlowMaterial()
	{
		if (UMaterialInterface* M = Material(TEXT("/Game/Opus55Minecraft/Materials/M_MCGlow.M_MCGlow"))) return M;
		return EntityMaterial();
	}

	UMaterialInterface* BeamMaterial()
	{
		if (UMaterialInterface* M = Material(TEXT("/Game/Opus55Minecraft/Materials/M_MCBeam.M_MCBeam"))) return M;
		return GlowMaterial();
	}

	UMaterialInstanceDynamic* MakeMID(UObject* Outer, UMaterialInterface* Parent, const FLinearColor& Color, float Emissive)
	{
		if (!Parent) return nullptr;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetVectorParameterValue(TEXT("Tint"), Color);
		MID->SetScalarParameterValue(TEXT("Glow"), Emissive);
		MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		return MID;
	}

	UStaticMeshComponent* AddMeshPart(AActor* Owner, USceneComponent* Parent, UStaticMesh* M, const FVector& LocUU, const FVector& Scale,
		UMaterialInterface* Mat, const FLinearColor& Color, float Emissive, bool bShadow)
	{
		if (!Owner || !M) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(M);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(bShadow);
		C->bReceivesDecals = false;
		C->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
		C->SetRelativeLocation(LocUU);
		C->SetRelativeScale3D(Scale);
		if (Mat) C->SetMaterial(0, MakeMID(C, Mat, Color, Emissive));
		C->RegisterComponent();
		return C;
	}
}
