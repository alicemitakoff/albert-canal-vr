// Viewer pawn for the Albert Canal battery swap: works with a Meta Quest (OpenXR) and on a laptop.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SwapPawn.generated.h"

class UCameraComponent;

UCLASS()
class ALBERTCANALSWAP_API ASwapPawn : public APawn
{
	GENERATED_BODY()
public:
	ASwapPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) USceneComponent* Origin;
	UPROPERTY(VisibleAnywhere) UCameraComponent* Camera;

	/** True when a headset is driving the camera. */
	bool IsVR() const;
	/** Desktop look direction (ignored in VR, where the head turns freely). */
	void SetDesktopView(float Yaw, float Pitch) { DeskYaw = Yaw; DeskPitch = Pitch; }

	bool bContinuePressed = false;
	bool bNextPlacePressed = false;
	bool bPrevPlacePressed = false;
	bool bPausePressed = false;
	/** Movement asked for with the Quest sticks since the last frame, in world cm. ASwapExperience applies it and sets it back to zero. */
	FVector PendingMove = FVector::ZeroVector;

private:
	void OnContinue() { bContinuePressed = true; }
	void OnNextPlace() { bNextPlacePressed = true; }
	void OnPrevPlace() { bPrevPlacePressed = true; }
	void OnPause() { bPausePressed = true; }
	// Quest sticks (axis mappings in Config/DefaultInput.ini)
	void OnMoveX(float V) { AxMoveX = V; }
	void OnMoveY(float V) { AxMoveY = V; }
	void OnTurn(float V) { AxTurn = V; }
	void OnUp(float V) { AxUp = V; }
	void OnFast(float V) { AxFast = V; }
	float AxMoveX = 0.f, AxMoveY = 0.f, AxTurn = 0.f, AxUp = 0.f, AxFast = 0.f;
	float DeskYaw = 0.f, DeskPitch = -8.f;
};
