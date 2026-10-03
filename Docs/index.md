# K-Cloner Documentation

Documentation for K-Cloner, a procedural cloning, MoGraph, and real-time animation system for Unreal Engine.

---

## Guide & Reference Pages

1. [Getting Started](01_getting_started.md) - Basic setup and first cloner
2. [Distribution Modes](02_distribution_modes.md) - Grid, circle, spline, surface, etc
3. [Modifiers](03_modifiers.md) - Animation and transform modifiers
4. [Effectors](04_effectors.md) - Runtime influence system
5. [Skeletal Mesh & VAT](05_skeletal_vat.md) - Animated clones and vertex animation textures
6. [Niagara Integration](06_niagara.md) - Particle effects on clones
7. [Sequencer](07_sequencer.md) - Keyframing modifier properties
8. [Blueprint API](08_blueprint_api.md) - Runtime control functions

---

## Recent Architecture & Features (Not in Legacy Docs)

The original docs above cover the core cloning workflow. Newer releases include several major architectural additions:

### 1. Standalone `UKClonerModifierComponent`
You are no longer limited to cloning actors. You can attach `UKClonerModifierComponent` directly onto **any** standard Actor, Character, or Weapon in your level to apply procedural MoGraph modifier stacks (sway, bounce, wave, noise, K-Script, audio) directly to that actor or its components.

### 2. External JSON Modifier Preset System (`.kmod.json`)
Author custom math-driven modifiers in lightweight JSON files. Expressions are evaluated in real time via ExprTk with support for custom sliders, timing controls, and context variables (`t`, `dt`, `i`, `n`, `u`, `seed`).
- Files located in `Content/Modifiers/` auto-import into `/Game/KCloner/ModifierPresets/`.
- Hot-reload at any time in the editor console using:
  ```text
  KCloner.ReloadModifiers
  ```

### 3. AnimNotify & Animation Effect Presets (`.keffect.json`)
Integrate procedural MoGraph layers directly into Skeletal Animation Montages and Sequences:
- **Dedicated Notifies:** `KCloner Modifier`, `KCloner Modifier State`, `KCloner Distribution`, and `KCloner Distribution State`.
- Define multi-modifier cascades, blend-in/out curves, and transient cloner bursts in `.keffect.json` files imported into `/Game/KCloner/AnimationEffects/`.
- Hot-reload effect sources at any time in the editor console using:
  ```text
  KCloner.ReloadAnimationEffects
  ```

### 4. Extended Distribution Modes
The distribution architecture now supports advanced layout modes beyond the original basics:
- **Spiral / Helix**, **Arc / Fan**, **Sphere / Fibonacci**, **Cone / Frustum**, and **Poisson (blue-noise)** scatter.

---

## Support

- **Fab Store:** [K-Cloner on Fab](https://fab.com/s/0d51d354f6f8)
- **Demo & Overview:** [YouTube Video](https://www.youtube.com/watch?v=xuypayM9kwU)
- **Email:** taylorkipp@greeble.co
