#include "SiteTrees.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

static UHierarchicalInstancedStaticMeshComponent* MakeH(AActor* A, const TCHAR* N)
{
	UHierarchicalInstancedStaticMeshComponent* C = A->CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(N);
	C->SetMobility(EComponentMobility::Static);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(true);
	C->InstanceEndCullDistance = 450000;   // 4.5 km
	return C;
}

ASiteTrees::ASiteTrees()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Broadleaf = MakeH(this, TEXT("Broadleaf")); Broadleaf->SetupAttachment(RootComponent);
	Small = MakeH(this, TEXT("Small")); Small->SetupAttachment(RootComponent);
	Poplar = MakeH(this, TEXT("Poplar")); Poplar->SetupAttachment(RootComponent);
	Conifer = MakeH(this, TEXT("Conifer")); Conifer->SetupAttachment(RootComponent);
}

void ASiteTrees::SetTrees(int32 Kind, const TArray<float>& D)
{
	UHierarchicalInstancedStaticMeshComponent* C = Kind == 0 ? Broadleaf : Kind == 1 ? Small : Kind == 2 ? Poplar : Conifer;
	C->ClearInstances();
	TArray<FTransform> T; T.Reserve(D.Num() / 5);
	for (int32 i = 0; i + 4 < D.Num(); i += 5)
	{
		const float Yaw = FMath::Frac(D[i] * 0.0137f + D[i + 1] * 0.0071f) * 360.f;
		T.Add(FTransform(FRotator(0, Yaw, 0), FVector(D[i], D[i + 1], D[i + 2]), FVector(D[i + 3] / 100.f, D[i + 3] / 100.f, D[i + 4] / 100.f)));
	}
	C->AddInstances(T, false, true);
}
