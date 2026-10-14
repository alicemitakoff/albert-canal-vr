// The guided battery swap story at BCTN Meerhout and Kwaadmechelen lock, ported from the WebXR version.
// One actor drives the barge, the swap crane, the battery packs, the lock, the captions, the live data and the viewpoints.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "SwapExperience.generated.h"

class UTextRenderComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class ASwapPawn;

USTRUCT()
struct FSwapKey { GENERATED_BODY() float T = 0; FVector P = FVector::ZeroVector; float Yaw = 0; };

UCLASS()
class ALBERTCANALSWAP_API ASwapExperience : public AActor
{
	GENERATED_BODY()
public:
	ASwapExperience();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- scene references (set once from the editor or by the setup script) ----
	UPROPERTY(EditAnywhere, Category="Scene") AActor* Barge = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* Hub = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* Lock = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* CraneStructure = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* CraneTrolley = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* CraneSpreader = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") TArray<AActor*> CraneSigns;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* GateDown = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* GateUp = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* ChamberWater = nullptr;
	/** Pack A (spent): on the barge, on the crane, in the docking station. */
	UPROPERTY(EditAnywhere, Category="Scene") AActor* PackA_Barge = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* PackA_Crane = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* PackA_Dock = nullptr;
	/** Pack C (charged): in the docking station, on the crane, on the barge. */
	UPROPERTY(EditAnywhere, Category="Scene") AActor* PackC_Hub = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* PackC_Crane = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") AActor* PackC_Barge = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") TArray<AActor*> SparePacks;
	/** Canal centreline in Unreal coordinates, every 100 m from KmStart. */
	UPROPERTY(EditAnywhere, Category="Scene") TArray<FVector> CanalPoints;
	UPROPERTY(EditAnywhere, Category="Scene") float KmStart = 48.f;
	UPROPERTY(EditAnywhere, Category="Scene") UMaterialInterface* ColourMaterial = nullptr;
	UPROPERTY(EditAnywhere, Category="Scene") UMaterialInterface* GlowMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Story") float Speed = 1.f;
	UPROPERTY(EditAnywhere, Category="Story") bool bGuided = true;
	/** Story time to preview in the editor (seconds). */
	UPROPERTY(EditAnywhere, Category="Story") float PreviewTime = 0.f;
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Story") void PreviewInEditor();
	/** Same as pressing A on the headset or Space on the laptop. */
	UFUNCTION(BlueprintCallable, Category="Story") void Continue();
	/** Jump to a story time in seconds (used by the tests and for screenshots). */
	UFUNCTION(BlueprintCallable, Category="Story") void SeekTo(float Seconds);
	/** 0 wheelhouse, 1 quay, 2 lock wall, 3 drone following the barge, 4 high above the canal. In VR only 3 and 4 are used. */
	UFUNCTION(BlueprintCallable, Category="Story") void SetPlace(int32 Index);
	UFUNCTION(BlueprintPure, Category="Story") float GetStoryTime() const { return T; }

