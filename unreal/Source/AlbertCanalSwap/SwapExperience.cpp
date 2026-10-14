#include "SwapExperience.h"
#include "SwapPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

// ------------------------------------------------------------------ story text (same as the web version)
struct FChapter { float T; const TCHAR* H; const TCHAR* B; };
static const FChapter CH[] = {
	{0,    TEXT("Under way on battery power"), TEXT("The CMA CGM e-barge heads east from Antwerp with 180 TEU. It runs on one leased 20 ft battery pack of 1.96 MWh, now down to about 44% after 50 km.")},
	{8,    TEXT("Approaching BCTN Meerhout"), TEXT("The swap point is the existing BCTN container terminal at Meerhout, canal km 51.9. NatPower Marine adds a charging station at the east end of its 330 m quay.")},
	{13.5f,TEXT("Coming alongside"), TEXT("The barge berths bow east, its battery bay lined up under the swap crane, so it can carry straight on to the lock afterwards.")},
	{19,   TEXT("Lifting out the spent pack"), TEXT("The crane locks onto pack A in its bay on the deck and lifts it clear of the barge.")},
	{23.5f,TEXT("Spent pack to the charger"), TEXT("Pack A crosses the quay to the docking station.")},
	{29.2f,TEXT("Charging ashore"), TEXT("Grid power, the solar canopy and on-site storage recharge pack A in about 2.5 hours, ready for the next barge. Operators lease packs as a service and never own them.")},
	{30.8f,TEXT("Collecting a charged pack"), TEXT("The crane picks up pack C, already full.")},
	{37.8f,TEXT("Charged pack on board"), TEXT("Pack C drops into the same slot and connects to the fixed power, data and fire-safety lines.")},
	{44,   TEXT("Swap complete"), TEXT("Around 30 minutes alongside instead of hours on a charger. The barge casts off with a full pack.")},
	{51.5f,TEXT("Heading for the lock"), TEXT("Kwaadmechelen lock is 400 m upstream. The two doors of the lower gate of the push tow chamber stand open.")},
	{70,   TEXT("Into the chamber"), TEXT("The barge passes between the open doors into the 200 m chamber.")},
	{79,   TEXT("Lower gate closes"), TEXT("The two doors swing shut behind the stern and meet in a V that holds back the water.")},
	{84,   TEXT("Rising ten metres"), TEXT("Water flows in until the chamber matches the upper reach, 10.1 m higher. The barge rises with it.")},
	{104,  TEXT("Upper gate opens"), TEXT("With the levels equal, the two doors of the upper gate swing open.")},
	{110,  TEXT("On to Genk"), TEXT("36 km and four more locks to the Genk east hub at Genk Cargo Connect, with a full pack and a healthy reserve.")}};
static const int32 NCH = UE_ARRAY_COUNT(CH);
struct FCheck { float T; const TCHAR* Btn; const TCHAR* P; };
static const FCheck CP[] = {
	{0,    TEXT("Start the journey"), TEXT("The e-barge is 1.2 km downstream of the swap hub. Pack A, its only battery, is down to 44%.")},
	{17,   TEXT("Start the battery swap"), TEXT("The barge is alongside with its battery bay right under the swap crane. Pack A is at 43%.")},
	{29.3f,TEXT("Bring the charged pack"), TEXT("Pack A is on the charger ashore. Pack C, fully charged, is waiting in the docking station.")},
	{44.2f,TEXT("Cast off for the lock"), TEXT("Pack C is connected. The barge leaves with a full battery after about 30 minutes alongside.")},
	{79,   TEXT("Close the lower gate"), TEXT("The barge is inside the 200 m push-tow chamber of Kwaadmechelen lock.")},
	{84,   TEXT("Fill the chamber"), TEXT("The chamber is sealed. Filling it lifts the barge 10.1 m to the upper reach.")},
	{104,  TEXT("Open the upper gate"), TEXT("The water in the chamber now matches the upper reach.")}};
static const int32 NCP = UE_ARRAY_COUNT(CP);
static const float T_END = 126.f, KEEL = -300.f, SOC_KM = 1.117f;
static const TCHAR* SpotNames[] = { TEXT("On the wheelhouse roof"), TEXT("On the quay by the swap crane"), TEXT("On the lock wall"), TEXT("Drone, following the barge"), TEXT("High above the canal") };
static const int32 NSPOT = 5;

// the trip from the quay into the lock (worked out once, see BuildDeparture) and the faster passages
static const double DEP_T0 = 46.5, DEP_T1 = 79.0, DEP_DT = 0.05, HULL = 133.0, T_SW = 53.0;
// mitre gates in the lock frame (metres): hinge line X, sill height, chamber centre Y, half width, door height, and the old lift gate it replaces
struct FGateSpec { float X, Bottom, Y, HalfW, H; const TCHAR* Old; };
static const FGateSpec GATES[] = {
	{-109.1f, -4.2f, 0.f, 12.4f, 15.8f, TEXT("Gate_PushTow_Down")}, {108.5f, 5.4f, 0.f, 12.4f, 6.2f, TEXT("Gate_PushTow_Up")},
	{-113.5f, -4.2f, -60.f, 8.4f, 15.8f, TEXT("Gate_Middle_Down")}, {38.3f, 5.4f, -60.f, 8.4f, 6.2f, TEXT("Gate_Middle_Up")},
	{-114.1f, -4.2f, -91.4f, 8.4f, 15.8f, TEXT("Gate_North_Down")}, {37.4f, 5.4f, -91.4f, 8.4f, 6.2f, TEXT("Gate_North_Up")} };
static const int32 NGATES = UE_ARRAY_COUNT(GATES);
static const float MITRE_DEG = 20.f;   // each door sits 20 degrees off square when closed, so the two doors meet in a V pointing upstream

// colours: red at 20% or less, orange below 60%, green from 60%
static FLinearColor PctColour(float P) { return P <= 20 ? FLinearColor(1.f, 0.045f, 0.03f) : P < 60 ? FLinearColor(1.f, 0.22f, 0.0f) : FLinearColor(0.016f, 0.87f, 0.27f); }
static FColor PctFColor(float P) { return P <= 20 ? FColor(255, 59, 48) : P < 60 ? FColor(255, 178, 30) : FColor(34, 240, 143); }

