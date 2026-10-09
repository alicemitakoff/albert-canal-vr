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
}

void ASwapPawn::Tick(float Dt)
{
	Super::Tick(Dt);
	if (IsVR()) return;
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
