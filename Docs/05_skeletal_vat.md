# Skeletal Mesh & VAT

Cloning animated characters and creatures. Two modes: live skeletal animation or baked vertex animation textures.

---

## Skeletal Mesh Basics

Instead of Source Mesh, assign a Source Skeletal Mesh. The cloner will create skeletal mesh instances.

Set an Animation Sequence and all clones play that animation.

---

## Skeletal Modes

### Physics/IK (Live)

Full skeletal animation per clone. Each clone is a real USkeletalMeshComponent.

Pros:
- Full animation blending
- Physics and ragdoll
- IK support
- Runtime animation changes

Cons:
- Expensive
- 100 clones = 100 skeletal mesh components ticking

### VAT/Baked

Vertex animation textures. Animation is baked into textures, played back on static meshes.

Pros:
- Extremely performant
- 10,000+ animated clones no problem
- Uses HISM (hierarchical instanced static mesh)
- GPU-driven

Cons:
- No runtime animation changes
- No physics/ragdoll
- Requires baking step
- Material setup required

Use case: Massive crowds, forests of animated trees, swarms.

### Auto (Distance-Based)

Hybrid mode. Near clones use Physics/IK, far clones use VAT.

Set the LOD distance threshold. Clones dynamically switch based on camera distance.

Use case: Best of both worlds when you need close-up detail but also scale.

---

## VAT Workflow

### 1. Bake the VAT

With a skeletal cloner selected:
1. Set Skeletal Mode to VATBaked (or have an animation assigned in AnimTweakMode)
2. Open the K-Cloner Data asset in the asset editor
3. Click "Bake Animation from Preset" in the toolbar
4. This generates:
   - Position texture (vertex offsets per frame)
   - Rotation texture (vertex rotations per frame) 
   - A material instance

### 2. The Material

K-Cloner auto-generates a VAT material instance. It reads the baked textures and reconstructs animation in the vertex shader.

The base material is at `/KCloner/Materials/M_KCloner_VAT_Base`.

You can override BaseVATMaterial on the cloner if you want custom shading.

### 3. Per-Instance Custom Data

VAT clones use per-instance custom data to offset animation time. This means each clone can be at a different point in the animation cycle.

Modifiers can drive this - the Delay modifier is great for staggering animation timing.

---

## Animation Tweaking

AnimTweakMode lets you apply modifier effects to the actual animation, not just the transforms.

1. Enable bAnimTweakMode
2. Assign an AnimationSequence
3. Add modifiers that affect position/rotation
4. Bake the result

The output is a new animation sequence with the modifier effects permanently baked in.

Use case: You have a walk cycle but want each clone's walk to be slightly different. Add Random modifier, tweak, bake. Now you have a unique animation.

---

## Baking to Skeletal Mesh

Want to export the entire cloner as a single skeletal mesh?

1. Open the K-Cloner Data asset
2. Use the Bake to Anim button
3. Choose output location
4. Get a skeletal mesh + animation sequence

Each clone becomes a bone in the skeleton. The animation sequence contains the combined motion of all clones.

---

Next: [Niagara Integration](06_niagara.md)