static FString Wrap(const FString& S, int32 Max)
{
	TArray<FString> W; S.ParseIntoArray(W, TEXT(" "));
	FString Out, Line;
	for (const FString& w : W) { if (Line.Len() + w.Len() + 1 > Max && Line.Len()) { Out += Line + TEXT("<br>"); Line = w; } else { Line += (Line.Len() ? TEXT(" ") : TEXT("")) + w; } }
	return Out + Line;
}
static float Hermite(const TArray<float>& Ts, const TArray<float>& V, float t)
{
	const int32 L = Ts.Num(); if (t <= Ts[0]) return V[0]; if (t >= Ts[L - 1]) return V[L - 1];
	int32 i = 0; while (Ts[i + 1] < t) i++;
	const float dt = Ts[i + 1] - Ts[i], u = (t - Ts[i]) / dt;
	auto Tan = [&](int32 j) -> float { if (j <= 0 || j >= L - 1) return 0; const float a = V[j - 1], b = V[j], c = V[j + 1];
		if (FMath::Abs(b - a) < 1e-4f || FMath::Abs(c - b) < 1e-4f) return 0; if ((b - a) * (c - b) < 0) return 0; return (c - a) / (Ts[j + 1] - Ts[j - 1]); };
	const float m0 = Tan(i) * dt, m1 = Tan(i + 1) * dt, u2 = u * u, u3 = u2 * u;
	return (2 * u3 - 3 * u2 + 1) * V[i] + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * V[i + 1] + (u3 - u2) * m1;
}
static void Show(AActor* A, bool b) { if (!A) return; A->SetActorHiddenInGame(!b);
#if WITH_EDITOR
	A->SetIsTemporarilyHiddenInEditor(!b);
#endif
}
static double Smooth01(double x) { x = FMath::Clamp(x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }
static double Unwrap(double Deg, double Ref) { while (Deg - Ref > 180.0) Deg -= 360.0; while (Deg - Ref < -180.0) Deg += 360.0; return Deg; }
// every name a part can be known by: the actor name, its label in the editor and the names of its meshes
static TArray<FString> NamesOf(AActor* A)
{
	TArray<FString> N; if (!A) return N;
	N.Add(A->GetName());
#if WITH_EDITOR
	N.Add(A->GetActorLabel());
#endif
	TArray<UStaticMeshComponent*> Ms; A->GetComponents<UStaticMeshComponent>(Ms);
	for (UStaticMeshComponent* M : Ms) if (M && M->GetStaticMesh()) N.Add(M->GetStaticMesh()->GetName());
	return N;
}
static bool NameMatches(AActor* A, const TArray<FString>& Exact, const TArray<FString>& Starts)
{
	for (const FString& N : NamesOf(A))
	{
		if (Exact.Contains(N)) return true;
		for (const FString& S : Starts) if (N.StartsWith(S, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}
// the box around an actor's meshes, measured in another actor's frame (cm)
static FBox LocalBox(AActor* A, const FTransform& Frame)
{
	FBox Out(ForceInit); if (!A) return Out;
	TArray<UStaticMeshComponent*> Ms; A->GetComponents<UStaticMeshComponent>(Ms);
	for (UStaticMeshComponent* M : Ms)
	{
		if (!M || !M->GetStaticMesh()) continue;
		const FBox B = M->GetStaticMesh()->GetBoundingBox(); const FTransform CT = M->GetComponentTransform();
		for (int32 i = 0; i < 8; i++)
			Out += Frame.InverseTransformPosition(CT.TransformPosition(FVector((i & 1) ? B.Max.X : B.Min.X, (i & 2) ? B.Max.Y : B.Min.Y, (i & 4) ? B.Max.Z : B.Min.Z)));
	}
	return Out;
}

ASwapExperience::ASwapExperience()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Col(TEXT("/Game/Site/Materials/M_UnlitColour.M_UnlitColour"));
	if (Col.Succeeded()) ColourMaterial = Col.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glw(TEXT("/Game/Site/Materials/M_Glow.M_Glow"));
	if (Glw.Succeeded()) GlowMaterial = Glw.Object;
	// plain lit material from the engine, used for the mitre gates and the battery bay (its colour parameter is called Color)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lit(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Lit.Succeeded()) LitMaterial = Lit.Object;
	if (Cube.Succeeded()) CubeMesh = Cube.Object;
	if (Cyl.Succeeded()) CylMesh = Cyl.Object;
	auto Mesh = [&](const TCHAR* N, UStaticMesh* M, USceneComponent* Parent) {
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(N); C->SetupAttachment(Parent); C->SetStaticMesh(M);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->SetMobility(EComponentMobility::Movable); return C; };
	auto Text = [&](const TCHAR* N, float Size, FColor Colour) {
		UTextRenderComponent* C = CreateDefaultSubobject<UTextRenderComponent>(N); C->SetupAttachment(Panel); C->SetWorldSize(Size);
		C->SetHorizontalAlignment(EHTA_Left); C->SetVerticalAlignment(EVRTA_TextTop); C->SetTextRenderColor(Colour); C->SetCastShadow(false); return C; };
	Panel = CreateDefaultSubobject<USceneComponent>(TEXT("Panel")); Panel->SetupAttachment(RootComponent);
	PanelBg = Mesh(TEXT("PanelBg"), Cube.Object, Panel); PanelBg->SetRelativeLocation(FVector(-1.5f, 0, 0)); PanelBg->SetRelativeScale3D(FVector(0.01f, 1.16f, 0.70f));
	TxtStep = Text(TEXT("Step"), 2.6f, FColor(140, 200, 255));     TxtStep->SetRelativeLocation(FVector(0, 54, 31));
	TxtTitle = Text(TEXT("Title"), 5.0f, FColor::White);            TxtTitle->SetRelativeLocation(FVector(0, 54, 27));
	TxtBody = Text(TEXT("Body"), 3.1f, FColor(225, 232, 240));      TxtBody->SetRelativeLocation(FVector(0, 54, 19.5f));
	TxtBatt = Text(TEXT("Batt"), 2.8f, FColor(225, 232, 240));      TxtBatt->SetRelativeLocation(FVector(0, 54, -7));
	TxtPrompt = Text(TEXT("Prompt"), 3.4f, FColor(255, 214, 120));  TxtPrompt->SetRelativeLocation(FVector(0, 54, -17));
	TxtLive = Text(TEXT("Live"), 2.3f, FColor(160, 190, 215));      TxtLive->SetRelativeLocation(FVector(0, 54, -28));
	BarBg = Mesh(TEXT("BarBg"), Cube.Object, Panel);   BarBg->SetRelativeLocation(FVector(-0.6f, 24, -13)); BarBg->SetRelativeScale3D(FVector(0.004f, 0.6f, 0.024f));
	BarFill = Mesh(TEXT("BarFill"), Cube.Object, Panel); BarFill->SetRelativeScale3D(FVector(0.006f, 0.3f, 0.012f));
	// translucent parts do not write depth, so draw order decides what is on top
	PanelBg->SetTranslucentSortPriority(0); BarBg->SetTranslucentSortPriority(1); BarFill->SetTranslucentSortPriority(2);
	// battery highlights
	auto Fx = [&](const TCHAR* N, bool bBeam) { FFx F; F.Box = Mesh(*FString::Printf(TEXT("%sBox"), N), Cube.Object, RootComponent);
		if (bBeam) F.Beam = Mesh(*FString::Printf(TEXT("%sBeam"), N), Cyl.Object, RootComponent);
		F.Tag = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("%sTag"), N)); F.Tag->SetupAttachment(RootComponent);
		F.Tag->SetHorizontalAlignment(EHTA_Center); F.Tag->SetVerticalAlignment(EVRTA_TextBottom); F.Tag->SetCastShadow(false); return F; };
	FxA = Fx(TEXT("PackA"), true); FxC = Fx(TEXT("PackC"), true);
	FxSpare.Add(Fx(TEXT("Spare1"), false)); FxSpare.Add(Fx(TEXT("Spare2"), false));
	// the wake: two arms that leave the bow in a V and a band of foam behind the stern
	WakeL = Mesh(TEXT("WakeL"), Cube.Object, RootComponent); WakeR = Mesh(TEXT("WakeR"), Cube.Object, RootComponent); WakeT = Mesh(TEXT("WakeT"), Cube.Object, RootComponent);
	WakeL->SetVisibility(false); WakeR->SetVisibility(false); WakeT->SetVisibility(false);
}

// ------------------------------------------------------------------ canal helpers
FVector ASwapExperience::KmPoint(float Km) const
{
	if (CanalPoints.Num() < 2) return FVector::ZeroVector;
	const float f = FMath::Clamp((Km - KmStart) / 0.1f, 0.f, CanalPoints.Num() - 1.001f); const int32 i = FMath::FloorToInt(f);
	return FMath::Lerp(CanalPoints[i], CanalPoints[i + 1], f - i);
}
FVector2D ASwapExperience::KmTangent(float Km) const
{
	const FVector a = KmPoint(Km - 0.05f), b = KmPoint(Km + 0.05f); return FVector2D(b.X - a.X, b.Y - a.Y).GetSafeNormal();
}
float ASwapExperience::KmOf(const FVector& P) const
{
	float Best = 1e30f, Km = KmStart;
	for (int32 i = 0; i + 1 < CanalPoints.Num(); i++)
	{
		const FVector2D A(CanalPoints[i]), B(CanalPoints[i + 1]), Q(P); const FVector2D AB = B - A;
		const float u = FMath::Clamp(FVector2D::DotProduct(Q - A, AB) / AB.SizeSquared(), 0.f, 1.f); const float d = FVector2D::DistSquared(A + AB * u, Q);
		if (d < Best) { Best = d; Km = KmStart + (i + u) * 0.1f; }
	}
	return Km;
}
FSwapKey ASwapExperience::BargeAt(float Km, float LatN) const
{
	const FVector c = KmPoint(Km); const FVector2D t = KmTangent(Km); const FVector n(t.Y, -t.X, 0);
	FSwapKey K; K.P = c + n * LatN * 100.f; K.P.Z = KEEL; K.Yaw = FMath::RadiansToDegrees(FMath::Atan2(t.Y, t.X)) + 90.f; return K;
}
FSwapKey ASwapExperience::HubPose(float X, float Y) const
{
	FSwapKey K; K.P = Hub->GetActorTransform().TransformPosition(FVector(X * 100.f, Y * 100.f, 0)); K.P.Z = KEEL; K.Yaw = Hub->GetActorRotation().Yaw - 90.f; return K;
}
FSwapKey ASwapExperience::LockAt(float XL, float Z) const
{
	FSwapKey K; K.P = Lock->GetActorTransform().TransformPosition(FVector(XL * 100.f, 0, 0)); K.P.Z = Z; K.Yaw = Lock->GetActorRotation().Yaw + 90.f; return K;
}

FSwapKey ASwapExperience::KeyAt(float t) const
{
	FSwapKey K; K.T = t; if (BK.Num() == 0) return K;
	TArray<float> Ts, Vx, Vy, Vz, Vr;
	for (const FSwapKey& k : BK) { Ts.Add(k.T); Vx.Add(k.P.X); Vy.Add(k.P.Y); Vz.Add(k.P.Z); Vr.Add(k.Yaw); }
	K.P = FVector(Hermite(Ts, Vx, t), Hermite(Ts, Vy, t), Hermite(Ts, Vz, t)); K.Yaw = Hermite(Ts, Vr, t);
	return K;
}

void ASwapExperience::BuildKeys()
{
	if (!Barge || !Hub || !Lock || CanalPoints.Num() < 2) return;
	HideOldParts();
	BuildMitreGates();
	const FSwapKey Dock = HubPose(21.4f, 6.9f);
	struct R { float T; FSwapKey K; };
	TArray<R> Raw = { {0, BargeAt(50.72f, -24)}, {8, BargeAt(51.45f, -30)}, {13.5f, HubPose(78, 15)}, {17, Dock}, {46.5f, Dock}, {51.5f, HubPose(-24, 21)},
		{60, LockAt(-300, KEEL)}, {70, LockAt(-235, KEEL)}, {79, LockAt(-85, KEEL)}, {84, LockAt(-85, KEEL)}, {104, LockAt(-85, KEEL + 1000)}, {110, LockAt(-85, KEEL + 1000)}, {125, LockAt(330, KEEL + 1000)} };
	BK.Reset(); float Prev = 0; bool bFirst = true;
	for (R& r : Raw) { float y = r.K.Yaw; if (!bFirst) { while (y - Prev > 180) y -= 360; while (y - Prev < -180) y += 360; } Prev = y; bFirst = false; FSwapKey k = r.K; k.T = r.T; k.Yaw = y; BK.Add(k); }
	// pack A seen from the hub while the barge is alongside: where the crane has to pick it up
	FVector aL(0, 0, 0);
	if (PackA_Barge)
	{
		const FTransform Rel = PackA_Barge->GetActorTransform().GetRelativeTransform(Barge->GetActorTransform());
		const FTransform DockT(FRotator(0, Dock.Yaw, 0), Dock.P);
		aL = Hub->GetActorTransform().InverseTransformPosition((Rel * DockT).GetLocation());
	}
	const float pz = aL.Z + 259.1f + 0.5f, X = 1e9f;
	const float S[][4] = { {17,0,640,680},{19,X,X,1400},{21,X,X,-X},{21.6f,X,X,-X},{23.5f,X,X,1400},{26.5f,1350,-2400,1400},{28.5f,1350,-2400,494.6f},{29.2f,1350,-2400,494.6f},
		{30.8f,1350,-2400,1400},{33.5f,-1350,-2400,1400},{35.5f,-1350,-2400,494.6f},{36.2f,-1350,-2400,494.6f},{37.8f,-1350,-2400,1400},{41,X,X,1400},{43,X,X,-X},{43.6f,X,X,-X},{45,X,X,1400},{47,0,640,1400} };
	SK.Reset();
	for (auto& s : S) SK.Add({ s[0], FVector(s[1] == X ? aL.X : s[1], s[2] == X ? aL.Y : s[2], s[3] == -X ? pz : s[3]) });
	if (CraneTrolley) BaseTrolley = CraneTrolley->GetRootComponent()->GetRelativeLocation();
	if (CraneStructure) BaseGantry = FVector(0, CraneStructure->GetRootComponent()->GetRelativeLocation().Y, CraneStructure->GetRootComponent()->GetRelativeLocation().Z);
	BaseSigns.Reset(); for (AActor* A : CraneSigns) BaseSigns.Add(A ? FVector(0, A->GetRootComponent()->GetRelativeLocation().Y, A->GetRootComponent()->GetRelativeLocation().Z) : FVector::ZeroVector);
	if (GateDown) { BaseGD = GateDown->GetRootComponent()->GetRelativeLocation(); BaseGD.Z = -420; }
	if (GateUp) { BaseGU = GateUp->GetRootComponent()->GetRelativeLocation(); BaseGU.Z = 660; }
	if (ChamberWater) { BaseWC = ChamberWater->GetRootComponent()->GetRelativeLocation(); BaseWC.Z = 0; }
	BuildDeparture();
	bBuilt = true;
}

// ------------------------------------------------------------------ departure from the quay
// Worked out once: the bow swings out over the stern first, then the bow runs along the lock centreline and the stern
// trails one hull length behind it, so the hull lines up with the guide walls before it reaches them (same as the web version).
void ASwapExperience::BuildDeparture()
{
	Dep.Reset(); if (!Lock || BK.Num() < 2) return;
	const FTransform LT = Lock->GetActorTransform();
	auto ToL = [&](const FVector2D& W) { const FVector v = LT.InverseTransformPosition(FVector(W.X, W.Y, 0.0)); return FVector2D(v.X / 100.0, v.Y / 100.0); };   // world cm to lock metres
	auto ToW = [&](const FVector2D& L) { const FVector w = LT.TransformPosition(FVector(L.X * 100.0, L.Y * 100.0, 0.0)); return FVector2D(w.X, w.Y); };       // lock metres to world cm
	auto Fwd = [](double YawDeg) { const double r = FMath::DegreesToRadians(YawDeg); return FVector2D(FMath::Sin(r), -FMath::Cos(r)); };                 // the bow is along the barge's -Y
	auto YawOf = [](const FVector2D& D) { return FMath::RadiansToDegrees(FMath::Atan2(D.X, -D.Y)); };
	const FSwapKey K0 = KeyAt((float)DEP_T0);
	const FVector2D S0(K0.P.X, K0.P.Y); const double R0 = K0.Yaw;
	const FVector2D B0 = ToL(S0 + Fwd(R0) * (HULL * 100.0));   // bow at the quay, in lock metres
	const FVector2D S0L = ToL(S0);
	FVector2D BSwing(B0.X, B0.Y * 0.38);                         // after the swing the bow is 62 % of the way to the centreline
	const double SwSternY = S0L.Y * 0.72;                        // and the stern 28 %
	const FVector2D SEnd(-85.0, 0.0), BEnd(-85.0 + HULL, 0.0);   // stern at the berth in the chamber
	FVector2D Stern = S0; double Ry = R0;
	TArray<FVector> Out;
	const int32 Steps = FMath::RoundToInt((float)((DEP_T1 - DEP_T0) / DEP_DT));
	for (int32 n = 0; n <= Steps; n++)
	{
		const double t = DEP_T0 + n * DEP_DT;
		FVector2D BowL;
		if (t <= T_SW)
		{
			const double k = Smooth01((t - DEP_T0) / (T_SW - DEP_T0)), ks = Smooth01((t - DEP_T0 - 1.2) / (T_SW - DEP_T0 - 1.2));   // the stern follows a moment later
			FVector2D Sp = S0L; Sp.X += 6.0 * ks; Sp.Y += (SwSternY - Sp.Y) * ks; Stern = ToW(Sp);
			const double Bz = B0.Y + (BSwing.Y - B0.Y) * k, Dz = Bz - Sp.Y;
			BowL = FVector2D(Sp.X + FMath::Sqrt(FMath::Max(1.0, HULL * HULL - Dz * Dz)), Bz);
			if (t + DEP_DT > T_SW) BSwing.X = BowL.X;
		}
		else
		{
			const double k = (t - T_SW) / (DEP_T1 - T_SW); const double e = k < 0.5 ? 2.0 * k * k : 1.0 - 2.0 * (1.0 - k) * (1.0 - k);   // ease in and out
			const double x = BSwing.X + (BEnd.X - BSwing.X) * e; const double y = BSwing.Y * FMath::Exp(-FMath::Max(0.0, x - BSwing.X) / 45.0);
			BowL = FVector2D(x, y);
			const FVector2D Bw = ToW(BowL); const FVector2D D = (Bw - Stern).GetSafeNormal(); Stern = Bw - D * (HULL * 100.0);   // the stern trails the bow
		}
		const FVector2D Bw = ToW(BowL); const double R = Unwrap(YawOf(Bw - Stern), Ry); Ry = R;
		Out.Add(FVector(Stern.X, Stern.Y, R));
	}
	if (Out.Num() < 2) return;
	// pin the last three seconds exactly onto the berth so the lock sequence starts where it always did
	const FVector2D Sw = ToW(SEnd); const double Rw = KeyAt((float)DEP_T1).Yaw; const FVector Last = Out.Last();
	for (int32 i = 0; i < Out.Num(); i++)
	{
		const double w = Smooth01((DEP_T0 + i * DEP_DT - (DEP_T1 - 3.0)) / 3.0);
		Out[i].X += (Sw.X - Last.X) * w; Out[i].Y += (Sw.Y - Last.Y) * w;
		const double Rr = Unwrap(Rw, Out[i].Z); Out[i].Z += (Rr - Out[i].Z) * w;
	}
	Dep = Out;
}

// ------------------------------------------------------------------ scene changes that match the web version
UStaticMeshComponent* ASwapExperience::MakeBlock(USceneComponent* Parent, const FVector& Centre, const FVector& SizeCm, const FLinearColor& Colour, bool bCylinder)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(bCylinder ? CylMesh : CubeMesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetupAttachment(Parent);
	C->RegisterComponent();
	C->SetRelativeLocation(Centre);
	C->SetRelativeScale3D(SizeCm / 100.0);   // the engine cube and cylinder are 100 cm across
	UMaterialInstanceDynamic* M = nullptr;
	for (int32 i = 0; i < BlockCols.Num(); i++) if (BlockCols[i].Equals(Colour)) { M = BlockMats[i]; break; }
	if (!M && LitMaterial) { M = UMaterialInstanceDynamic::Create(LitMaterial, this); M->SetVectorParameterValue(TEXT("Color"), Colour); BlockMats.Add(M); BlockCols.Add(Colour); }
	if (M) C->SetMaterial(0, M);
	return C;
}

void ASwapExperience::HideOldParts()
{
	TArray<FString> BargeExact; for (int32 i = 1; i <= 12; i++) BargeExact.Add(FString::Printf(TEXT("Box40_%03d"), i));   // the first row of 40 ft containers
	const TArray<FString> BargeStarts = { TEXT("Battery_bay_"), TEXT("Bollards"), TEXT("Stern_masts"), TEXT("Anchor") };
	const TArray<FString> LockStarts = { TEXT("Gate_"), TEXT("Lock_gate_towers"), TEXT("Lock_wall_fittings"), TEXT("Lock_signals") };
	const TArray<FString> NoExact;
	TArray<AActor*> Kids;
	if (Barge) { Barge->GetAttachedActors(Kids, true, true); for (AActor* A : Kids) if (NameMatches(A, BargeExact, BargeStarts)) Show(A, false); }
	if (Lock) { Lock->GetAttachedActors(Kids, true, true); for (AActor* A : Kids) if (NameMatches(A, NoExact, LockStarts)) Show(A, false); }
	Show(GateDown, false); Show(GateUp, false);
}

void ASwapExperience::MoveBatteryBay()
{
	if (!Barge || bLayoutDone) return;
	bLayoutDone = true;
	const FTransform BT = Barge->GetActorTransform();
	TArray<AActor*> Kids; Barge->GetAttachedActors(Kids, true, true);
	auto Child = [&](const TCHAR* N) -> AActor* { const FString S(N); for (AActor* A : Kids) if (A && NamesOf(A).Contains(S)) return A; return nullptr; };
	AActor* BatA = Child(TEXT("Battery_A")); AActor* BatB = Child(TEXT("Battery_B"));
	AActor* LedA = Child(TEXT("Battery_A_LED")); AActor* LedB = Child(TEXT("Battery_B_LED"));
	AActor* Ref = PackA_Barge ? PackA_Barge : BatA;
	if (!Ref) return;
	const FBox Old = LocalBox(Ref, BT);
	if (!Old.IsValid) return;
	// each pack moves one container row (9.9 m) towards the bow, centred across the barge, standing 0.5 m above the deck
	TSet<AActor*> Done;
	auto MovePack = [&](AActor* P, AActor* Led)
	{
		if (!P || Done.Contains(P)) return;
		Done.Add(P);
		const FVector L0 = BT.InverseTransformPosition(P->GetActorLocation());
		const FVector L1(0.0, L0.Y - 990.0, 500.0);
		const FVector Delta = BT.TransformVector(L1 - L0);
		P->SetActorLocation(P->GetActorLocation() + Delta);
		if (Led && !Done.Contains(Led) && !Led->IsAttachedTo(P)) { Done.Add(Led); Led->SetActorLocation(Led->GetActorLocation() + Delta); }
	};
	MovePack(BatA, LedA); MovePack(BatB, LedB); MovePack(PackA_Barge, nullptr); MovePack(PackC_Barge, nullptr);
	// the bay: a steel well with a yellow edge and a dark bearing pad, and new deck plating over the old bay
	const FBox New = LocalBox(Ref, BT);
	if (!New.IsValid) return;
	const FVector Sz = New.GetSize(), C = New.GetCenter(); const double Bottom = New.Min.Z;
	USceneComponent* Hull = Barge->GetRootComponent();
	const FLinearColor Deck(FColor(138, 148, 153)), Well(FColor(127, 141, 147)), Edge(FColor(242, 194, 0)), Pad(FColor(109, 122, 128));
	BayParts.Add(MakeBlock(Hull, FVector(0.0, Old.GetCenter().Y, 435.0), FVector(1040.0, 1040.0, 30.0), Deck));
	BayParts.Add(MakeBlock(Hull, FVector(C.X, C.Y, Bottom - 33.5), FVector(Sz.X + 140.0, Sz.Y + 140.0, 35.0), Edge));
	BayParts.Add(MakeBlock(Hull, FVector(C.X, C.Y, Bottom - 32.5), FVector(Sz.X + 90.0, Sz.Y + 90.0, 35.0), Well));
	BayParts.Add(MakeBlock(Hull, FVector(C.X, C.Y, Bottom - 9.0), FVector(Sz.X + 10.0, Sz.Y + 10.0, 12.0), Pad));
}

void ASwapExperience::BuildMitreGates()
{
	if (!Lock || LeafPivots.Num() > 0 || !CubeMesh) return;
	const FTransform LT = Lock->GetActorTransform();
	TArray<AActor*> Kids; Lock->GetAttachedActors(Kids, true, true);
	const FLinearColor Steel(FColor(75, 82, 87)), Dark(FColor(42, 46, 49)), Rail(FColor(216, 216, 212));
	for (int32 g = 0; g < NGATES; g++)
	{
		const FGateSpec& G = GATES[g];
		// use the old gate of the same chamber to find the chamber centre, so the doors land in the right chamber whatever the import did
		float Yc = G.Y;
		for (AActor* A : Kids)
		{
			if (!A || !NamesOf(A).Contains(FString(G.Old))) continue;
			FVector O, E; A->GetActorBounds(false, O, E);
			const float Y = (float)(LT.InverseTransformPosition(O).Y / 100.0);
			if (FMath::Abs(FMath::Abs(Y) - FMath::Abs(G.Y)) < 10.f) Yc = Y;
			break;
		}
		const float L = G.HalfW / FMath::Cos(FMath::DegreesToRadians(MITRE_DEG)) + 0.25f, H = G.H;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			USceneComponent* P = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
			P->SetMobility(EComponentMobility::Movable);
			P->SetupAttachment(RootComponent);
			P->RegisterComponent();
			LeafPivots.Add(P);
			FLeaf Lf; Lf.Pivot = P; Lf.Hinge = FVector(G.X * 100.f, (Yc + Side * G.HalfW) * 100.f, G.Bottom * 100.f); Lf.Side = (float)Side; Lf.Gate = g;
			Leaves.Add(Lf);
			// one door in its own frame: the hinge is at 0 and the door runs along +X
			MakeBlock(P, FVector(L / 2, 0, H / 2) * 100.f, FVector(L, 0.7f, H) * 100.f, Steel);                       // skin plate
			for (int32 k = 1; k * 4.4f < H; k++) MakeBlock(P, FVector(L / 2, 0, k * 4.4f) * 100.f, FVector(L, 1.1f, 0.35f) * 100.f, Dark);   // girders
			MakeBlock(P, FVector(L / 2, 0, H + 0.25f) * 100.f, FVector(L + 0.4f, 1.6f, 0.5f) * 100.f, Dark);          // walkway on top
			MakeBlock(P, FVector(L / 2, 0, H + 1.4f) * 100.f, FVector(L, 0.08f, 0.08f) * 100.f, Rail);                // handrail
			MakeBlock(P, FVector(0, 0, H / 2 + 0.3f) * 100.f, FVector(0.9f, 0.9f, H + 1.2f) * 100.f, Dark, true);     // heel post at the hinge
		}
	}
}

