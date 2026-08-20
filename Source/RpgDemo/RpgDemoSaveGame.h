#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RpgDemoSaveGame.generated.h"

UCLASS()
class RPGDEMO_API URpgDemoSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY() int32 SaveVersion = 2;
    UPROPERTY() FString SavedAtText;
    UPROPERTY() FString Summary;
    UPROPERTY() FString ActiveQuestId;
    UPROPERTY() TMap<FString, int32> QuestSteps;
    UPROPERTY() TMap<FString, int32> Inventory;
    UPROPERTY() int32 QuestState = 0;
    UPROPERTY() bool bHasIncense = false;
    UPROPERTY() int32 PlayerHealth = 100;
    UPROPERTY() int32 PlayerMana = 30;
    UPROPERTY() int32 PotionCount = 2;
    UPROPERTY() int32 Gold = 0;
    UPROPERTY() FVector PlayerLocation = FVector::ZeroVector;
    UPROPERTY() FRotator PlayerRotation = FRotator::ZeroRotator;
};
