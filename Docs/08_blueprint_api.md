# Blueprint API

Runtime control of cloners from blueprints and C++.

---

## Core Functions

### Query

| Function | Returns | Description |
|----------|---------|-------------|
| GetInstanceCount() | int | Number of active clones |
| GetCloneTransform(Index) | Transform | World transform of a specific clone |
| GetCloneLocation(Index) | Vector | World position of a clone |
| GetNearestCloneIndex(Location) | int | Index of clone closest to a world position |
| GetAllCloneTransforms() | Array<Transform> | All transforms (allocates, use sparingly) |
| IsAnimating() | bool | True if any modifiers are actively animating |
| GetEffectorInfluenceAtLocation(Location) | float | Effector influence at a point (0-1) |

### Control

| Function | Description |
|----------|-------------|
| ForceRebuild() | Regenerate all instances from scratch |
| SetCloneVisible(Index, Visible) | Show/hide specific clone |
| HideAllClones() | Hide everything |
| ShowAllClones() | Show everything |
| UpdateVFXComponent() | Refresh Niagara component |

### Modifiers

| Function | Description |
|----------|-------------|
| GetModifierByIndex(Index) | Get a specific modifier |
| GetModifierCount() | How many modifiers |
| AddModifierOfClass(Class) | Add a modifier at runtime |
| RemoveModifier(Modifier) | Remove a modifier |
| ClearAllModifiers() | Remove all modifiers |
| SetModifierEnabled(Index, Enabled) | Enable/disable a modifier |

### Animation

| Function | Description |
|----------|-------------|
| PlayClonerMontage(Montage, Rate, StartSection) | Play montage on all skeletal clones |
| ApplyPreset(KClonerData) | Apply settings from a preset asset |
| SetTimeScale(Value) | Set global time multiplier |

---

## Events

Bind to these in blueprint:

| Event | Description |
|-------|-------------|
| OnClonerRebuilt | Fired when instances are regenerated |
| OnClonerUpdated(DeltaTime) | Fired every frame after modifier update |
| OnCloneInteracted(Index, Location) | Fired when a clone is clicked (requires trace setup) |

---

## Example: Spawn Explosion at Nearest Clone

```
// On hit event
FVector HitLocation = HitResult.Location;
int32 Index = ClonerActor->GetNearestCloneIndex(HitLocation);
FTransform CloneTransform = ClonerActor->GetCloneTransform(Index);

// Spawn explosion at clone location
SpawnEmitterAtLocation(ExplosionFX, CloneTransform.GetLocation());

// Hide the clone
ClonerActor->SetCloneVisible(Index, false);
```

---

## Example: Add Modifier at Runtime

```
// Add a shake modifier when player enters zone
UKClonerModifier* Shake = ClonerActor->AddModifierOfClass(UKClonerModifier_Shake::StaticClass());

// Configure it
if (UKClonerModifier_Shake* ShakeMod = Cast<UKClonerModifier_Shake>(Shake))
{
    ShakeMod->ShakeIntensity = 50.0f;
    ShakeMod->ShakeSpeed = 10.0f;
}
```

---

## Example: Raycast to Clone

```
// Trace from camera
FHitResult Hit;
bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility);

if (bHit)
{
    // Check if we hit a cloner
    if (AKClonerActor* Cloner = Cast<AKClonerActor>(Hit.GetActor()))
    {
        // Find which clone was hit
        int32 Index = Cloner->GetNearestCloneIndex(Hit.Location);
        
        // Do something with it
        Cloner->SetCloneVisible(Index, false);
    }
}
```

---

## Blueprint Library

UKClonerBlueprintLibrary provides static utility functions:

| Function | Description |
|----------|-------------|
| FindAllClonersInWorld(World) | Get all cloners in level |
| FindClonersByTag(World, Tag) | Find cloners with specific tag |
| GetCloneAtScreenPosition(Cloner, ScreenPos) | 2D to 3D clone lookup |

---

## Performance Tips

- GetAllCloneTransforms allocates. Cache if calling frequently.
- ForceRebuild is expensive. Avoid calling every frame.
- SetCloneVisible is cheap. Use it for LOD or hide/show logic.
- Modifier property changes are applied next tick automatically.

---