// Open 0 = closed (the doors meet in the V), 1 = open (the doors lie along the walls, pointing upstream)
void ASwapExperience::SetMitre(int32 Gate, float Open)
{
	if (!Lock) return;
	const FTransform LT = Lock->GetActorTransform(); const float o = FMath::Clamp(Open, 0.f, 1.f);
	for (const FLeaf& Lf : Leaves)
	{
		if (Lf.Gate != Gate || !Lf.Pivot) continue;
		const FRotator Local(0.f, -Lf.Side * (90.f - MITRE_DEG) * (1.f - o), 0.f);
		Lf.Pivot->SetWorldLocationAndRotation(LT.TransformPosition(Lf.Hinge), LT.TransformRotation(Local.Quaternion()));
	}
}

void ASwapExperience::UpdateWake()
{
	if (!Barge || !WakeL || !WakeR || !WakeT) return;
	const FVector P = Barge->GetActorLocation();
	// speed along the story, in metres per story second: full wake when under way, none when alongside or in the lock
	if (LastWakeT >= 0.f && FMath::Abs(T - LastWakeT) > 1e-4f)
	{
		const float V = (float)(FVector::Dist2D(P, LastBargePos) / 100.0) / FMath::Abs(T - LastWakeT);
		WakeK = FMath::FInterpTo(WakeK, FMath::Clamp(V / 25.f, 0.f, 1.f), GetWorld()->GetDeltaSeconds(), 2.f);
	}
	LastBargePos = P; LastWakeT = T;
	float K = WakeK;
	if (Lock)
	{
		const FVector L = Lock->GetActorTransform().InverseTransformPosition(P) / 100.0;
		if (FMath::Abs(L.Y) < 60.0 && L.X > -300.0 && L.X < 280.0)
			K *= (float)(L.X < -260.0 ? (-260.0 - L.X) / 40.0 : L.X > 240.0 ? (L.X - 240.0) / 40.0 : 0.0);
	}
	const bool bShow = K > 0.02f;
	WakeL->SetVisibility(bShow); WakeR->SetVisibility(bShow); WakeT->SetVisibility(bShow);
	if (!bShow) return;
	if (WakeMats.Num() == 0 && ColourMaterial)
	{
		UStaticMeshComponent* Parts[] = { WakeL, WakeR, WakeT };
		for (UStaticMeshComponent* W : Parts)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(ColourMaterial, this);
			M->SetVectorParameterValue(TEXT("Colour"), FLinearColor(0.85f, 0.92f, 0.95f)); M->SetScalarParameterValue(TEXT("Glow"), 1.f);
			W->SetMaterial(0, M); WakeMats.Add(M);
		}
	}
	for (int32 i = 0; i < WakeMats.Num(); i++) WakeMats[i]->SetScalarParameterValue(TEXT("Opacity"), (i < 2 ? 0.22f : 0.3f) * K);
	const FVector Fwd = Barge->GetActorRotation().RotateVector(FVector(0, -1, 0)); const FVector Back = -Fwd;
	const double WaterZ = P.Z - KEEL + 8.0;   // the barge origin sits 3 m below the waterline
	const FVector Bow = P + Fwd * (HULL * 100.0);
	const float Len = 6000.f * FMath::Max(0.3f, K);
	for (int32 s = 0; s < 2; s++)
	{
		const FVector ArmDir = Back.RotateAngleAxis(s ? 19.5f : -19.5f, FVector::UpVector);   // the two arms leave the bow at 19.5 degrees
		FVector C = Bow + ArmDir * (Len / 2.f); C.Z = WaterZ;
		UStaticMeshComponent* W = s ? WakeR : WakeL;
		W->SetWorldLocationAndRotation(C, ArmDir.Rotation()); W->SetWorldScale3D(FVector(Len / 100.f, 2.5f, 0.02f));
	}
	FVector C = P + Back * 3000.0; C.Z = WaterZ;
	WakeT->SetWorldLocationAndRotation(C, Back.Rotation()); WakeT->SetWorldScale3D(FVector(60.f, 9.f, 0.02f));
}

