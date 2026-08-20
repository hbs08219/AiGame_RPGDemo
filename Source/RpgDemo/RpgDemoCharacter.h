#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "RpgDemoCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UTexture2D;
class UMaterialInstanceDynamic;

UCLASS()
class RPGDEMO_API ARpgDemoCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ARpgDemoCharacter();
    void ApplyToolGenConfig(class URpgDemoToolGenSubsystem* ToolGen);

    /** UE 3C Adapter 命令：位移数值来自 TS Kernel，碰撞由胶囊扫掠执行。 */
    void ApplyKernelMove(const FVector2D& WorldDelta);
    /** UE 3C Adapter 命令：状态/朝向由 TS Kernel 选择，UE 仅更新精灵表现。 */
    void ApplyKernelAnimationState(const FString& StateId, const FString& Direction);
    /** UE 3C Adapter 命令：相机模式由 TS Kernel 请求，具体 Pose 由项目设置和 Camera asset 决定。 */
    void ApplyKernelCameraMode(const FString& ModeId);
    /** 将项目设置解析出的默认 Camera pose 应用到固定俯视镜头。 */
    void ApplyToolGenCameraPose();

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void UpdateSpriteAnimation(float DeltaSeconds);
    // 唯一的可见几何体：Visual（Plane + 遮罩材质）同时承担“显示精灵”与“投射精灵形状阴影”两个职责，
    // 保证两者的 UV / 缩放 / 朝向永远来自同一份计算，不会出现显示与阴影不一致的问题。
    void ApplySpriteFrame();
    void OrientVisualToFixedCamera();
    // 冷启动时精灵材质的着色器/PSO 尚未编译完成，若立即显示会先短暂渲染出未生效材质参数
    // （表现为整张精灵表贴图直接显示，没有按当前帧裁剪）。延迟一小段时间后再显示可规避此问题。
    void RevealVisual();
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Interact();
    void Attack();
    void UseSkill();
    void UsePotion();
    void DebugQuestStart();
    void DebugGetIncense();
    void DebugStartBattle();
    void ToggleSystemMenu();
    void MenuLeft();
    void MenuRight();
    void MenuUp();
    void MenuDown();
    void MenuConfirm();
    void MenuDelete();
    void ResetDemo();

    UPROPERTY(VisibleAnywhere, Category="RPG Demo")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, Category="RPG Demo")
    UCameraComponent* FollowCamera;

    UPROPERTY(VisibleAnywhere, Category="RPG Demo")
    UStaticMeshComponent* Visual;

    const struct FRpgToolGenSpriteSheet* SpriteSheet = nullptr;
    const struct FRpgToolGenSpriteClip* ActiveSpriteClip = nullptr;
    UTexture2D* SpriteTexture = nullptr;
    UMaterialInstanceDynamic* SpriteMaterial = nullptr;
    FString LastSpriteDirection = TEXT("down");
    bool bKernelLocomotionIsMoving = false;
    int32 SpriteFrameIndex = 0;
    float SpriteFrameElapsed = 0.f;
    FTimerHandle RevealVisualTimerHandle;
};
