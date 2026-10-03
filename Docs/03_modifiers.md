# Modifiers

Modifiers animate and transform clones. Stack them, keyframe them, go wild.

---

## Adding Modifiers

In the Details panel under Modifiers, click the + dropdown. Pick a modifier type. It gets added to the stack and immediately starts affecting clones.

Modifiers run top to bottom. The order matters - orbit before wave gives different results than wave before orbit.

---

## Modifier Types

### Orbit
Clones rotate around their origin point.

- **Orbit Speed** - Rotations per second
- **Orbit Axis** - Which axis to rotate around
- **Orbit Radius** - Distance from center

### Float
Clones bob up and down.

- **Float Speed** - Oscillation frequency
- **Float Height** - Amplitude
- **Float Offset** - Phase offset per clone

### Pulse
Clones scale in and out.

- **Pulse Speed** - Oscillation frequency
- **Pulse Amount** - Scale delta
- **Pulse Offset** - Phase offset per clone

### Wave
Creates a wave pattern through clones.

- **Wave Speed** - How fast the wave moves
- **Wave Amplitude** - Height of wave
- **Wave Frequency** - How tight the waves are
- **Wave Direction** - Which axis the wave travels

### Shake
Random jittering.

- **Shake Intensity** - How much movement
- **Shake Speed** - How fast
- **Position/Rotation/Scale** - Which transforms to shake

### Noise
Perlin noise-based movement.

- **Noise Scale** - Frequency of noise
- **Noise Amplitude** - Strength of effect
- **Noise Speed** - How fast it evolves

### Random
Random static offset per clone.

- **Random Position** - Position variance
- **Random Rotation** - Rotation variance
- **Random Scale** - Scale variance
- **Seed** - Randomization seed

### Step
Staggered offset based on clone index.

- **Step Position** - Cumulative position offset
- **Step Rotation** - Cumulative rotation offset
- **Step Scale** - Cumulative scale offset

### Delay
Staggers animation timing per clone.

- **Delay Amount** - Time offset between clones
- **Delay Mode** - Linear, Ping-Pong, Random

### Color
Tints clones. Requires material with vertex color support.

- **Color A / Color B** - Gradient endpoints
- **Color Mode** - How color is distributed

### Attract
Pulls clones toward a point.

- **Attract Target** - World position or actor reference
- **Attract Strength** - How strong the pull
- **Attract Falloff** - Distance-based falloff

### Target
Makes clones look at or move toward a target.

- **Target Actor** - What to track
- **Target Mode** - LookAt, MoveToward

### Push
Pushes clones away from a point. Opposite of attract.

### Vortex
Swirling motion toward center.

- **Vortex Strength** - Rotation intensity
- **Vortex Pull** - Inward movement

### Tumble
Continuous rotation on all axes.

- **Tumble Speed** - Rotation speed per axis

### Elastic
Spring-based motion.

- **Elastic Stiffness** - Spring constant
- **Elastic Damping** - How fast it settles

### Audio
React to audio input. Requires audio analysis setup.

- **Audio Source** - Audio component to analyze
- **Frequency Band** - Bass, mid, treble
- **Response Mode** - Scale, position, rotation

### Texture
Sample a texture to drive transforms.

- **Sample Texture** - Texture to read
- **Sample Channel** - R, G, B, A
- **Sample Mode** - How UV is calculated

### KScript
Custom expression-based modifier. Uses ExprTk math expressions.

- **Position Expression** - Math formula for position
- **Rotation Expression** - Math formula for rotation
- **Scale Expression** - Math formula for scale

Variables available: index, count, time, x, y, z, random

Example: `sin(time + index * 0.5) * 100` for a wave effect

### Custom
Blueprint-implementable modifier. Override ReceiveApplyCustomEffect to write your own logic.

---

## Common Patterns

**Cascading wave**: Float modifier with high Delay offset

**Breathing effect**: Pulse modifier, slow speed, all clones synced

**Chaotic scatter**: Random + Shake modifiers stacked

**Spiral motion**: Orbit + Step (small rotation per clone)

**Audio reactive**: Audio modifier on scale, Pulse on position for baseline animation

---

## Keyframing Modifiers

All modifier properties can be keyframed in Sequencer. See [Sequencer](07_sequencer.md) for details.

---

Next: [Effectors](04_effectors.md)