void ASwapExperience::ApplyTime(float t)
{
	if (!bBuilt) return;
	FSwapKey K = KeyAt(t);
	if (Dep.Num() > 1 && t > DEP_T0 && t < DEP_T1)   // leaving the quay and entering the lock
	{
		const double f = (t - DEP_T0) / DEP_DT; const int32 i = FMath::Clamp((int32)FMath::FloorToDouble(f), 0, Dep.Num() - 2);
		const FVector D = FMath::Lerp(Dep[i], Dep[i + 1], FMath::Clamp(f - i, 0.0, 1.0));
		K.P.X = D.X; K.P.Y = D.Y; K.Yaw = (float)D.Z;
	}
	Barge->SetActorLocationAndRotation(K.P, FRotator(0, K.Yaw, 0));
	TArray<float> St, Sx, Sy, Sz; for (const FSK& k : SK) { St.Add(k.T); Sx.Add(k.V.X); Sy.Add(k.V.Y); Sz.Add(k.V.Z); }
	const float sx = Hermite(St, Sx, t), sy = Hermite(St, Sy, t), sz = Hermite(St, Sz, t);
	if (CraneStructure) CraneStructure->SetActorRelativeLocation(BaseGantry + FVector(sx, 0, 0));
	for (int32 i = 0; i < CraneSigns.Num(); i++) if (CraneSigns[i]) CraneSigns[i]->SetActorRelativeLocation(BaseSigns[i] + FVector(sx, 0, 0));
	if (CraneTrolley) CraneTrolley->SetActorRelativeLocation(FVector(sx, sy, BaseTrolley.Z));
	if (CraneSpreader) CraneSpreader->SetActorRelativeLocation(FVector(sx, sy, sz));
	// lock: the lower mitre gate swings shut (79 to 84 s), the chamber fills (84 to 104 s), the upper gate swings open (104 to 109 s)
	SetMitre(0, Hermite({ 0, 79, 84 }, { 1, 1, 0 }, t));
	if (ChamberWater) ChamberWater->SetActorRelativeLocation(BaseWC + FVector(0, 0, 100.f * Hermite({ 0, 84, 104 }, { 0, 0, 10 }, t)));
	SetMitre(1, Hermite({ 0, 104, 109 }, { 0, 0, 1 }, t));
	for (int32 g = 2; g < NGATES; g++) SetMitre(g, 0.f);   // the two smaller chambers stay closed
	// which copy of each pack is visible
	Show(PackA_Barge, t < 21.6f); Show(PackA_Crane, t >= 21.6f && t < 29.2f); Show(PackA_Dock, t >= 29.2f);
	Show(PackC_Hub, t < 36.2f); Show(PackC_Crane, t >= 36.2f && t < 43.6f); Show(PackC_Barge, t >= 43.6f);
}

