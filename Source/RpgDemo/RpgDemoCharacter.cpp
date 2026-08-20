#include "RpgDemoCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RpgDemoGameMode.h"
#include "RpgDemoToolGenSubsystem.h"

ARpgDemoCharacter::ARpgDemoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    // PlayerStart 未就绪时，发布版会回退到 (0,0,0)；不允许该临时出生过程因关卡碰撞而失败。
    // ToolGen / TypeScript 启动后会立即传送到关卡编辑器配置的 worldRuntime.playerSpawn。
    SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    // 固定俯视镜头：旋转由 ToolGen/Camera 配置写入，绝不继承角色转向。
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bDoCollisionTest = false;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // 角色只使用外观编辑器配置的 2D 精灵：一块 Plane + 遮罩材质，
    // 既是玩家看到的“本体”，也是投射到地面的阴影——两者共用同一份网格/材质/UV，永远保持一致。
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(RootComponent);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetStaticMesh(nullptr);
    Visual->SetVisibility(false, true);
    Visual->SetHiddenInGame(true, true);
}

void ARpgDemoCharacter::ApplyToolGenConfig(URpgDemoToolGenSubsystem* ToolGen)
{
    if (!ToolGen || !ToolGen->IsLoaded()) return;

    const FRpgToolGenCharacter& Character = ToolGen->GetPlayerCharacter();
    const FRpgToolGenCameraPose& Camera = ToolGen->GetExploreCameraPose();
    // 3C 位移由 TS Kernel 下达、UE Adapter 用胶囊扫掠执行；关闭 CMC，避免它接管并回写权威位置。
    GetCharacterMovement()->SetComponentTickEnabled(false);
    CameraBoom->TargetArmLength = Camera.ArmLength;
    CameraBoom->SetWorldRotation(FRotator(Camera.Pitch, Camera.FixedYaw, 0.f));
    CameraBoom->SocketOffset = Camera.SocketOffset;
    CameraBoom->TargetOffset = Camera.TargetOffset;
    CameraBoom->bEnableCameraLag = Camera.bEnableLag;
    CameraBoom->CameraLagSpeed = Camera.CameraLagSpeed;
    CameraBoom->bEnableCameraRotationLag = Camera.bEnableLag;
    CameraBoom->CameraRotationLagSpeed = Camera.CameraRotationLagSpeed;
    FollowCamera->SetFieldOfView(Camera.FieldOfView);

    SpriteSheet = &ToolGen->GetPlayerSpriteSheet();
    if (!SpriteSheet->ObjectPath.IsEmpty())
    {
        SpriteTexture = LoadObject<UTexture2D>(nullptr, *SpriteSheet->ObjectPath);
    }
    if (SpriteTexture && SpriteSheet->FindClip(SpriteSheet->Render.IdleStateId, SpriteSheet->Render.DefaultDirection))
    {
        // 采样策略完全由角色外观编辑器配置。Mip、流送、压缩均在精灵表纹理资产导入期确定，
        // 运行时只使用资产已保存的设置，避免 PIE 与发布版出现不同的纹理资源状态。
        SpriteTexture->Filter = SpriteSheet->Render.PixelSampling == TEXT("nearest") ? TF_Nearest : TF_Bilinear;
        SpriteTexture->UpdateResource();

        // 角色本体 + 阴影（八方旅人式）：不再拆成“显示用 Billboard + 阴影用 Plane”两份几何体，
        // 就用这一块 Plane 直接承载角色贴图——玩家看到的就是它，投射到地面的阴影也是它，
        // UV/缩放/朝向只算一次，永远不会出现“阴影和本体不一致”的问题。
        Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->SetRelativeLocation(SpriteSheet->Render.LocalOffset);
        Visual->SetVisibility(true, true);
        Visual->SetHiddenInGame(true);           // 先保持隐藏，等材质参数确认生效后再显示，避免冷启动闪一帧完整精灵表
        Visual->SetCastShadow(true);
        Visual->bCastDynamicShadow = true;

        UMaterialInterface* SpriteBaseMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/AiGame/Runtime/Materials/M_RpgDemoSpriteShadow.M_RpgDemoSpriteShadow"));
        Visual->SetMaterial(0, SpriteBaseMaterial);
        SpriteMaterial = SpriteBaseMaterial ? Visual->CreateDynamicMaterialInstance(0) : nullptr;
        if (SpriteMaterial) SpriteMaterial->SetTextureParameterValue(TEXT("SpriteTexture"), SpriteTexture);

        OrientVisualToFixedCamera();

        LastSpriteDirection = SpriteSheet->Render.DefaultDirection;
        ActiveSpriteClip = SpriteSheet->FindClip(SpriteSheet->Render.IdleStateId, LastSpriteDirection);
        SpriteFrameIndex = 0;
        SpriteFrameElapsed = 0.f;
        ApplySpriteFrame();

        // 冷启动（首次打开 UE）时该材质的着色器/PSO 可能还在编译，若这一帧就显示，
        // 渲染线程会先用材质默认参数（未裁剪的整张精灵表）画一两帧，看起来像“显示了完整 sheet”。
        // 延迟 0.2 秒再显示，给着色器编译留出时间；后续运行着色器已缓存，这个延迟不会造成可感知的等待。
        GetWorldTimerManager().SetTimer(RevealVisualTimerHandle, this, &ARpgDemoCharacter::RevealVisual, 0.2f, false);
        UE_LOG(LogTemp, Display, TEXT("[RpgDemo][2D] Loaded sprite sheet: %s (%d clips)"), *SpriteSheet->ObjectPath, SpriteSheet->ClipsByKey.Num());
    }
    else
    {
        Visual->SetHiddenInGame(true, true);
        Visual->SetVisibility(false, true);
        UE_LOG(LogTemp, Error, TEXT("[RpgDemo][2D] Sprite sheet unavailable; no fallback mesh will be rendered."));
    }

    UE_LOG(LogTemp, Display, TEXT("[RpgDemo][3C] Applied ToolGen config: Run=%.0f Arm=%.0f FOV=%.1f Pitch=%.1f Yaw=%.1f"), Character.RunSpeed, Camera.ArmLength, Camera.FieldOfView, Camera.Pitch, Camera.FixedYaw);
}

void ARpgDemoCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateSpriteAnimation(DeltaSeconds);
}

void ARpgDemoCharacter::ApplyKernelMove(const FVector2D& LocalDelta)
{
    if (LocalDelta.IsNearlyZero()) return;

    // TS Kernel 输出相对固定相机的逻辑平面位移；UE Adapter 仅负责映射到世界与碰撞扫描。
    const FRotator CameraYaw(0.f, CameraBoom->GetComponentRotation().Yaw, 0.f);
    const FVector Forward = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y);
    const FVector WorldIntent = Forward * LocalDelta.Y + Right * LocalDelta.X;
    if (WorldIntent.IsNearlyZero()) return;

    // WorldScene 定义固定 2.5D 移动平面：Z 恒定，只在 XY 平面移动。
    // 用手动胶囊扫掠仅检测 Pawn 阻挡体（建筑）；扫掠只用于“能走多远”，绝不整帧丢弃移动，因此不会卡死。
    const FVector Current = GetActorLocation();
    const FVector PlanarIntent(WorldIntent.X, WorldIntent.Y, 0.f);
    FVector Target = Current + PlanarIntent;

    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (Capsule && GetWorld())
    {
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgDemoKernelMove), false, this);
        const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
        FHitResult Hit;
        const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Current, Target, FQuat::Identity, ECC_Pawn, Shape, Params);
        if (bHit && !Hit.bStartPenetrating)
        {
            // 停在阻挡体之前，并沿墙面滑动剩余位移，保证贴着建筑仍可移动。
            Target = Current + PlanarIntent * FMath::Clamp(Hit.Time, 0.f, 1.f);
            const FVector Remaining = PlanarIntent * (1.f - Hit.Time);
            const FVector SlideDir = FVector::VectorPlaneProject(Remaining, Hit.Normal);
            FVector SlideTarget = Target + FVector(SlideDir.X, SlideDir.Y, 0.f);
            FHitResult SlideHit;
            if (!GetWorld()->SweepSingleByChannel(SlideHit, Target, SlideTarget, FQuat::Identity, ECC_Pawn, Shape, Params) || SlideHit.bStartPenetrating)
            {
                Target = FVector(SlideTarget.X, SlideTarget.Y, Current.Z);
            }
        }
        // bStartPenetrating 情况下保持 Target = 全量位移，让角色能从穿透状态中走出，避免永久卡死。
    }

    Target.Z = Current.Z;
    SetActorLocationAndRotation(Target, WorldIntent.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
}

void ARpgDemoCharacter::ApplyKernelAnimationState(const FString& StateId, const FString& Direction)
{
    if (!SpriteSheet || !SpriteTexture) return;

    const FRpgToolGenSpriteClip* RequestedClip = SpriteSheet->FindClip(StateId, Direction);
    if (!RequestedClip) return;

    bKernelLocomotionIsMoving = StateId == SpriteSheet->Render.WalkStateId;
    LastSpriteDirection = Direction;
    if (RequestedClip != ActiveSpriteClip)
    {
        ActiveSpriteClip = RequestedClip;
        SpriteFrameIndex = 0;
        SpriteFrameElapsed = 0.f;
        ApplySpriteFrame();
    }
}

