#include "RpgDemoToolGenSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Default = FString())
    {
        FString Value;
        return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : Default;
    }

    int32 ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Default = 0)
    {
        int32 Value = Default;
        return Object.IsValid() && Object->TryGetNumberField(Field, Value) ? Value : Default;
    }

    float ReadFloat(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float Default = 0.f)
    {
        double Value = Default;
        return Object.IsValid() && Object->TryGetNumberField(Field, Value) ? static_cast<float>(Value) : Default;
    }

    FVector ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 3) return FVector::ZeroVector;
        return FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
    }
}

bool URpgDemoToolGenSubsystem::ReadJson(const FString& RelativePath, TSharedPtr<FJsonObject>& OutObject) const
{
    FString Source;
    const FString FullPath = DataRoot / RelativePath;
    if (!FFileHelper::LoadFileToString(Source, *FullPath)) return false;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Source);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

void URpgDemoToolGenSubsystem::AddError(const FString& Message)
{
    if (!LastError.IsEmpty()) LastError += TEXT("\n");
    LastError += Message;
    UE_LOG(LogTemp, Warning, TEXT("[RpgDemo][ToolGen] %s"), *Message);
}

bool URpgDemoToolGenSubsystem::LoadToolGenData()
{
    bLoaded = false;
    LastError.Empty();
    Npcs.Empty();
    Quests.Empty();
    Dialogues.Empty();
    ItemNames.Empty();
    DataRoot = FPaths::ProjectContentDir() / TEXT("Data/ToolGen");

    TSharedPtr<FJsonObject> ProjectSettings;
    if (!ReadJson(TEXT("ProjectSettings.json"), ProjectSettings))
    {
        AddError(TEXT("无法读取 Content/Data/ToolGen/ProjectSettings.json"));
        return false;
    }

    PlayerCharacterId = ReadString(ProjectSettings, TEXT("playerCharacter"), TEXT("Player"));
    DefaultCameraId = ReadString(ProjectSettings, TEXT("defaultCamera"), TEXT("ThirdPerson"));
    DefaultCameraPoseId = ReadString(ProjectSettings, TEXT("defaultCameraPose"), TEXT("Explore"));

    TSharedPtr<FJsonObject> Character;
    if (ReadJson(FString::Printf(TEXT("Characters/Classes/%s.json"), *PlayerCharacterId), Character))
    {
        PlayerCharacter.Id = ReadString(Character, TEXT("id"), PlayerCharacterId);
        PlayerCharacter.AppearanceId = ReadString(Character, TEXT("appearanceId"));
        PlayerCharacter.AttributeTemplateId = ReadString(Character, TEXT("attributeTemplateId"));
        PlayerCharacter.InitialLevel = ReadInt(Character, TEXT("initialLevel"), 1);
        const TSharedPtr<FJsonObject>* MoveSpeed = nullptr;
        if (Character->TryGetObjectField(TEXT("moveSpeed"), MoveSpeed) && MoveSpeed && MoveSpeed->IsValid())
        {
            PlayerCharacter.WalkSpeed = ReadFloat(*MoveSpeed, TEXT("walk"), 300.f);
            PlayerCharacter.RunSpeed = ReadFloat(*MoveSpeed, TEXT("run"), 600.f);
            PlayerCharacter.SprintSpeed = ReadFloat(*MoveSpeed, TEXT("sprint"), 900.f);
        }
    }
    else
    {
        AddError(FString::Printf(TEXT("无法读取玩家角色配置：Characters/Classes/%s.json"), *PlayerCharacterId));
    }

    TSharedPtr<FJsonObject> Camera;
    if (ReadJson(FString::Printf(TEXT("Camera/%s.json"), *DefaultCameraId), Camera))
    {
        ExploreCameraPose.FixedYaw = ReadFloat(Camera, TEXT("fixedYaw"), -35.f);
        const TSharedPtr<FJsonObject>* Poses = nullptr;
        const TSharedPtr<FJsonObject>* SelectedPose = nullptr;
        if (!Camera->TryGetObjectField(TEXT("poses"), Poses) || !Poses || !Poses->IsValid())
        {
            AddError(FString::Printf(TEXT("相机配置没有 poses：Camera/%s.json"), *DefaultCameraId));
        }
        else
        {
            (*Poses)->TryGetObjectField(DefaultCameraPoseId, SelectedPose);
            if (!SelectedPose || !SelectedPose->IsValid())
            {
                AddError(FString::Printf(TEXT("默认相机模式不存在：Camera/%s.json -> %s"), *DefaultCameraId, *DefaultCameraPoseId));
            }
            else
            {
                ExploreCameraPose.ArmLength = ReadFloat(*SelectedPose, TEXT("armLength"), 735.f);
                ExploreCameraPose.FieldOfView = ReadFloat(*SelectedPose, TEXT("fieldOfView"), 51.f);
                ExploreCameraPose.Pitch = ReadFloat(*SelectedPose, TEXT("pitch"), -58.f);
                ExploreCameraPose.SocketOffset = ReadVector(*SelectedPose, TEXT("socketOffset"));
                ExploreCameraPose.TargetOffset = ReadVector(*SelectedPose, TEXT("targetOffset"));
                (*SelectedPose)->TryGetBoolField(TEXT("enableLag"), ExploreCameraPose.bEnableLag);
                ExploreCameraPose.CameraLagSpeed = ReadFloat(*SelectedPose, TEXT("cameraLagSpeed"), 10.f);
                ExploreCameraPose.CameraRotationLagSpeed = ReadFloat(*SelectedPose, TEXT("cameraRotationLagSpeed"), 10.f);
            }
        }
    }
    else
    {
        AddError(FString::Printf(TEXT("无法读取相机配置：Camera/%s.json"), *DefaultCameraId));
    }

    if (!PlayerCharacter.AttributeTemplateId.IsEmpty())
    {
        TSharedPtr<FJsonObject> Attributes;
        if (ReadJson(FString::Printf(TEXT("Attributes/%s.json"), *PlayerCharacter.AttributeTemplateId), Attributes))
        {
            PlayerAttributes.Health = ReadInt(Attributes, TEXT("hp"), 100);
            PlayerAttributes.Mana = ReadInt(Attributes, TEXT("mp"), 30);
            PlayerAttributes.Attack = ReadInt(Attributes, TEXT("atk"), 25);
            PlayerAttributes.Defense = ReadInt(Attributes, TEXT("def"), 20);
        }
        else AddError(FString::Printf(TEXT("无法读取属性模板：Attributes/%s.json"), *PlayerCharacter.AttributeTemplateId));
    }

    LoadInputBindings();
    LoadPlayerAppearance();
    LoadNpcDefinitions();
    LoadQuestDefinitions();
    LoadDialogueDefinitions();
    LoadItemNames();
    bLoaded = true;
    UE_LOG(LogTemp, Display, TEXT("[RpgDemo][ToolGen] Loaded: NPC=%d Quest=%d Dialogue=%d Item=%d"), Npcs.Num(), Quests.Num(), Dialogues.Num(), ItemNames.Num());
    return true;
}