void ASwapExperience::PreviewInEditor()
{
	BuildKeys(); T = PreviewTime; ApplyTime(T);
}
void ASwapExperience::Continue()
{
	if (Waiting >= 0) { Waiting = -1; CpNext++; bPaused = false; }
	else if (bEnded) { T = 0; CpNext = 0; bEnded = false; LastChapter = -1; }
	else bPaused = !bPaused;
}
void ASwapExperience::SeekTo(float S)
{
	T = FMath::Clamp(S, 0.f, T_END - 0.01f); bEnded = false; Waiting = -1; LastChapter = -1;
	CpNext = NCP; for (int32 i = 0; i < NCP; i++) if (CP[i].T >= T - 0.05f) { CpNext = i; break; }
	ApplyTime(T);
}
void ASwapExperience::SetPlace(int32 I) { Spot = ((I % NSPOT) + NSPOT) % NSPOT; LastChapter = -1; ApplySpot(true); }
void ASwapExperience::ChangePlace(int32 Step)
{
	if (Viewer && Viewer->IsVR()) Spot = (Spot == 3) ? 4 : 3;     // in the headset: following the barge, or high above the canal
	else Spot = ((Spot + Step) % NSPOT + NSPOT) % NSPOT;           // on the laptop: all five places
	LastChapter = -1; ApplySpot(true);
}

