#include "RpgDemoNpc.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"

ARpgDemoNpc::ARpgDemoNpc()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    // NPC 的外观由 NPC/角色外观编辑器提供；未发布外观时只显示交互标签，绝不回退为圆柱白模或阻挡玩家。
    Mesh->SetStaticMesh(nullptr);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetVisibility(false, true);
    Mesh->SetHiddenInGame(true, true);

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(RootComponent);
    Label->SetRelativeLocation(FVector(0.f, 0.f, 125.f));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(30.f);
    Label->SetTextRenderColor(FColor(255, 236, 145));
}

void ARpgDemoNpc::Configure(const FString& InId, const FString& InDisplayName, const FLinearColor& InColor, float InInteractRadius)
{
    Id = InId;
    InteractRadius = FMath::Max(1.f, InInteractRadius);
    Label->SetText(FText::FromString(InDisplayName + TEXT("\n[E] 交互")));
    Label->SetTextRenderColor(InColor.ToFColor(true));
}