void URpgDemoToolGenSubsystem::LoadInputBindings()
{
    MovementInput = FRpgToolGenMovementInput();
    TSharedPtr<FJsonObject> Input;
    if (!ReadJson(TEXT("Input/Default.json"), Input))
    {
        AddError(TEXT("无法读取输入配置：Input/Default.json"));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* DefaultContexts = nullptr;
    if (!Input->TryGetArrayField(TEXT("defaultContexts"), DefaultContexts) || !DefaultContexts || DefaultContexts->Num() == 0)
    {
        AddError(TEXT("输入配置未启用默认 Context；请在输入编辑器启用 Game Context"));
        return;
    }

    const FString ContextId = (*DefaultContexts)[0]->AsString();
    const TSharedPtr<FJsonObject>* Contexts = nullptr;
    const TSharedPtr<FJsonObject>* Context = nullptr;
    const TSharedPtr<FJsonObject>* Axes2D = nullptr;
    const TSharedPtr<FJsonObject>* Move = nullptr;
    if (!Input->TryGetObjectField(TEXT("contexts"), Contexts) || !Contexts || !Contexts->IsValid()
        || !(*Contexts)->TryGetObjectField(ContextId, Context) || !Context || !Context->IsValid()
        || !(*Context)->TryGetObjectField(TEXT("axes2D"), Axes2D) || !Axes2D || !Axes2D->IsValid()
        || !(*Axes2D)->TryGetObjectField(TEXT("Move"), Move) || !Move || !Move->IsValid())
    {
        AddError(TEXT("输入配置缺少默认 Context 的 Move Axis2D"));
        return;
    }

    MovementInput.PositiveX = ReadString(*Move, TEXT("positiveX"));
    MovementInput.NegativeX = ReadString(*Move, TEXT("negativeX"));
    MovementInput.PositiveY = ReadString(*Move, TEXT("positiveY"));
    MovementInput.NegativeY = ReadString(*Move, TEXT("negativeY"));
    MovementInput.ScaleX = ReadFloat(*Move, TEXT("scaleX"));
    MovementInput.ScaleY = ReadFloat(*Move, TEXT("scaleY"));
    if (!MovementInput.IsValid()) AddError(TEXT("Move Axis2D 缺少四方向按键配置"));
}

void URpgDemoToolGenSubsystem::LoadPlayerAppearance()
{
    PlayerSpriteSheet = FRpgToolGenSpriteSheet();
    if (PlayerCharacter.AppearanceId.IsEmpty()) return;

    TSharedPtr<FJsonObject> Appearance;
    if (!ReadJson(FString::Printf(TEXT("Characters/Appearances/%s.json"), *PlayerCharacter.AppearanceId), Appearance))
    {
        AddError(FString::Printf(TEXT("无法读取角色外观：Characters/Appearances/%s.json"), *PlayerCharacter.AppearanceId));
        return;
    }

    const TSharedPtr<FJsonObject>* Render = nullptr;
    if (!Appearance->TryGetObjectField(TEXT("spriteRender"), Render) || !Render || !Render->IsValid())
    {
        AddError(TEXT("角色外观未配置 spriteRender；请在角色外观编辑器保存运行时 2D 参数"));
        return;
    }
    PlayerSpriteSheet.Render.WorldScale = ReadFloat(*Render, TEXT("worldScale"));
    PlayerSpriteSheet.Render.LocalOffset = ReadVector(*Render, TEXT("localOffset"));
    PlayerSpriteSheet.Render.Yaw = ReadFloat(*Render, TEXT("yaw"));
    PlayerSpriteSheet.Render.MovementThreshold = ReadFloat(*Render, TEXT("movementThreshold"));
    PlayerSpriteSheet.Render.IdleStateId = ReadString(*Render, TEXT("idleStateId"));
    PlayerSpriteSheet.Render.WalkStateId = ReadString(*Render, TEXT("walkStateId"));
    PlayerSpriteSheet.Render.DefaultDirection = ReadString(*Render, TEXT("defaultDirection"));
    PlayerSpriteSheet.Render.PixelSampling = ReadString(*Render, TEXT("pixelSampling"));
    (*Render)->TryGetBoolField(TEXT("mipmaps"), PlayerSpriteSheet.Render.bMipmaps);
    if (PlayerSpriteSheet.Render.WorldScale <= 0.f || PlayerSpriteSheet.Render.IdleStateId.IsEmpty() || PlayerSpriteSheet.Render.WalkStateId.IsEmpty() || PlayerSpriteSheet.Render.DefaultDirection.IsEmpty() || PlayerSpriteSheet.Render.PixelSampling.IsEmpty())
    {
        AddError(TEXT("spriteRender 配置不完整：需要 worldScale、状态 ID、默认方向与像素采样设置"));
        return;
    }

    const TSharedPtr<FJsonObject>* SpriteRef = nullptr;
    if (!Appearance->TryGetObjectField(TEXT("spriteSheetAsset"), SpriteRef) || !SpriteRef || !SpriteRef->IsValid())
    {
        AddError(TEXT("角色外观未配置 spriteSheetAsset"));
        return;
    }

    const FString AssetId = ReadString(*SpriteRef, TEXT("assetId"));
    const FString VersionId = ReadString(*SpriteRef, TEXT("versionId"));
    TSharedPtr<FJsonObject> Manifest;
    if (!ReadJson(TEXT("RuntimeAssetManifest.json"), Manifest))
    {
        AddError(TEXT("无法读取 RuntimeAssetManifest.json；请先从资产中心发布精灵表"));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("assets"), Assets) || !Assets) return;
    for (const TSharedPtr<FJsonValue>& Value : *Assets)
    {
        const TSharedPtr<FJsonObject> Asset = Value->AsObject();
        if (!Asset.IsValid() || ReadString(Asset, TEXT("assetId")) != AssetId || ReadString(Asset, TEXT("versionId")) != VersionId) continue;

        PlayerSpriteSheet.ObjectPath = ReadString(Asset, TEXT("objectPath"));
        const TArray<TSharedPtr<FJsonValue>>* Clips = nullptr;
        if (Asset->TryGetArrayField(TEXT("clips"), Clips) && Clips)
        {
            for (const TSharedPtr<FJsonValue>& ClipValue : *Clips)
            {
                const TSharedPtr<FJsonObject> ClipJson = ClipValue->AsObject();
                if (!ClipJson.IsValid()) continue;
                FRpgToolGenSpriteClip Clip;
                Clip.Id = ReadString(ClipJson, TEXT("id"));
                Clip.State = ReadString(ClipJson, TEXT("state"));
                Clip.Direction = ReadString(ClipJson, TEXT("dir"));
                Clip.FramesPerSecond = ReadFloat(ClipJson, TEXT("fps"), 10.f);
                ClipJson->TryGetBoolField(TEXT("loop"), Clip.bLoop);
                const TArray<TSharedPtr<FJsonValue>>* Frames = nullptr;
                if (ClipJson->TryGetArrayField(TEXT("frames"), Frames) && Frames)
                {
                    for (const TSharedPtr<FJsonValue>& FrameValue : *Frames)
                    {
                        const TSharedPtr<FJsonObject> FrameJson = FrameValue->AsObject();
                        if (!FrameJson.IsValid()) continue;
                        FRpgToolGenSpriteFrame Frame;
                        Frame.X = ReadInt(FrameJson, TEXT("x"));
                        Frame.Y = ReadInt(FrameJson, TEXT("y"));
                        Frame.Width = ReadInt(FrameJson, TEXT("width"));
                        Frame.Height = ReadInt(FrameJson, TEXT("height"));
                        if (Frame.Width > 0 && Frame.Height > 0) Clip.Frames.Add(Frame);
                    }
                }
                if (!Clip.State.IsEmpty() && !Clip.Direction.IsEmpty() && !Clip.Frames.IsEmpty())
                {
                    PlayerSpriteSheet.ClipsByKey.Add(Clip.State.ToLower() + TEXT("_") + Clip.Direction.ToLower(), MoveTemp(Clip));
                }
            }
        }
        break;
    }

    if (PlayerSpriteSheet.ObjectPath.IsEmpty() || PlayerSpriteSheet.ClipsByKey.IsEmpty())
    {
        AddError(FString::Printf(TEXT("精灵表固定版本未发布到运行时清单：%s / %s"), *AssetId, *VersionId));
    }
}