// ------------------------------------------------------------------ battery state
float ASwapExperience::SocAt() const
{
	const float km = Barge ? KmOf(Barge->GetActorLocation()) : 51.f;
	return T < 43.6f ? FMath::Clamp(44.f - (km - 50.72f) * SOC_KM, 0.f, 100.f) : FMath::Clamp(100.f - (km - 51.95f) * SOC_KM, 0.f, 100.f);
}
float ASwapExperience::PackAPct() const { return T < 29.2f ? SocAt() : FMath::Clamp(42.f + (T - 29.2f) / (T_END - 29.2f) * 40.f, 0.f, 100.f); }
float ASwapExperience::PackCPct() const { return T < 43.6f ? 100.f : SocAt(); }
FString ASwapExperience::PackAState() const { return T < 21.6f ? TEXT("On board") : T < 29.2f ? TEXT("On the crane") : TEXT("Charging ashore"); }

void ASwapExperience::PlaceFx(FFx& F, AActor* Pack, const FLinearColor& C, float Pct, const FString& Name, const FString& State, bool bBeam, bool bTag)
{
	const bool bOn = Pack != nullptr;
	UStaticMeshComponent* PM = Pack ? Pack->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!bOn || !PM || !PM->GetStaticMesh()) { F.Box->SetVisibility(false); if (F.Beam) F.Beam->SetVisibility(false); F.Tag->SetVisibility(false); return; }
	const FBox LB = PM->GetStaticMesh()->GetBoundingBox(); const FTransform CT = PM->GetComponentTransform();
	const FVector Ctr = CT.TransformPosition(LB.GetCenter()); const FVector Ext = LB.GetExtent() * CT.GetScale3D() + FVector(16);
	F.Box->SetVisibility(true); F.Box->SetWorldLocationAndRotation(Ctr, CT.GetRotation()); F.Box->SetWorldScale3D(Ext / 50.f);
	if (!F.MBox && (GlowMaterial || ColourMaterial)) { F.MBox = UMaterialInstanceDynamic::Create(GlowMaterial ? GlowMaterial : ColourMaterial, this); F.Box->SetMaterial(0, F.MBox); }
	const float Pulse = 0.45f + 0.12f * FMath::Sin(GetWorld()->GetTimeSeconds() * 3.f);
	if (F.MBox) { F.MBox->SetVectorParameterValue(TEXT("Colour"), C); F.MBox->SetScalarParameterValue(TEXT("Opacity"), Pulse); F.MBox->SetScalarParameterValue(TEXT("Glow"), 2.5f); }
	const float Top = Ctr.Z + Ext.Z;
	if (F.Beam)
	{
		bBeam = false;   // the light beam above the packs was confusing: off (colour and tag stay)
		F.Beam->SetVisibility(bBeam);
		if (bBeam) { F.Beam->SetWorldLocationAndRotation(FVector(Ctr.X, Ctr.Y, Top + 2600), FRotator::ZeroRotator); F.Beam->SetWorldScale3D(FVector(2.6f, 2.6f, 52.f));
			if (!F.MBeam && (GlowMaterial || ColourMaterial)) { F.MBeam = UMaterialInstanceDynamic::Create(GlowMaterial ? GlowMaterial : ColourMaterial, this); F.Beam->SetMaterial(0, F.MBeam); }
			if (F.MBeam) { F.MBeam->SetVectorParameterValue(TEXT("Colour"), C); F.MBeam->SetScalarParameterValue(TEXT("Opacity"), 0.35f); F.MBeam->SetScalarParameterValue(TEXT("Glow"), 2.f); } }
	}
	F.Tag->SetVisibility(bTag);
	if (bTag && Viewer)
	{
		const FVector Eye = Viewer->Camera->GetComponentLocation(); const float d = FVector::Dist(Eye, FVector(Ctr.X, Ctr.Y, Top));
		const float Size = FMath::Clamp(d * 0.016f, 18.f, 900.f);
		F.Tag->SetWorldSize(Size); F.Tag->SetTextRenderColor(PctFColor(Pct));
		F.Tag->SetText(FText::FromString(FString::Printf(TEXT("%s  %d%%<br>%s"), *Name, FMath::RoundToInt(Pct), *State)));
		const FVector P(Ctr.X, Ctr.Y, Top + 120 + Size * 0.4f);
		F.Tag->SetWorldLocation(P); FVector ToEye = Eye - P; ToEye.Z = 0; F.Tag->SetWorldRotation(ToEye.Rotation());
	}
}

// ------------------------------------------------------------------ viewer and panel
void ASwapExperience::AttachPanel()
{
	if (!Viewer) return;
	if (Viewer->IsVR())
	{
		Panel->AttachToComponent(Viewer->Origin, FAttachmentTransformRules::KeepRelativeTransform);
		Panel->SetRelativeLocationAndRotation(FVector(150, 0, 118), FRotator(0, 180, 0)); Panel->SetRelativeScale3D(FVector(1));
	}
	else
	{
		Panel->AttachToComponent(Viewer->Camera, FAttachmentTransformRules::KeepRelativeTransform);
		Panel->SetRelativeLocationAndRotation(FVector(120, -50, -25), FRotator(0, 180, 0)); Panel->SetRelativeScale3D(FVector(0.5f));
	}
}

void ASwapExperience::ApplySpot(bool bSnap)
{
	if (!Viewer || !Barge) return;
	Viewer->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	VrOff = FVector::ZeroVector;
	auto Face = [&](const FVector& P, const FVector& Look) { FVector d = Look - P; d.Z = 0; const float Yaw = d.Rotation().Yaw;
		Viewer->SetActorLocationAndRotation(P, FRotator(0, Viewer->IsVR() ? Yaw : 0, 0)); Viewer->SetDesktopView(Yaw, -6.f); };
	switch (Spot)
	{
	case 0: // wheelhouse roof, facing the battery bay and the containers
		Viewer->AttachToActor(Barge, FAttachmentTransformRules::KeepRelativeTransform);
		Viewer->SetActorRelativeLocation(FVector(0, -1730, 1225)); Viewer->SetActorRelativeRotation(FRotator(0, Viewer->IsVR() ? -90.f : 0.f, 0));
		Viewer->SetDesktopView(-90.f, -12.f); break;
	case 1: { const FTransform H = Hub->GetActorTransform(); Face(H.TransformPosition(FVector(4200, -3400, 180)), H.TransformPosition(FVector(0, -600, 700))); break; }
	case 2: { const FTransform L = Lock->GetActorTransform(); Face(L.TransformPosition(FVector(1000, -1600, 1205)), L.TransformPosition(FVector(1000, 800, 1205))); break; }
	case 4: { // high above the canal, north west of the terminal, looking over the hub, the canal and the lock
		const FVector Tg = (Hub && Lock) ? FMath::Lerp(Hub->GetActorLocation(), Lock->GetActorLocation(), 0.35) : Barge->GetActorLocation();
		const FVector P = Tg + FVector(-36100.0, -60000.0, 24400.0);
		const FRotator Look = (Tg - P).Rotation();
		Viewer->SetActorLocationAndRotation(P, FRotator(0, Viewer->IsVR() ? Look.Yaw : 0.f, 0)); Viewer->SetDesktopView(Look.Yaw, Look.Pitch); break; }
	default: UpdateDrone(0); break;
	}
	if (bSnap) AttachPanel();
}

