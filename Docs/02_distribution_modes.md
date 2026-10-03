# Distribution Modes

How clones get arranged in space. Each mode has its own parameters.

---

## Grid

The default. Clones arranged in a 3D grid pattern.

| Parameter | Description |
|-----------|-------------|
| Grid Count | X, Y, Z count |
| Grid Spacing | Distance between clones |
| Grid Offset | Shift the entire grid |

Use case: Walls of objects, floors, shelves, anything rectangular.

---

## Circle

Clones arranged in a ring. Good for radial patterns.

| Parameter | Description |
|-----------|-------------|
| Radius | Size of the circle |
| Count | How many clones around the ring |
| Arc Angle | Partial circle (360 = full) |
| Start Angle | Rotation offset |

Use case: Chandeliers, clock faces, radial menus, magical circles.

---

## Sphere

Clones distributed on a sphere surface. Fibonacci distribution for even spacing.

| Parameter | Description |
|-----------|-------------|
| Radius | Sphere size |
| Count | Total clones |

Use case: Disco balls, planet surfaces, explosion patterns.

---

## Line

Simple line between two points.

| Parameter | Description |
|-----------|-------------|
| Count | Clones along the line |
| Line Direction | Vector defining the line |
| Line Length | Total length |

Use case: Fences, chains, anything linear.

---

## Spline

Clones follow a spline component. The cloner has a built-in spline you can edit.

| Parameter | Description |
|-----------|-------------|
| Count | Clones along spline |
| Spline Offset | Shift along the spline |
| Use Spline Rotation | Align clones to spline tangent |

Use case: Roads, rivers, cables, vines, anything that curves.

Edit the spline: Select the cloner, then select the spline component in the Details panel. Now you can add/move spline points in the viewport.

---

## Surface

Clones scattered on a mesh surface. Needs a target mesh to scatter on.

| Parameter | Description |
|-----------|-------------|
| Surface Mesh | The mesh to scatter onto |
| Count | How many clones |
| Seed | Randomization seed |
| Align to Normal | Orient clones to surface normal |

Use case: Foliage, debris, decals on objects.

---

## Honeycomb

Hexagonal grid pattern. More organic than regular grid.

| Parameter | Description |
|-----------|-------------|
| Count X/Y | Grid dimensions |
| Cell Size | Hexagon size |

Use case: Beehives, sci-fi panels, organic patterns.

---

## Stacking Layers

You can have multiple distribution layers on one cloner. They all render the same mesh but with different arrangements.

In the Details panel, expand Distribution Layers and hit the + button to add more.

Example: Grid layer for the base + circle layer floating above = interesting hybrid pattern.

---

Next: [Modifiers](03_modifiers.md)
