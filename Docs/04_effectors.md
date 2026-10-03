# Effectors

Runtime influence system. Actors in your level can push, pull, or transform nearby clones based on proximity.

---

## How It Works

1. Add a KClonerEffectorComponent to any actor
2. Set the influence radius and shape
3. Clones within range get affected based on distance

The cloner uses a KD-tree for fast spatial queries - thousands of clones with multiple effectors still runs smooth.

---

## Setting Up an Effector

### On the Influencing Actor

1. Select the actor you want to use as an effector (character, projectile, whatever)
2. Add Component > Search "KClonerEffector"
3. Configure the effector settings

### On the Cloner

1. Enable **Use Effectors** in the cloner details
2. Set **Effector Radius** - Max distance to search for effectors
3. Set **Effector Falloff** - How quickly influence drops off with distance

---

## Effector Properties

| Property | Description |
|----------|-------------|
| Shape | Sphere, Box, Cylinder |
| Radius | Influence distance |
| Falloff | 0 = hard edge, 1 = smooth gradient |
| Position Influence | How much to push/pull position |
| Rotation Influence | How much to rotate affected clones |
| Scale Influence | How much to scale affected clones |
| Invert | Flip the effect direction |

---

## Effector Shapes

**Sphere** - Radial influence from center. Most common.

**Box** - Rectangular influence zone. Good for doors, walls.

**Cylinder** - Circular influence with vertical extent. Good for pillars.

---

## Use Cases

**Character walking through grass**: Effector on character pushes grass clones aside

**Explosion ripple**: Expanding sphere effector triggers scale pulse

**Magnetic objects**: Effector attracts nearby clone debris

**Spotlight reveal**: Effector scales up clones when they enter the light cone

---

## Performance Notes

- Effectors are evaluated every frame by default
- Use UpdateSkipFrames on the cloner to reduce update frequency if needed
- The KD-tree rebuild happens when clones move, not every frame
- For static clones with moving effectors, this is very cheap

---

## Blueprint Access

```
// Get influence at a point
float Influence = ClonerActor->GetEffectorInfluenceAtLocation(WorldLocation);
```

You can also manually find all effectors affecting a location using FKClonerEffectorFinder.

---

Next: [Skeletal Mesh & VAT](05_skeletal_vat.md)