void ARpgDemoCharacter::RevealVisual()
{
    if (!Visual) return;
    Visual->SetHiddenInGame(false);
}

void ARpgDemoCharacter::ApplyToolGenCameraPose()
{
    const URpgDemoToolGenSubsystem* ToolGen = GetGameInstance() ? GetGameInstance()->GetSubsystem<URpgDemoToolGenSubsystem>() : nullptr;
    if (!ToolGen || !ToolGen->IsLoaded()) return;

    const FRpgToolGenCameraPose& Camera = ToolGen->GetExploreCameraPose();
    CameraBoom->TargetArmLength = Camera.ArmLength;
    CameraBoom->SetWorldRotation(FRotator(Camera.Pitch, Camera.FixedYaw, 0.f));
    CameraBoom->SocketOffset = Camera.SocketOffset;
    CameraBoom->TargetOffset = Camera.TargetOffset;
    CameraBoom->bEnableCameraLag = Camera.bEnableLag;
    CameraBoom->CameraLagSpeed = Camera.CameraLagSpeed;
    CameraBoom->bEnableCameraRotationLag = Camera.bEnableLag;
    CameraBoom->CameraRotationLagSpeed = Camera.CameraRotationLagSpeed;
    FollowCamera->SetFieldOfView(Camera.FieldOfView);
}

void ARpgDemoCharacter::ApplyKernelCameraMode(const FString& ModeId)
{
    // 当前 3C 仅支持 ProjectSettings.defaultCameraPose；Kernel 只声明项目设置选中的模式。
    ApplyToolGenCameraPose();
    UE_LOG(LogTemp, Display, TEXT("[RpgDemo][3C Adapter] Camera mode requested: %s"), *ModeId);
}

void ARpgDemoCharacter::ApplySpriteFrame()
{
    if (!SpriteMaterial || !SpriteTexture || !ActiveSpriteClip || !ActiveSpriteClip->Frames.IsValidIndex(SpriteFrameIndex) || !SpriteSheet) return;

    const FRpgToolGenSpriteFrame& Frame = ActiveSpriteClip->Frames[SpriteFrameIndex];
    const float TexWidth = FMath::Max(1.f, static_cast<float>(SpriteTexture->GetSizeX()));
    const float TexHeight = FMath::Max(1.f, static_cast<float>(SpriteTexture->GetSizeY()));

    // 唯一一份 UV 子矩形：本体显示与地面阴影用的是同一块 Plane、同一个材质实例，
    // 这里改了，玩家看到的形状和阴影形状会同步改，不可能出现两者不一致。
    // 注意：Plane 网格朝向相机后，我们实际看到的是它的背面（TwoSided 才能显示），
    // 背面视角会让 U（水平）方向发生镜像，同时站立后 V（纵向）也与世界“上”相反，
    // 因此这里把 U、V 都反转采样，纠正“上下颠倒”与“左右动画互换”两个问题。
    const float OffsetU = (Frame.X + Frame.Width) / TexWidth;
    const float ScaleU = -(Frame.Width / TexWidth);
    const float OffsetV = (Frame.Y + Frame.Height) / TexHeight;
    const float ScaleV = -(Frame.Height / TexHeight);
    SpriteMaterial->SetVectorParameterValue(TEXT("SpriteUVOffset"), FLinearColor(OffsetU, OffsetV, 0.f, 0.f));
    SpriteMaterial->SetVectorParameterValue(TEXT("SpriteUVScale"), FLinearColor(ScaleU, ScaleV, 0.f, 0.f));

    // 平面尺寸随当前帧像素宽高与角色世界缩放联动。
    const float PlaneWidth = Frame.Width * SpriteSheet->Render.WorldScale / 100.f;
    const float PlaneHeight = Frame.Height * SpriteSheet->Render.WorldScale / 100.f;
    Visual->SetRelativeScale3D(FVector(PlaneWidth, PlaneHeight, 1.f));
}

void ARpgDemoCharacter::OrientVisualToFixedCamera()
{
    if (!Visual || !CameraBoom) return;

    // 角色是“立在地上的纸片人”：只绕世界 Z 轴转向相机的水平朝向（忽略相机俯仰角），
    // 让贴图始终竖直站立，而不是像之前那样把相机的俯仰角也算进法线，
    // 导致平面跟着相机俯角一起被“压平”贴在地上。俯仰产生的透视效果由真实 3D 相机自然呈现即可。
    Visual->SetUsingAbsoluteRotation(true);
    FVector CameraForwardFlat = CameraBoom->GetComponentRotation().Vector();
    CameraForwardFlat.Z = 0.f;
    if (!CameraForwardFlat.Normalize())
    {
        CameraForwardFlat = FVector::ForwardVector;
    }
    const FRotator FaceRotation = FRotationMatrix::MakeFromZY(-CameraForwardFlat, FVector::UpVector).Rotator();
    Visual->SetWorldRotation(FaceRotation);
}