void ASwapExperience::UpdateDrone(float Dt)
{
	if (Spot != 3 || !Viewer || !Barge) return;
	const FTransform B = Barge->GetActorTransform();
	if (Viewer->IsVR())
	{
		// in the headset: 40 m off the north side and 14 m above the water, heading free, so you can turn round and see the canal behind
		const FVector Want = B.TransformPosition(FVector(-4000, -950, 0)) + FVector(0, 0, 1700) + VrOff;
		const FVector P = Dt > 0 ? FMath::VInterpTo(Viewer->GetActorLocation(), Want, Dt, 1.2f) : Want;
		Viewer->SetActorLocation(P);
		if (Dt == 0) { const FVector d = B.TransformPosition(FVector(0, -6650, 0)) - P; Viewer->SetActorRotation(FRotator(0, d.Rotation().Yaw, 0)); }
		return;
	}
	// behind and above the barge, on the north side, looking at the battery bay
	const FVector Target = B.TransformPosition(FVector(0, -2100, 300));
	const FVector Want = B.TransformPosition(FVector(-9000, 7000, 0)) + FVector(0, 0, 5200);
	const FVector P = Dt > 0 ? FMath::VInterpTo(Viewer->GetActorLocation(), Want, Dt, 1.2f) : Want;
	FVector d = Target - P; const float Yaw = d.Rotation().Yaw, Pitch = d.Rotation().Pitch;
	Viewer->SetActorLocation(P);
	if (!Viewer->IsVR()) { Viewer->SetActorRotation(FRotator::ZeroRotator); if (Dt == 0) Viewer->SetDesktopView(Yaw, Pitch); }
	else if (Dt == 0) Viewer->SetActorRotation(FRotator(0, Yaw, 0));
}

void ASwapExperience::UpdatePanel()
{
	int32 Ci = 0; for (int32 j = 0; j < NCH; j++) if (T >= CH[j].T) Ci = j;
	if (Ci != LastChapter)
	{
		LastChapter = Ci;
		TxtStep->SetText(FText::FromString(FString::Printf(TEXT("STEP %02d OF %d  ·  %s"), Ci + 1, NCH, SpotNames[Spot])));
		TxtTitle->SetText(FText::FromString(CH[Ci].H));
		TxtBody->SetText(FText::FromString(Wrap(CH[Ci].B, 62)));
	}
	const bool bVR = Viewer && Viewer->IsVR();
	FString Prompt;
	if (Waiting >= 0) Prompt = FString::Printf(TEXT("%s  %s<br>%s"), bVR ? TEXT("Press A:") : TEXT("Press Space:"), CP[Waiting].Btn, *Wrap(CP[Waiting].P, 70));
	else if (bEnded) Prompt = bVR ? TEXT("Journey complete. Press A to watch again.") : TEXT("Journey complete. Press Space to watch again.");
	else Prompt = bVR ? TEXT("B or Y: change place  ·  left stick: move (grip: fast)  ·  right stick: turn, up and down") : TEXT("Tab: change place  ·  hold right mouse: look around  ·  P: pause");
	if (Prompt != LastPrompt) { LastPrompt = Prompt; TxtPrompt->SetText(FText::FromString(Prompt)); TxtPrompt->SetTextRenderColor(Waiting >= 0 ? FColor(255, 214, 120) : FColor(150, 175, 200)); }
	// on-board pack: pack A until the swap, pack C after
	const bool bA = T < 43.6f; const float Pct = (bA && T >= 21.6f) ? 0.f : (bA ? PackAPct() : PackCPct());
	TxtBatt->SetText(FText::FromString(FString::Printf(TEXT("On-board pack: %d%%  %s"), FMath::RoundToInt(Pct), bA ? (T < 21.6f ? TEXT("Pack A") : TEXT("being swapped")) : TEXT("Pack C"))));
	TxtBatt->SetTextRenderColor(PctFColor(Pct));
	const float W = 60.f * FMath::Clamp(Pct / 100.f, 0.01f, 1.f);
	BarFill->SetRelativeLocation(FVector(-0.4f, 54 - W / 2.f, -13)); BarFill->SetRelativeScale3D(FVector(0.006f, W / 100.f, 0.024f));
	if (!MBar && ColourMaterial) { MBar = UMaterialInstanceDynamic::Create(ColourMaterial, this); BarFill->SetMaterial(0, MBar);
		UMaterialInstanceDynamic* Bg = UMaterialInstanceDynamic::Create(ColourMaterial, this); Bg->SetVectorParameterValue(TEXT("Colour"), FLinearColor(0.01f, 0.02f, 0.04f)); Bg->SetScalarParameterValue(TEXT("Opacity"), 0.86f); Bg->SetScalarParameterValue(TEXT("Glow"), 1.f); PanelBg->SetMaterial(0, Bg);
		UMaterialInstanceDynamic* Bb = UMaterialInstanceDynamic::Create(ColourMaterial, this); Bb->SetVectorParameterValue(TEXT("Colour"), FLinearColor(0.08f, 0.1f, 0.13f)); Bb->SetScalarParameterValue(TEXT("Opacity"), 1.f); Bb->SetScalarParameterValue(TEXT("Glow"), 1.f); BarBg->SetMaterial(0, Bb); }
	if (MBar) { MBar->SetVectorParameterValue(TEXT("Colour"), PctColour(Pct)); MBar->SetScalarParameterValue(TEXT("Opacity"), 1.f); MBar->SetScalarParameterValue(TEXT("Glow"), 1.5f); }
	TxtLive->SetText(FText::FromString(LiveLine()));
}

// ------------------------------------------------------------------ live data
void ASwapExperience::GetJson(const FString& Url, TFunction<void(TSharedPtr<FJsonObject>)> Done)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = FHttpModule::Get().CreateRequest();
	R->SetURL(Url); R->SetVerb(TEXT("GET")); R->SetTimeout(15.f);
	TWeakObjectPtr<ASwapExperience> Self(this);
	R->OnProcessRequestComplete().BindLambda([Self, Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk) {
		if (!Self.IsValid() || !bOk || !Resp.IsValid() || Resp->GetResponseCode() != 200) return;
		TSharedPtr<FJsonObject> O; TSharedRef<TJsonReader<>> Rd = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
		if (FJsonSerializer::Deserialize(Rd, O) && O.IsValid()) Done(O); });
	R->ProcessRequest();
}