private:
	// timeline
	float T = 0.f; int32 CpNext = 0; int32 Waiting = -1; bool bEnded = false; bool bPaused = false;
	TArray<FSwapKey> BK; struct FSK { float T; FVector V; }; TArray<FSK> SK;
	FVector BaseTrolley, BaseGantry, BaseGD, BaseGU, BaseWC; TArray<FVector> BaseSigns;
	bool bBuilt = false;
	void BuildKeys();
	void ApplyTime(float Time);
	float KmOf(const FVector& P) const;
	FVector KmPoint(float Km) const; FVector2D KmTangent(float Km) const;
	FSwapKey BargeAt(float Km, float LatN) const; FSwapKey HubPose(float X, float Y) const; FSwapKey LockAt(float XL, float Z) const;
	FSwapKey KeyAt(float Time) const;

	// departure from the quay into the lock, worked out once: x, y in cm and the yaw in degrees for every 0.05 s of story time
	TArray<FVector> Dep;
	void BuildDeparture();

	// changes that bring the scene in line with the web version (14 Oct 2026)
	bool bLayoutDone = false;
	void HideOldParts();      // first container row, old battery bay, bollards, masts, anchors, the old lift gates and their towers
	void MoveBatteryBay();    // the packs sit one container row further forward, in an open bay on the deck (game only)
	UPROPERTY() UStaticMesh* CubeMesh = nullptr;
	UPROPERTY() UStaticMesh* CylMesh = nullptr;
	UPROPERTY() UMaterialInterface* LitMaterial = nullptr;
	UStaticMeshComponent* MakeBlock(USceneComponent* Parent, const FVector& Centre, const FVector& SizeCm, const FLinearColor& Colour, bool bCylinder = false);
	UPROPERTY() TArray<UStaticMeshComponent*> BayParts;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> BlockMats;
	TArray<FLinearColor> BlockCols;

	// mitre gates: two doors per gate that swing open, built from simple blocks
	struct FLeaf { USceneComponent* Pivot = nullptr; FVector Hinge = FVector::ZeroVector; float Side = 1.f; int32 Gate = 0; };
	TArray<FLeaf> Leaves;
	UPROPERTY() TArray<USceneComponent*> LeafPivots;
	void BuildMitreGates();
	void SetMitre(int32 Gate, float Open);

	// the wake behind the barge
	UPROPERTY() UStaticMeshComponent* WakeL = nullptr;
	UPROPERTY() UStaticMeshComponent* WakeR = nullptr;
	UPROPERTY() UStaticMeshComponent* WakeT = nullptr;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> WakeMats;
	FVector LastBargePos = FVector::ZeroVector; float LastWakeT = -1.f; float WakeK = 0.f;
	void UpdateWake();

	// viewer
	ASwapPawn* Viewer = nullptr; int32 Spot = 0; void ApplySpot(bool bSnapPanel);
	FVector VrOff = FVector::ZeroVector;   // how far the viewer has moved away from the drone's spot with the sticks
	void ChangePlace(int32 Step);
	void UpdateDrone(float Dt);

	// panel
	UPROPERTY() USceneComponent* Panel = nullptr;
	UPROPERTY() UStaticMeshComponent* PanelBg = nullptr;
	UPROPERTY() UTextRenderComponent* TxtStep = nullptr;
	UPROPERTY() UTextRenderComponent* TxtTitle = nullptr;
	UPROPERTY() UTextRenderComponent* TxtBody = nullptr;
	UPROPERTY() UTextRenderComponent* TxtPrompt = nullptr;
	UPROPERTY() UTextRenderComponent* TxtLive = nullptr;
	UPROPERTY() UTextRenderComponent* TxtBatt = nullptr;
	UPROPERTY() UStaticMeshComponent* BarBg = nullptr;
	UPROPERTY() UStaticMeshComponent* BarFill = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* MBar = nullptr;
	int32 LastChapter = -1; FString LastPrompt;
	void AttachPanel();
	void UpdatePanel();

	// battery highlights
	struct FFx { UStaticMeshComponent* Box = nullptr; UStaticMeshComponent* Beam = nullptr; UTextRenderComponent* Tag = nullptr; UMaterialInstanceDynamic* MBox = nullptr; UMaterialInstanceDynamic* MBeam = nullptr; };
	FFx FxA, FxC; TArray<FFx> FxSpare;
	FFx MakeFx(const TCHAR* Name, bool bBeam);
	void PlaceFx(FFx& F, AActor* Pack, const FLinearColor& C, float Pct, const FString& Name, const FString& State, bool bBeam, bool bTag);
	float PackAPct() const; float PackCPct() const; float SocAt() const; FString PackAState() const;

	// live data
	float LiveTimer = 0.f; float Temp = -1e9f, Wind = -1e9f, Dir = -1e9f, Cloud = -1e9f, Rain = 0, WindMW = -1e9f, SolarMW = -1e9f, LoadMW = -1e9f; int32 Vessels = -1; FString LiveTime;
	void RefreshLive();
	void GetJson(const FString& Url, TFunction<void(TSharedPtr<class FJsonObject>)> Done);
	FString LiveLine() const;
	TArray<AActor*> Rotors;
};
