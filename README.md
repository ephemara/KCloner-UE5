# K-Cloner

> **Unreal Engine 5.4 – 5.8** | C4D-Style MoGraph & Procedural Instancing Framework for UE5

[![Fab](https://img.shields.io/badge/Fab-Get%20on%20Fab-0052FF?style=flat&logo=unrealengine)](https://fab.com/s/0d51d354f6f8)
[![YouTube Demo](https://img.shields.io/badge/YouTube-Watch%20Demo-FF0000?style=flat&logo=youtube)](https://www.youtube.com/watch?v=xuypayM9kwU)

K-Cloner brings the flexibility of Cinema 4D-style MoGraph workflows directly into Unreal Engine. It provides a real-time procedural instancing, animation, and effector framework built natively in C++ for both **Static Meshes** and **Skeletal Meshes**.

Whether you need a field of 10,000 wind-reactive trees, dynamic crowd animations, weapon sway, complex combat impact fx, or generative mathematical structures, K-Cloner handles layout generation, modifier stacks, math-driven scripting, and high-performance rendering out of the box.

*Note: This repository has a clean commit history because K-Cloner was previously developed inside a private monorepo. It has now been separated and made public on GitHub.*

---

## Media & Links

- **Video Demonstration:** [Watch on YouTube](https://www.youtube.com/watch?v=xuypayM9kwU)
- **Fab Marketplace:** [Get K-Cloner on Fab](https://fab.com/s/0d51d354f6f8)

---

## Key Features

### 1. Core Distribution & Combinatorial Layouts
Stack multiple distribution layers (e.g. Radial on a Spline, or Hex-grid on a Mesh surface) using the `FKClonerDistributionLayer` architecture:

- **Radial:** Procedural circular distribution with tangential alignment.
- **Linear:** Step-based directional offset accumulation.
- **Spline:** Align to any `USplineComponent` path with tangents and distance offsets.
- **Honeycomb:** Hexagonal grid tessellation.
- **Scatter:** Volume- or box-bounded randomized point scattering.
- **Mesh Surface:** Point sampling via vertices, surface triangles, or mesh volume.
- **Spiral / Helix, Arc / Fan, Sphere / Fibonacci, Cone / Frustum:** Built-in generative shapes.
- **Single:** Precise manual placement layer.

### 2. Non-Destructive Modifier Stack & Effectors
Modifiers evaluate in real time and can be stacked, toggled, and blended non-destructively:

- **Audio Reactive:** Uses `AudioSynesthesia` to drive scale, position, rotation, or custom data from live or prerecorded audio spectrums.
- **Texture Sampling:** Sample grayscale textures to drive displacements, rotations, or scale across instances.
- **Organic Motion:** Realistic Wind Sway (multi-frequency sine waves with cross-axis depth) and Perlin/Curl noise fields.
- **Physics Emulation:** Squash-and-stretch Bounce, damped spring Elastic oscillation, and Pendulum swinging.
- **Flow & Propagation:** Wave propagation, Step cascades, and Time Delay (domino effects).
- **Transform Drivers:** Orbit, Float, Pulse, Tumble, and Vortex.
- **Math Curves:** Lissajous curves and Figure-8 paths.
- **Fields & Constraints:** Attract/Repel (with configurable falloff), Explosive Push, Gravity, and Look-At Target constraints (targeting actors or component tags).
- **Inheritance:** Smoothly morph instance transform matrices between two distinct `AKClonerActor` layouts in the world.

### 3. Hybrid Skeletal & VAT Rendering
Balance visual fidelity and performance when instancing hundreds or thousands of animated characters:

- **Physics / IK Mode:** Full skeletal mesh component evaluation for hero close-ups.
- **VAT / Baked Mode:** Instanced Vertex Animation Textures rendered on an HISM component with zero skeletal CPU overhead.
- **Distance-Based Auto LOD:** Seamlessly transitions from full skeletal evaluation to lightweight VAT rendering based on camera distance thresholds.

### 4. K-Script (Math Expression Engine)
Write custom procedural logic directly in the Details panel using K-Script, powered by the high-performance [ExprTk](https://www.partow.net/programming/exprtk/) library:

```c
// Example: Procedural sine wave with index offset
p.z += sin(t * v0 + i * v1) * v2;
```

- **Variables:** `t` (time), `dt` (delta time), `i` (instance index), `n` (total count), `u` (normalized index 0..1), `seed`, and user-defined slider parameters (`v0`, `v1`, ...).
- **Units:** Built-in scale helpers (`uu`, `cm`, `mm`, `m`, `deg`, `rad`).
- **DataAssets:** Save expressions into `UKClonerModifierPreset` assets with custom slider names and min/max ranges for easy team reuse.

### 5. External JSON Modifiers & Animation Effects
Modifier presets and animation reactions can be authored as JSON files without touching C++ or the Unreal editor:

- **`.kmod.json`:** Custom modifier definitions with mathematical expressions and exposed parameter sliders.
- **`.keffect.json`:** Multi-layer animation reactions that combine distribution modes, modifier stacks, blend times, and cloner bursts.
- **Hot Reloading:** Modify JSON files externally and run console commands in the editor:
  ```text
  KCloner.ReloadModifiers
  KCloner.ReloadAnimationEffects
  ```
- **AnimNotify Integration:** Trigger procedural reactions directly from animation sequences and montages using `KCloner Modifier`, `KCloner Modifier State`, `KCloner Distribution`, and `KCloner Distribution State` notifies.

### 6. Animation Authoring, Baking & Tracer Suite
- **Anim Tweak Mode:** Ingest an existing Skeletal Animation Sequence, apply procedural modifiers/noise on top, and bake the result into a clean new animation asset.
- **Bake to Static Mesh:** Merge thousands of cloner instances into a single optimized `UStaticMesh` with automatic LOD generation, collision, and distance field support.
- **1-Click VAT Generation:** Sample procedural animations into 8-bit, 16-bit, or 32-bit HDR Vertex Animation Textures with automatic Material Instance generation and UV wiring.
- **Tracer System:** `AKClonerTracer` records instance motion histories into real-time `USplineComponent` paths for ribbons, trails, or light streaks.

### 7. Blueprints & C++
Nearly every function, parameter, and distribution property is exposed to Blueprints. The underlying architecture is modular C++ optimized with spatial acceleration (`nanoflann`) and expression compilation (`ExprTk`).

---

## Requirements

- **Engine Versions:** Unreal Engine 5.4, 5.5, 5.6, 5.7, 5.8
- **Platform:** Windows (Win64)
- **Engine Plugins:**
  - `Niagara` (Required, enabled by default)
  - `AudioSynesthesia` (Optional, required for audio-reactive modifiers)
  - `GeometryCache` (Optional)

---

## Installation

### Option A: Pre-built Binaries (UE 5.8)
1. Download the pre-built `KCloner-UE5.8-Win64.zip` from the [GitHub Releases](../../releases) page.
2. Extract the `KCloner` folder into your project's `Plugins/` directory:
   ```text
   YourProject/
   └── Plugins/
       └── KCloner/
           ├── KCloner.uplugin
           ├── Binaries/
           ├── Content/
           ├── Resources/
           └── Source/
   ```
3. Open your project in Unreal Engine 5.8. When prompted, enable the plugin and restart.

### Option B: Building from Source (UE 5.4 – 5.8)
1. Clone this repository into your project's `Plugins/` folder:
   ```bash
   cd YourProject/Plugins
   git clone https://github.com/ephemara/KCloner-UE5.git KCloner
   ```
2. If building for UE 5.4 – 5.7, open `KCloner.uplugin` in a text editor and update `"EngineVersion"` to match your installed engine version (e.g. `"5.4.0"`).
3. Regenerate Visual Studio project files (right-click your `.uproject` → **Generate Visual Studio project files**).
4. Build your project in Visual Studio or Rider (Development Editor configuration).

---

## Quick Start

1. **Place a Cloner:** In the Place Actors panel or Content Browser, spawn a `KClonerActor` into your level.
2. **Assign a Mesh:** Select the actor and set your **Static Mesh** or **Skeletal Mesh** under the Cloner settings.
3. **Choose Distribution:** Pick a distribution mode (e.g. `Radial`, `Grid`, or `Spline`) and adjust count and radius.
4. **Add Modifiers:** Under the Modifier Stack, click **Add Modifier** and select `Wave`, `Sway`, `Bounce`, or `Audio`.
5. **Tweak Sliders:** Adjust frequency, amplitude, and falloff sliders in real time in the viewport.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
