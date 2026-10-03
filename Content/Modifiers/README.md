# External K-Cloner Modifiers

The K-Cloner editor imports every `*.json` file in this folder and writes real preset assets to `/Game/KCloner/ModifierPresets/`.

A file can contain one preset or a pack with a `Presets` array. The current source format maps directly to `UKClonerModifierPreset` and its ExprTk expressions. You can either use the split position/rotation/scale fields or provide one `KScript` block; `p.x/p.y/p.z`, `r.x/r.y/r.z`, and `s.x/s.y/s.z` are normalized automatically:

```json
{
  "AssetName": "Enemy_Bounce",
  "DisplayName": "Enemy Bounce",
  "Category": "Gameplay/Enemy",
  "Description": "A lightweight procedural enemy bounce.",
  "PositionExpression": "z := z + abs(sin(t * v0 + i * v1)) * v2;",
  "RotationExpression": "",
  "ScaleExpression": "",
  "KScript": "p.z := p.z + abs(sin(t * v0 + i * v1)) * v2;",
  "Step": 0.1,
  "SpeedMultiplier": 1.0,
  "Variables": [
    { "Name": "Speed", "DefaultValue": 2.0, "MinValue": 0.0, "MaxValue": 10.0 },
    { "Name": "Phase", "DefaultValue": 0.25, "MinValue": 0.0, "MaxValue": 6.28 },
    { "Name": "Height", "DefaultValue": 20.0, "MinValue": 0.0, "MaxValue": 200.0 }
  ]
}
```

KScript 2.0 context variables include `t/time`, `dt/delta_time`, `i/index`, `n/count`, `u/normalized`, `duration`, `loop`, `direction`, and `seed`. Unit constants are available as `uu`, `cm`, `mm`, `m`, `deg`, and `rad` (`m * 0.25` is 25 Unreal units).

For full effect-notify reactions, use a `.keffect.json` source with a `KScript` modifier. The animation effect importer can configure the same timing fields (`TimeScale`, `Duration`, `bLoop`, `bPingPong`, `IndexPhase`, and `Seed`) and the AnimNotify effect system can trigger/cross-fade the resulting stack.

Run `KCloner.ReloadModifiers` in the editor console after editing a source file. Imported entries appear under **Modifier Library** on K-Cloner actors, K-Cloner data assets, and `KClonerModifierComponent` details.

## Animation effects (`*.keffect.json`)

Animation reaction presets live beside modifier sources and are imported into `/Game/KCloner/AnimationEffects/`. They can be assigned directly to `KCloner Event` and `KCloner Event State` animation notifies.

```json
{
  "AssetName": "Enemy_Attack_Burst",
  "DisplayName": "Enemy Attack Burst",
  "Category": "Gameplay/Enemy",
  "DefaultLayer": "Combat",
  "Duration": 0.35,
  "BlendIn": 0.02,
  "BlendOut": 0.16,
  "Modifiers": [
    { "Type": "Bounce", "Speed": 3.2, "Height": 22.0, "Squash": 0.35 },
    { "Type": "Lissajous", "Speed": 1.6, "Size": 12.0 }
  ],
  "DistributionLayers": [
    { "Mode": "Radial", "RadialCount": 8, "RadialRadius": 70.0 }
  ],
  "SpawnDistributedCloner": true,
  "UseOwnerMesh": true
}
```

Modifier objects use their native class display names (`Bounce`, `Lissajous`, `Pendulum`, `Shake`, etc.) and their reflected property names. Vectors may be written as `[x, y, z]` or `{ "X": x, "Y": y, "Z": z }`.

Distribution layers support the original `Grid`, `Radial`, `Linear`, `Spline`, `Single`, `Honeycomb`, `Scatter`, and `Mesh` modes plus experimental `Arc`/`Fan`, `Spiral`/`Helix`, `Sphere`/`Fibonacci`, `Cone`/`Frustum`, and `Poisson`/blue-noise scatter modes. Their reflected fields are available directly in the distribution layer details and in `.keffect.json` files. Example:

```json
{
  "Mode": "Spiral",
  "SpiralCount": 48,
  "SpiralInnerRadius": 10.0,
  "SpiralOuterRadius": 180.0,
  "SpiralTurns": 2.5,
  "SpiralHeight": 240.0,
  "bSpiralAlign": true
}
```

Run `KCloner.ReloadAnimationEffects` after editing effect sources. The importer creates real DataAssets, so the animation notify details panel can pick them and changes can be iterated externally without editing the montage itself.

### Notify workflow

The animation editor exposes dedicated notify types:

- `KCloner Modifier` is an instant event and triggers a timed effect.
- `KCloner Modifier State` owns an effect for the notify window and blends it out on `NotifyEnd`.
- `KCloner Distribution` creates a transient distributed cloner burst.
- `KCloner Distribution State` keeps a distributed burst alive for the notify window.

Add `KClonerModifierComponent` and `KClonerAnimation` to the same skeletal actor, then place one of the K-Cloner notifies in a montage or animation sequence. The K-Cloner notify details customization includes:

- categorized external **Effect Library** picker
- external **Modifier Library** picker
- **Copy Selected Preset to Inline Stack** for per-notify iteration
- inline modifier objects with the normal K-Cloner modifier details
- inline distribution layers and a radial-distribution quick setup

The editor preview bridge also creates transient K-Cloner components for Persona/debug skeletal meshes, including preview widgets that do not have an owning gameplay actor. This keeps preview-only state isolated from the level actor and supports multiple preview skeletal meshes independently.

Optional `SpawnDistributedCloner` creates a transient cloner burst using the notify preview/actor mesh and `DistributionLayers`. Distributed bursts are visual effects. Gameplay collision/hitbox windows should remain an explicit combat component/notify state so visual clone counts do not silently become damage sources.