void URpgDemoToolGenSubsystem::LoadNpcDefinitions()
{
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *(DataRoot / TEXT("NPC")), TEXT("*.json"), true, false);
    for (const FString& File : Files)
    {
        TSharedPtr<FJsonObject> Json;
        FString Source;
        if (!FFileHelper::LoadFileToString(Source, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Json) || !Json.IsValid()) continue;
        FRpgToolGenNpc Npc;
        Npc.Id = ReadString(Json, TEXT("id"));
        if (Npc.Id.IsEmpty()) continue;
        Npc.Name = ReadString(Json, TEXT("name"), Npc.Id);
        Npc.DialogueId = ReadString(Json, TEXT("dialogueId"));
        Npc.InteractRadius = ReadFloat(Json, TEXT("interactRadius"), 200.f);
        Npcs.Add(Npc.Id, MoveTemp(Npc));
    }
}

void URpgDemoToolGenSubsystem::LoadQuestDefinitions()
{
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *(DataRoot / TEXT("Quest")), TEXT("*.json"), true, false);
    for (const FString& File : Files)
    {
        TSharedPtr<FJsonObject> Json;
        FString Source;
        if (!FFileHelper::LoadFileToString(Source, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Json) || !Json.IsValid()) continue;
        FRpgToolGenQuest Quest;
        Quest.Id = ReadString(Json, TEXT("id"));
        if (Quest.Id.IsEmpty()) continue;
        Quest.Name = ReadString(Json, TEXT("name"), Quest.Id);
        const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
        if (Json->TryGetArrayField(TEXT("steps"), Steps) && Steps)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Steps)
            {
                const TSharedPtr<FJsonObject> StepJson = Value->AsObject();
                if (!StepJson.IsValid()) continue;
                FRpgToolGenQuestStep Step;
                Step.Id = ReadString(StepJson, TEXT("id"));
                Step.Type = ReadString(StepJson, TEXT("type"));
                Step.Description = ReadString(StepJson, TEXT("desc"));
                Step.TargetNpcId = ReadString(StepJson, TEXT("targetNpcId"));
                Quest.Steps.Add(MoveTemp(Step));
            }
        }
        const TSharedPtr<FJsonObject>* Rewards = nullptr;
        if (Json->TryGetObjectField(TEXT("rewards"), Rewards) && Rewards && Rewards->IsValid())
        {
            Quest.RewardExperience = ReadInt(*Rewards, TEXT("exp"));
            Quest.RewardGold = ReadInt(*Rewards, TEXT("gold"));
            const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
            if ((*Rewards)->TryGetArrayField(TEXT("items"), Items) && Items)
            {
                for (const TSharedPtr<FJsonValue>& Value : *Items)
                {
                    const TSharedPtr<FJsonObject> Item = Value->AsObject();
                    if (!Item.IsValid()) continue;
                    const FString ItemId = ReadString(Item, TEXT("id"));
                    if (!ItemId.IsEmpty()) Quest.RewardItems.Add(ItemId, ReadInt(Item, TEXT("count"), 1));
                }
            }
        }
        Quests.Add(Quest.Id, MoveTemp(Quest));
    }
}