void ASwapExperience::RefreshLive()
{
	const FString E = TEXT("https://opendata.elia.be/api/explore/v2.1/catalog/datasets/");
	GetJson(TEXT("https://api.open-meteo.com/v1/forecast?latitude=51.099&longitude=5.105&current=temperature_2m,wind_speed_10m,wind_direction_10m,cloud_cover,precipitation,is_day&wind_speed_unit=ms&timezone=Europe%2FBrussels"),
		[this](TSharedPtr<FJsonObject> O) { const TSharedPtr<FJsonObject>* C; if (O->TryGetObjectField(TEXT("current"), C)) {
			Temp = (*C)->GetNumberField(TEXT("temperature_2m")); Wind = (*C)->GetNumberField(TEXT("wind_speed_10m")); Dir = (*C)->GetNumberField(TEXT("wind_direction_10m"));
			Cloud = (*C)->GetNumberField(TEXT("cloud_cover")); Rain = (*C)->GetNumberField(TEXT("precipitation")); LiveTime = (*C)->GetStringField(TEXT("time")); } });
	GetJson(E + TEXT("ods086/records?where=realtime%20is%20not%20null&order_by=datetime%20desc&limit=20&select=datetime,realtime"),
		[this](TSharedPtr<FJsonObject> O) { const TArray<TSharedPtr<FJsonValue>>* R; if (O->TryGetArrayField(TEXT("results"), R) && R->Num()) {
			const FString T0 = (*R)[0]->AsObject()->GetStringField(TEXT("datetime")); double S = 0;
			for (auto& v : *R) if (v->AsObject()->GetStringField(TEXT("datetime")) == T0) S += v->AsObject()->GetNumberField(TEXT("realtime")); WindMW = S; } });
	GetJson(E + TEXT("ods087/records?where=realtime%20is%20not%20null%20and%20region%3D%22Belgium%22&order_by=datetime%20desc&limit=1&select=datetime,realtime"),
		[this](TSharedPtr<FJsonObject> O) { const TArray<TSharedPtr<FJsonValue>>* R; if (O->TryGetArrayField(TEXT("results"), R) && R->Num()) SolarMW = (*R)[0]->AsObject()->GetNumberField(TEXT("realtime")); });
	GetJson(E + TEXT("ods002/records?where=measured%20is%20not%20null&order_by=datetime%20desc&limit=1&select=datetime,measured"),
		[this](TSharedPtr<FJsonObject> O) { const TArray<TSharedPtr<FJsonValue>>* R; if (O->TryGetArrayField(TEXT("results"), R) && R->Num()) LoadMW = (*R)[0]->AsObject()->GetNumberField(TEXT("measured")); });
	GetJson(FString::Printf(TEXT("https://raw.githubusercontent.com/alicemitakoff/albert-canal-vr/live-data/live/ships.json?t=%lld"), FDateTime::UtcNow().ToUnixTimestamp()),
		[this](TSharedPtr<FJsonObject> O) { const TArray<TSharedPtr<FJsonValue>>* S; if (O->TryGetArrayField(TEXT("ships"), S)) {
			const int64 Now = FDateTime::UtcNow().ToUnixTimestamp(); int32 n = 0; for (auto& v : *S) { double seen = 0; if (v->AsObject()->TryGetNumberField(TEXT("seen"), seen) && Now - seen < 3 * 3600) n++; } Vessels = n; } });
}

FString ASwapExperience::LiveLine() const
{
	static const TCHAR* Cmp[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
	FString S = TEXT("LIVE  ");
	if (!(Temp < -1e8f)) S += FString::Printf(TEXT("%d°C, wind %.1f m/s %s"), FMath::RoundToInt(Temp), Wind, Cmp[((int32)FMath::RoundToInt(Dir / 45.f)) % 8]);
	else S += TEXT("weather loading");
	if (!(WindMW < -1e8f)) { const double Ren = WindMW + ((SolarMW < -1e8f) ? 0 : SolarMW); S += FString::Printf(TEXT("  ·  Belgian grid %.1f GW wind and solar"), Ren / 1000.0);
		if (!(LoadMW < -1e8f) && LoadMW > 0) S += FString::Printf(TEXT(" (%d%% of demand)"), FMath::RoundToInt(100.0 * Ren / LoadMW)); }
	if (Vessels >= 0) S += FString::Printf(TEXT("  ·  %d vessels on the corridor (AIS)"), Vessels);
	if (LiveTime.Len() >= 16) S += TEXT("  ·  ") + LiveTime.Mid(11, 5) + TEXT(" Brussels");
	return S;
}

// ------------------------------------------------------------------ main loop
void ASwapExperience::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld() && GetWorld()->IsGameWorld()) MoveBatteryBay();   // only while playing, so the saved level never changes
	BuildKeys();
	for (AActor* A : SparePacks) { (void)A; }
	TArray<AActor*> R; UGameplayStatics::GetAllActorsWithTag(this, TEXT("TurbineRotor"), Rotors);
	Viewer = Cast<ASwapPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	Spot = 3;   // start on the drone that follows the barge
	T = 0; CpNext = 0; Waiting = -1; bEnded = false;
	ApplyTime(0); ApplySpot(true);
	RefreshLive();
}

void ASwapExperience::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bBuilt) return;
	if (!Viewer) { Viewer = Cast<ASwapPawn>(UGameplayStatics::GetPlayerPawn(this, 0)); if (Viewer) ApplySpot(true); }
	if (Viewer)
	{
		if (Viewer->bContinuePressed) { Viewer->bContinuePressed = false; Continue(); }
		if (Viewer->bPausePressed) { Viewer->bPausePressed = false; bPaused = !bPaused; }
		if (Viewer->bNextPlacePressed) { Viewer->bNextPlacePressed = false; ChangePlace(1); }
		if (Viewer->bPrevPlacePressed) { Viewer->bPrevPlacePressed = false; ChangePlace(-1); }
		// moving with the Quest sticks: on the drone it shifts the drone's spot, anywhere else it moves the viewer
		if (!Viewer->PendingMove.IsNearlyZero())
		{
			if (Spot == 3 && Viewer->IsVR()) VrOff += Viewer->PendingMove;
			else if (Spot != 0) Viewer->AddActorWorldOffset(Viewer->PendingMove);
			Viewer->PendingMove = FVector::ZeroVector;
		}
	}
	if (Waiting < 0 && !bEnded && !bPaused)
	{
		// faster passages, as in the web version: towards the lock and while the chamber fills
		const float Boost = (T > 56.5f && T < 78.f) ? 1.8f : (T > 84.f && T < 104.f) ? 1.7f : 1.f;
		const float Next = T + Dt * Speed * Boost;
		if (bGuided && CpNext < NCP && Next >= CP[CpNext].T) { T = CP[CpNext].T; Waiting = CpNext; }
		else if (Next >= T_END - 0.01f) { T = T_END - 0.01f; bEnded = true; }
		else T = Next;
	}
	ApplyTime(T);
	UpdateWake();
	UpdateDrone(Dt);
	// battery highlights follow whichever copy of each pack is in view
	AActor* A = T < 21.6f ? PackA_Barge : T < 29.2f ? PackA_Crane : PackA_Dock;
	AActor* C = T < 36.2f ? PackC_Hub : T < 43.6f ? PackC_Crane : PackC_Barge;
	const float pa = PackAPct(), pc = PackCPct();
	const bool bDrone = Spot != 0;
	PlaceFx(FxA, T < 60 ? A : nullptr, PctColour(T >= 21.6f && T < 29.2f ? FMath::Min(pa, 43.f) : pa), pa, TEXT("Pack A"), PackAState(), T < 47 && bDrone, T < 60);
	PlaceFx(FxC, T >= 10 ? C : nullptr, PctColour(pc), pc, TEXT("Pack C"), T < 36.2f ? TEXT("Fully charged") : T < 43.6f ? TEXT("On the crane") : TEXT("Powering the barge"), T >= 11 && bDrone, T >= 11);
	for (int32 i = 0; i < FxSpare.Num(); i++) PlaceFx(FxSpare[i], SparePacks.IsValidIndex(i) ? SparePacks[i] : nullptr, PctColour(100), 100, TEXT(""), TEXT(""), false, false);
	UpdatePanel();
	// the turbines turn at the measured wind speed (stopped below the 3 m/s cut-in)
	for (AActor* Ro : Rotors) if (Ro) { const float Rad = 47.5f * Ro->GetActorScale3D().X; const float W = (Wind < -1e8f) ? 0.6f : (Wind < 3 ? 0.05f : FMath::Min(1.6f, 7.f * FMath::Min(Wind, 13.f) / Rad));
		Ro->AddActorLocalRotation(FQuat(FVector::YAxisVector, W * Dt)); }
	LiveTimer += Dt; if (LiveTimer > 120.f) { LiveTimer = 0; RefreshLive(); }
}
