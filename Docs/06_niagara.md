# Niagara Integration

Spawn particle effects at clone positions. Fire, smoke, sparks, whatever.

---

## Quick Setup

1. In the cloner details, find the VFX category
2. Assign a Niagara System to the "Niagara System" property
3. Done. Particles spawn at clone positions.

The VFXScale property controls particle size multiplier.

---

## How It Works

The cloner spawns a UNiagaraComponent and feeds it clone position data through a custom data interface (UKClonerDataInterface).

Your Niagara system needs to be set up to read from this data interface.

---

## Creating Compatible Niagara Systems

### Using the Data Interface

1. Create a new Niagara System
2. Add a User Parameter of type "K-Cloner Data Interface" named "KClonerSource"
3. In your emitter, use the GetCloneTransform or GetCloneCount functions to read clone data

### Available Functions

| Function | Returns | Description |
|----------|---------|-------------|
| GetCloneCount | int | Total number of clones |
| GetCloneTransform | Transform | World transform of clone at index |
| GetCloneColor | LinearColor | Color/custom data of clone at index |

### Spawn Behavior

Typical setup:
- Spawn burst of particles equal to clone count
- Each particle gets its index
- Particle samples GetCloneTransform(index) to position itself
- Particle follows or spawns effects at that position

---

## Example: Fire at Each Clone

Emitter setup:
1. Spawn mode: Burst, count = GetCloneCount()
2. Particle spawn: Set position = GetCloneTransform(Particle.Index).Location
3. Add fire sprite renderer
4. Loop or one-shot depending on your needs

The particles will appear at each clone location.

---

## Example: Trail Following Clones

Emitter setup:
1. Spawn mode: Continuous
2. Particle spawn: Randomly sample clone indices
3. Particle update: Move toward GetCloneTransform(index).Location
4. Add ribbon renderer for trails

Creates trails that follow clone movement over time.

---

## Performance Notes

- The data interface is GPU-compatible
- Massive clone counts work fine
- Particle counts are independent of clone counts
- You can spawn way fewer particles than clones if needed (sample randomly)

---

## Clearing VFX

Set Niagara System back to None. The component gets destroyed.

Or call UpdateVFXComponent() from blueprint after clearing the property.

---

Next: [Sequencer](07_sequencer.md)
