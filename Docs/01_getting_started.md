# Getting Started

The basics - drop a cloner in your level and start duplicating meshes.

## Your First Cloner

1. Right-click in the viewport or World Outliner
2. Select K-Studio > K-Cloner Actor
3. A cloner spawns with a default cube in a 3x3 grid

You now have 9 cubes. Change the mesh, change the count, add modifiers. Everything updates in real time.

---

## The Details Panel

All cloner settings live in the Details panel under the "K-Cloner" category. Here's the breakdown:

### Source
- **Source Mesh** - The static mesh to clone
- **Source Skeletal Mesh** - Use this instead for animated clones
- **Skeletal Mode** - Physics/IK (live animation) or VAT (baked, more performant)

### Distribution Layers
This is where you define how things get distributed. You can stack multiple layers.

- **Mode** - Grid, Circle, Spline, Surface, Sphere, Line, Honeycomb
- **Count** - How many clones per layer
- **Spacing/Radius** - Depends on the mode

### Modifiers
Animation and transform effects. Add as many as you want, they stack.

### Effectors
Runtime influence from other actors in your level. Proximity-based transforms.

---

## Quick Tips

- Hold Alt and drag to duplicate the cloner actor itself
- To create a KClonerActor blueprint, right click in content browser and create a new blueprint class. Click the "all" dropdown and search "KClonerActor"
- Modifiers run top to bottom - order matters
- Effectors need a KClonerEffectorComponent on the influencing actor
- Use VATBaked mode for 1000+ skeletal clones, PhysicsIK will impact performance

---

Next: [Distribution Modes](02_distribution_modes.md)
