#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RpgDemoToolGenSubsystem.generated.h"

struct FRpgToolGenCameraPose
{
    float ArmLength = 735.f;
    float FieldOfView = 51.f;
    float Pitch = -58.f;
    float FixedYaw = -35.f;
    FVector SocketOffset = FVector::ZeroVector;
    FVector TargetOffset = FVector::ZeroVector;
    bool bEnableLag = true;
    float CameraLagSpeed = 10.f;
    float CameraRotationLagSpeed = 10.f;
};

struct FRpgToolGenCharacter
{
    FString Id;
    FString AppearanceId;
    FString AttributeTemplateId;
    float WalkSpeed = 300.f;
    float RunSpeed = 600.f;
    float SprintSpeed = 900.f;
    int32 InitialLevel = 1;
};

struct FRpgToolGenAttributes
{
    int32 Health = 100;
    int32 Mana = 30;
    int32 Attack = 25;
    int32 Defense = 20;
};

struct FRpgToolGenSpriteFrame
{
    int32 X = 0;
    int32 Y = 0;
    int32 Width = 0;
    int32 Height = 0;
};

struct FRpgToolGenSpriteClip
{
    FString Id;
    FString State;
    FString Direction;
    float FramesPerSecond = 10.f;
    bool bLoop = true;
    TArray<FRpgToolGenSpriteFrame> Frames;
};

struct FRpgToolGenMovementInput
{
    FString PositiveX;
    FString NegativeX;
    FString PositiveY;
    FString NegativeY;
    float ScaleX = 0.f;
    float ScaleY = 0.f;

    bool IsValid() const
    {
        return !PositiveX.IsEmpty() && !NegativeX.IsEmpty() && !PositiveY.IsEmpty() && !NegativeY.IsEmpty();
    }
};

struct FRpgToolGenSpriteRenderSettings
{
    float WorldScale = 0.f;
    FVector LocalOffset = FVector::ZeroVector;
    float Yaw = 0.f;
    float MovementThreshold = 0.f;
    FString IdleStateId;
    FString WalkStateId;
    FString DefaultDirection;
    FString PixelSampling;
    bool bMipmaps = false;
};

struct FRpgToolGenSpriteSheet
{
    FString ObjectPath;
    FRpgToolGenSpriteRenderSettings Render;
    TMap<FString, FRpgToolGenSpriteClip> ClipsByKey;

    const FRpgToolGenSpriteClip* FindClip(const FString& State, const FString& Direction) const
    {
        return ClipsByKey.Find(State.ToLower() + TEXT("_") + Direction.ToLower());
    }
};

struct FRpgToolGenNpc
{
    FString Id;
    FString Name;
    FString DialogueId;
    float InteractRadius = 200.f;
};

struct FRpgToolGenQuestStep
{
    FString Id;
    FString Type;
    FString Description;
    FString TargetNpcId;
};

struct FRpgToolGenQuest
{
    FString Id;
    FString Name;
    TArray<FRpgToolGenQuestStep> Steps;
    int32 RewardExperience = 0;
    int32 RewardGold = 0;
    TMap<FString, int32> RewardItems;
};

struct FRpgToolGenDialogueBranch
{
    FString Kind;
    FString Key;
    int32 Value = 0;
    FString Next;
};

struct FRpgToolGenDialogueOption
{
    FString Label;
    FString Next;
};

struct FRpgToolGenDialogueNode
{
    FString Id;
    FString Type;
    FString Speaker;
    FString Text;
    FString Next;
    FString Action;
    FString QuestId;
    FString ItemId;
    int32 Count = 1;
    FString ElseNext;
    TArray<FRpgToolGenDialogueBranch> Branches;
    TArray<FRpgToolGenDialogueOption> Options;
};

struct FRpgToolGenDialogue
{
    FString Id;
    FString StartNodeId;
    TMap<FString, FRpgToolGenDialogueNode> Nodes;
};

UCLASS()
class RPGDEMO_API URpgDemoToolGenSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    bool LoadToolGenData();
    bool IsLoaded() const { return bLoaded; }
    const FString& GetLastError() const { return LastError; }
    const FRpgToolGenCameraPose& GetExploreCameraPose() const { return ExploreCameraPose; }
    const FRpgToolGenMovementInput& GetMovementInput() const { return MovementInput; }
    const FRpgToolGenCharacter& GetPlayerCharacter() const { return PlayerCharacter; }
    const FRpgToolGenAttributes& GetPlayerAttributes() const { return PlayerAttributes; }
    const FRpgToolGenSpriteSheet& GetPlayerSpriteSheet() const { return PlayerSpriteSheet; }
    const FRpgToolGenNpc* FindNpc(const FString& Id) const;
    const FRpgToolGenQuest* FindQuest(const FString& Id) const;
    const FRpgToolGenDialogue* FindDialogue(const FString& Id) const;
    FString GetItemName(const FString& Id) const;

private:
    bool ReadJson(const FString& RelativePath, TSharedPtr<class FJsonObject>& OutObject) const;
    void AddError(const FString& Message);
    void LoadNpcDefinitions();
    void LoadQuestDefinitions();
    void LoadDialogueDefinitions();
    void LoadItemNames();
    void LoadPlayerAppearance();
    void LoadInputBindings();

    bool bLoaded = false;
    FString LastError;
    FString DataRoot;
    FString PlayerCharacterId = TEXT("Player");
    FString DefaultCameraId = TEXT("ThirdPerson");
    FString DefaultCameraPoseId = TEXT("Explore");
    FRpgToolGenCameraPose ExploreCameraPose;
    FRpgToolGenMovementInput MovementInput;
    FRpgToolGenCharacter PlayerCharacter;
    FRpgToolGenAttributes PlayerAttributes;
    FRpgToolGenSpriteSheet PlayerSpriteSheet;
    TMap<FString, FRpgToolGenNpc> Npcs;
    TMap<FString, FRpgToolGenQuest> Quests;
    TMap<FString, FRpgToolGenDialogue> Dialogues;
    TMap<FString, FString> ItemNames;
};