void ARpgDemoCharacter::UpdateSpriteAnimation(float DeltaSeconds)
{
    if (!SpriteSheet || !SpriteTexture || !ActiveSpriteClip || SpriteSheet->ClipsByKey.IsEmpty()) return;
    if (!bKernelLocomotionIsMoving || ActiveSpriteClip->Frames.Num() <= 1) return;
    SpriteFrameElapsed += DeltaSeconds;
    const float FrameDuration = 1.f / FMath::Max(1.f, ActiveSpriteClip->FramesPerSecond);
    while (SpriteFrameElapsed >= FrameDuration)
    {
        SpriteFrameElapsed -= FrameDuration;
        SpriteFrameIndex = (SpriteFrameIndex + 1) % ActiveSpriteClip->Frames.Num();
        ApplySpriteFrame();
    }
}

void ARpgDemoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // 八方旅人式固定俯视镜头不接受鼠标控制旋转；偏航与俯仰仅由相机编辑器数据提供。
    PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ARpgDemoCharacter::Interact);
    PlayerInputComponent->BindAction(TEXT("Attack"), IE_Pressed, this, &ARpgDemoCharacter::Attack);
    PlayerInputComponent->BindAction(TEXT("Skill"), IE_Pressed, this, &ARpgDemoCharacter::UseSkill);
    PlayerInputComponent->BindAction(TEXT("Potion"), IE_Pressed, this, &ARpgDemoCharacter::UsePotion);
    PlayerInputComponent->BindAction(TEXT("DebugQuestStart"), IE_Pressed, this, &ARpgDemoCharacter::DebugQuestStart);
    PlayerInputComponent->BindAction(TEXT("DebugGetIncense"), IE_Pressed, this, &ARpgDemoCharacter::DebugGetIncense);
    PlayerInputComponent->BindAction(TEXT("DebugStartBattle"), IE_Pressed, this, &ARpgDemoCharacter::DebugStartBattle);
    PlayerInputComponent->BindAction(TEXT("ToggleSystemMenu"), IE_Pressed, this, &ARpgDemoCharacter::ToggleSystemMenu);
    PlayerInputComponent->BindAction(TEXT("MenuLeft"), IE_Pressed, this, &ARpgDemoCharacter::MenuLeft);
    PlayerInputComponent->BindAction(TEXT("MenuRight"), IE_Pressed, this, &ARpgDemoCharacter::MenuRight);
    PlayerInputComponent->BindAction(TEXT("MenuUp"), IE_Pressed, this, &ARpgDemoCharacter::MenuUp);
    PlayerInputComponent->BindAction(TEXT("MenuDown"), IE_Pressed, this, &ARpgDemoCharacter::MenuDown);
    PlayerInputComponent->BindAction(TEXT("MenuConfirm"), IE_Pressed, this, &ARpgDemoCharacter::MenuConfirm);
    PlayerInputComponent->BindAction(TEXT("MenuDelete"), IE_Pressed, this, &ARpgDemoCharacter::MenuDelete);
    PlayerInputComponent->BindAction(TEXT("ResetDemo"), IE_Pressed, this, &ARpgDemoCharacter::ResetDemo);
}

void ARpgDemoCharacter::Interact()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->HandleInteract();
}

void ARpgDemoCharacter::Attack()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->HandleBattleAttack();
}

void ARpgDemoCharacter::UseSkill()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->HandleBattleSkill();
}

void ARpgDemoCharacter::UsePotion()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->HandleBattlePotion();
}

void ARpgDemoCharacter::DebugQuestStart()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->DebugAdvanceDemo(1);
}

void ARpgDemoCharacter::DebugGetIncense()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->DebugAdvanceDemo(2);
}

void ARpgDemoCharacter::DebugStartBattle()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->DebugAdvanceDemo(3);
}

void ARpgDemoCharacter::ToggleSystemMenu()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->ToggleSystemMenu();
}

void ARpgDemoCharacter::MenuLeft()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->NavigateSystemMenu(-1);
}

void ARpgDemoCharacter::MenuRight()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->NavigateSystemMenu(1);
}

void ARpgDemoCharacter::MenuUp()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->NavigateSystemMenu(-10);
}

void ARpgDemoCharacter::MenuDown()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->NavigateSystemMenu(10);
}

void ARpgDemoCharacter::MenuConfirm()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->ExecuteSystemMenuAction();
}

void ARpgDemoCharacter::MenuDelete()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->DeleteSelectedSave();
}

void ARpgDemoCharacter::ResetDemo()
{
    if (ARpgDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARpgDemoGameMode>()) GameMode->ResetDemo();
}
