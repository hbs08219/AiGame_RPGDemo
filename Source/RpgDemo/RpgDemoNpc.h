#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RpgDemoNpc.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class RPGDEMO_API ARpgDemoNpc : public AActor
{
    GENERATED_BODY()

public:
    ARpgDemoNpc();
    void Configure(const FString& InId, const FString& InDisplayName, const FLinearColor& InColor, float InInteractRadius = 200.f);

    UPROPERTY(VisibleAnywhere, Category="RPG Demo")
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, Category="RPG Demo")
    UTextRenderComponent* Label;

    UPROPERTY(VisibleAnywhere, Category="RPG Demo")
    FString Id;

    UPROPERTY(VisibleAnywhere, Category="RPG Demo")
    float InteractRadius = 200.f;
};
