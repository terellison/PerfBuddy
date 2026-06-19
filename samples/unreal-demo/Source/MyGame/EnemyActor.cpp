// Sample Unreal actor with intentional per-frame anti-patterns.
#include "EnemyActor.h"
#include "Kismet/GameplayStatics.h"

AEnemyActor::AEnemyActor() {
  PrimaryActorTick.bCanEverTick = true;  // pb_unreal: tick_count
}

void AEnemyActor::Tick(float DeltaSeconds) {
  Super::Tick(DeltaSeconds);

  // Expensive: world access + cast every frame -> pb_unreal: work_in_tick
  if (UWorld* World = GetWorld()) {
    AActor* Target = Cast<AActor>(CachedTarget);
    (void)Target;

    // Very expensive: iterate every actor every frame -> pb_unreal: get_all_actors
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(World, AEnemyActor::StaticClass(), Found);
  }
}
