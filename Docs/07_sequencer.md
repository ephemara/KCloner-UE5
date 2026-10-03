# Sequencer

Keyframe modifier properties directly in Unreal's Sequencer. Animate your cloner effects over time for cinematics.

---

## Adding a Cloner to Sequencer

1. Open a Level Sequence
2. Drag the K-Cloner actor into the Sequencer timeline
3. Expand the actor track

You'll see the standard transform track plus a "K-Cloner Modifiers" section.

---

## Keyframing Modifier Properties

1. Expand the K-Cloner Modifiers track
2. Each modifier appears as a sub-track
3. Click the + next to a modifier to add keyframeable properties
4. Right-click on a property > Add Key, or enable auto-key and scrub

All numeric modifier properties can be keyframed: speed, amplitude, offset, etc.

---

## Keyframeable Cloner Properties

Beyond modifiers, these cloner-level properties are also Interp-enabled:

- **TimeScale** - Global animation speed multiplier
- **GridCount** - Grid dimensions (for grid mode)
- **GridSpacing** - Spacing between clones
- **EffectorRadius** - Influence distance
- **EffectorFalloff** - Falloff curve

---

## Example: Wave Intensity Over Time

1. Add Wave modifier to your cloner
2. In Sequencer, expand the Wave modifier track
3. Add keyframe at frame 0: Wave Amplitude = 0
4. Add keyframe at frame 60: Wave Amplitude = 200
5. Add keyframe at frame 120: Wave Amplitude = 0

The wave builds up and then dies down.

---

## Example: Grid Morphing

1. Start with a 10x10x1 grid
2. Keyframe GridCount from (10, 10, 1) to (5, 5, 4)
3. Play the sequence

The flat grid morphs into a cube formation.

---

## Tips

- Use cubic interpolation for smooth transitions
- TimeScale at 0 freezes all modifier animations at current state
- Combine with camera animation for cinematic reveals
- Modifiers can be enabled/disabled mid-sequence (keyframe the Enabled property)

---

## Blueprint Control During Playback

If you need runtime control during Sequencer playback:

```
ClonerActor->SetTimeScale(NewValue);
ClonerActor->MarkModifiersDirty();
```

The cloner respects both Sequencer keys and runtime calls. Last write wins.

---

Next: [Blueprint API](08_blueprint_api.md)
