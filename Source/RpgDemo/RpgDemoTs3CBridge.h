#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RpgDemoTs3CBridge.generated.h"

class ARpgDemoCharacter;

/**
 * UE 侧 3C Adapter：只提供引擎事实与命令执行。
 * 角色状态机、输入解释、朝向和动画状态选择由 TypeScript Gameplay Kernel 保持权威。
 */
UCLASS()
class RPGDEMO_API URpgDemoTs3CBridge : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** 根据 AGMaker Input/Default.json 查询当前语义 Move 轴；返回 X=左右，Y=前后。 */
    UFUNCTION(BlueprintCallable, Category = "AGMaker|3C")
    FVector2D ReadMoveInput() const;

    /** 按 WorldScene 数据放置玩家；TS Gameplay 决定逻辑出生点，UE 只执行世界变换。 */
    UFUNCTION(BlueprintCallable, Category = "AGMaker|3C")
    void PlacePlayer(float X, float Y, float Z);

    /** 将 TS Kernel 的逻辑位移请求交给 UE 胶囊碰撞适配层。 */
    UFUNCTION(BlueprintCallable, Category = "AGMaker|3C")
    void ApplyMoveIntent(float DeltaX, float DeltaY);

    /** 将 Kernel 的语义动画状态交给 UE 表现层。 */
    UFUNCTION(BlueprintCallable, Category = "AGMaker|3C")
    void ApplyAnimationState(const FString& StateId, const FString& Direction);

    /** 将 Kernel 的逻辑相机模式交给当前引擎表现配置。 */
    UFUNCTION(BlueprintCallable, Category = "AGMaker|3C")
    void ApplyCameraMode(const FString& ModeId);

    UFUNCTION(BlueprintPure, Category = "AGMaker|3C")
    FVector2D GetLogicalPosition() const;

private:
    ARpgDemoCharacter* GetPlayerCharacter() const;
    bool IsConfiguredKeyDown(const FString& KeyName) const;
};