void URpgDemoToolGenSubsystem::LoadDialogueDefinitions()
{
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *(DataRoot / TEXT("Dialogue")), TEXT("*.json"), true, false);
    for (const FString& File : Files)
    {
        TSharedPtr<FJsonObject> Json;
        FString Source;
        if (!FFileHelper::LoadFileToString(Source, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Json) || !Json.IsValid()) continue;
        FRpgToolGenDialogue Dialogue;
        Dialogue.Id = ReadString(Json, TEXT("id"));
        if (Dialogue.Id.IsEmpty()) continue;
        Dialogue.StartNodeId = ReadString(Json, TEXT("startNodeId"));
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        if (Json->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Nodes)
            {
                const TSharedPtr<FJsonObject> NodeJson = Value->AsObject();
                if (!NodeJson.IsValid()) continue;
                FRpgToolGenDialogueNode Node;
                Node.Id = ReadString(NodeJson, TEXT("id"));
                if (Node.Id.IsEmpty()) continue;
                Node.Type = ReadString(NodeJson, TEXT("type"));
                Node.Speaker = ReadString(NodeJson, TEXT("speaker"));
                Node.Text = ReadString(NodeJson, TEXT("text"));
                Node.Next = ReadString(NodeJson, TEXT("next"));
                Node.Action = ReadString(NodeJson, TEXT("action"));
                Node.QuestId = ReadString(NodeJson, TEXT("questId"));
                Node.ItemId = ReadString(NodeJson, TEXT("itemId"));
                Node.Count = ReadInt(NodeJson, TEXT("count"), 1);
                Node.ElseNext = ReadString(NodeJson, TEXT("elseNext"));
                const TArray<TSharedPtr<FJsonValue>>* Branches = nullptr;
                if (NodeJson->TryGetArrayField(TEXT("branches"), Branches) && Branches)
                {
                    for (const TSharedPtr<FJsonValue>& BranchValue : *Branches)
                    {
                        const TSharedPtr<FJsonObject> BranchJson = BranchValue->AsObject();
                        if (!BranchJson.IsValid()) continue;
                        FRpgToolGenDialogueBranch Branch;
                        Branch.Kind = ReadString(BranchJson, TEXT("kind"));
                        Branch.Key = ReadString(BranchJson, TEXT("key"));
                        Branch.Value = ReadInt(BranchJson, TEXT("value"));
                        Branch.Next = ReadString(BranchJson, TEXT("next"));
                        Node.Branches.Add(MoveTemp(Branch));
                    }
                }
                const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
                if (NodeJson->TryGetArrayField(TEXT("options"), Options) && Options)
                {
                    for (const TSharedPtr<FJsonValue>& OptionValue : *Options)
                    {
                        const TSharedPtr<FJsonObject> OptionJson = OptionValue->AsObject();
                        if (!OptionJson.IsValid()) continue;
                        FRpgToolGenDialogueOption Option;
                        Option.Label = ReadString(OptionJson, TEXT("label"));
                        Option.Next = ReadString(OptionJson, TEXT("next"));
                        Node.Options.Add(MoveTemp(Option));
                    }
                }
                Dialogue.Nodes.Add(Node.Id, MoveTemp(Node));
            }
        }
        Dialogues.Add(Dialogue.Id, MoveTemp(Dialogue));
    }
}

void URpgDemoToolGenSubsystem::LoadItemNames()
{
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *(DataRoot / TEXT("Item")), TEXT("*.json"), true, false);
    for (const FString& File : Files)
    {
        TSharedPtr<FJsonObject> Json;
        FString Source;
        if (!FFileHelper::LoadFileToString(Source, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Json) || !Json.IsValid()) continue;
        const FString Id = ReadString(Json, TEXT("id"));
        if (!Id.IsEmpty()) ItemNames.Add(Id, ReadString(Json, TEXT("name"), Id));
    }
}

const FRpgToolGenNpc* URpgDemoToolGenSubsystem::FindNpc(const FString& Id) const
{
    return Npcs.Find(Id);
}

const FRpgToolGenQuest* URpgDemoToolGenSubsystem::FindQuest(const FString& Id) const
{
    return Quests.Find(Id);
}

const FRpgToolGenDialogue* URpgDemoToolGenSubsystem::FindDialogue(const FString& Id) const
{
    return Dialogues.Find(Id);
}

FString URpgDemoToolGenSubsystem::GetItemName(const FString& Id) const
{
    if (const FString* Name = ItemNames.Find(Id)) return *Name;
    return Id;
}
