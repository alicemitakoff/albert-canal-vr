// Holds the 30,000 surveyed trees of the site as four instanced meshes (filled from Python at setup time).
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SiteTrees.generated.h"
class UHierarchicalInstancedStaticMeshComponent;
UCLASS()
class ALBERTCANALSWAP_API ASiteTrees : public AActor
{
	GENERATED_BODY()
public:
	ASiteTrees();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UHierarchicalInstancedStaticMeshComponent* Broadleaf;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UHierarchicalInstancedStaticMeshComponent* Small;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UHierarchicalInstancedStaticMeshComponent* Poplar;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UHierarchicalInstancedStaticMeshComponent* Conifer;
	/** Python helper: replaces all instances of one kind (0 broadleaf, 1 small, 2 poplar, 3 conifer). Each entry is X, Y, Z, width, height in cm. */
	UFUNCTION(BlueprintCallable, CallInEditor) void SetTrees(int32 Kind, const TArray<float>& Data);
};
