#include "SwapPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"

ASwapPawn::ASwapPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Origin = CreateDefaultSubobject<USceneComponent>(TEXT("Origin"));
	RootComponent = Origin;
	Origin->SetMobility(EComponentMobility::Movable);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Origin);
	Camera->bLockToHmd = true;
	Camera->SetFieldOfView(70.f);
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

bool ASwapPawn::IsVR() const
{
	return GEngine && GEngine->XRSystem.IsValid() && UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
}

void ASwapPawn::BeginPlay()
{
	Super::BeginPlay();
	if (IsVR())
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
		Camera->SetRelativeLocation(FVector::ZeroVector);
	}
	else
	{
		Camera->SetRelativeLocation(FVector(0, 0, 165));   // eye height of a standing adult
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
	}
}

void ASwapPawn::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAction(TEXT("Continue"), IE_Pressed, this, &ASwapPawn::OnContinue);
	In->BindAction(TEXT("NextPlace"), IE_Pressed, this, &ASwapPawn::OnNextPlace);
	In->BindAction(TEXT("PrevPlace"), IE_Pressed, this, &ASwapPawn::OnPrevPlace);
	In->BindAction(TEXT("Pause"), IE_Pressed, this, &ASwapPawn::OnPause);
	In->BindAxis(TEXT("VRMoveX"), this, &ASwapPawn::OnMoveX);
	In->BindAxis(TEXT("VRMoveY"), this, &ASwapPawn::OnMoveY);
	In->BindAxis(TEXT("VRTurn"), this, &ASwapPawn::OnTurn);
	In->BindAxis(TEXT("VRUp"), this, &ASwapPawn::OnUp);
	In->BindAxis(TEXT("VRFast"), this, &ASwapPawn::OnFast);
}

void ASwapPawn::Tick(float Dt)
{
	Super::Tick(Dt);
	if (IsVR())
	{
		// Quest: the left stick walks or flies where you look (hold the left grip to go fast),
		// the right stick turns you smoothly (left and right) and lifts you up or brings you down (up and down)
		auto DeadZone = [](float x) { return FMath::Abs(x) < 0.15f ? 0.f : (x - FMath::Sign(x) * 0.15f) / 0.85f; };
		const float mx = DeadZone(AxMoveX), my = DeadZone(AxMoveY), tx = DeadZone(AxTurn), ty = DeadZone(AxUp);
		if (mx != 0.f || my != 0.f)
		{
			FVector F = Camera->GetForwardVector(); F.Z = 0; F = F.GetSafeNormal();
			const FVector Right = FVector::CrossProduct(FVector::UpVector, F);
			const float Step = (AxFast > 0.5f ? 2200.f : 450.f) * Dt;   // 4.5 m/s walking, 22 m/s fast
			PendingMove += (F * my + Right * mx) * Step;
		}
		if (ty != 0.f) PendingMove.Z += ty * 400.f * Dt;
		if (tx != 0.f)
		{
			// turn around the head, not around the middle of the play space
			const FVector Before = Camera->GetComponentLocation();
			AddActorWorldRotation(FRotator(0.f, tx * 75.f * Dt, 0.f));
			PendingMove += Before - Camera->GetComponentLocation();
		}
		return;
	}
	// laptop: hold the right mouse button and move the mouse to look around
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float dx = 0, dy = 0;
		if (PC->IsInputKeyDown(EKeys::RightMouseButton)) { PC->GetInputMouseDelta(dx, dy); }
		if (PC->IsInputKeyDown(EKeys::Left)) dx -= 60.f * Dt;
		if (PC->IsInputKeyDown(EKeys::Right)) dx += 60.f * Dt;
		if (PC->IsInputKeyDown(EKeys::Up)) dy += 40.f * Dt;
		if (PC->IsInputKeyDown(EKeys::Down)) dy -= 40.f * Dt;
		DeskYaw += dx * 1.0f;
		DeskPitch = FMath::Clamp(DeskPitch + dy * 1.0f, -80.f, 60.f);
	}
	Camera->SetRelativeRotation(FRotator(DeskPitch, DeskYaw, 0));
}
