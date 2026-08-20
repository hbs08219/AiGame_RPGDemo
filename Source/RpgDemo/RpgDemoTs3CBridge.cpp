#include "RpgDemoTs3CBridge.h"

#include "RpgDemoCharacter.h"
#include "RpgDemoToolGenSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ARpgDemoCharacter* URpgDemoTs3CBridge::GetPlayerCharacter() const
{
    return GetGameInstance()
        ? Cast<ARpgDemoCharacter>(UGameplayStatics::GetPlayerPawn(GetGameInstance(), 0))
        : nullptr;
}

bool URpgDemoTs3CBridge::IsConfiguredKeyDown(const FString& KeyName) const
{
    const APlayerController* Controller = GetGameInstance()
        ? UGameplayStatics::GetPlayerController(GetGameInstance(), 0)
        : nullptr;
    const FKey Key(*KeyName);
    return Controller && Key.IsValid() && Controller->IsInputKeyDown(Key);
}

FVector2D URpgDemoTs3CBridge::ReadMoveInput() const
{
    const URpgDemoToolGenSubsystem* ToolGen = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<URpgDemoToolGenSubsystem>()
        : nullptr;
    if (!ToolGen || !ToolGen->IsLoaded() || !ToolGen->GetMovementInput().IsValid()) return FVector2D::ZeroVector;

    const FRpgToolGenMovementInput& Move = ToolGen->GetMovementInput();
    const float X = (IsConfiguredKeyDown(Move.PositiveX) ? Move.ScaleX : 0.f) - (IsConfiguredKeyDown(Move.NegativeX) ? Move.ScaleX : 0.f);
    const float Y = (IsConfiguredKeyDown(Move.PositiveY) ? Move.ScaleY : 0.f) - (IsConfiguredKeyDown(Move.NegativeY) ? Move.ScaleY : 0.f);
    return FVector2D(X, Y).GetClampedToMaxSize(1.f);
}

void URpgDemoTs3CBridge::PlacePlayer(float X, float Y, float Z)
{
    if (ARpgDemoCharacter* Character = GetPlayerCharacter())
    {
        Character->SetActorLocation(FVector(X, Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Display, TEXT("[RpgDemo][3C Adapter] Player placed from WorldScene at X=%.2f Y=%.2f Z=%.2f"), X, Y, Z);
    }
}

void URpgDemoTs3CBridge::ApplyMoveIntent(float DeltaX, float DeltaY)
{
    if (ARpgDemoCharacter* Character = GetPlayerCharacter())
    {
        Character->ApplyKernelMove(FVector2D(DeltaX, DeltaY));
    }
}

void URpgDemoTs3CBridge::ApplyAnimationState(const FString& StateId, const FString& Direction)
{
    if (ARpgDemoCharacter* Character = GetPlayerCharacter())
    {
        Character->ApplyKernelAnimationState(StateId, Direction);
    }
}

void URpgDemoTs3CBridge::ApplyCameraMode(const FString& ModeId)
{
    if (ARpgDemoCharacter* Character = GetPlayerCharacter())
    {
        Character->ApplyKernelCameraMode(ModeId);
    }
}

FVector2D URpgDemoTs3CBridge::GetLogicalPosition() const
{
    if (const ARpgDemoCharacter* Character = GetPlayerCharacter())
    {
        const FVector Location = Character->GetActorLocation();
        return FVector2D(Location.X, Location.Y);
    }
    return FVector2D::ZeroVector;
}
