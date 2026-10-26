# Cloth and hair physics (.phys)

A `.phys` file gives physics to the cloth and the hair of a model.

Put the file next to the `.glm` file, with the same name:
`models/players/<name>/model.glm` reads `models/players/<name>/model.phys`.
You do not change the model files.

The renderers `rdsp-vulkan` (raster) and `rdsp_swgl` use the file.
The path tracer (RTX) does not use it: the cloth does not move.

## Example

```
cloth cape
{
	surfaces	torso_cape
	pin			gradient thoracic -z 2 22
	material	cotton
	reach		26 38
	collide		humanoid
}

hair hair
{
	surfaces	head_hair
	pin			gradient cranium -z -6 0
	mode		lag
	reach		4
}
```

There are more files in `docs/phys/`.

## File

- A file has one or more groups.
- A group starts with `cloth <name>` or `hair <name>`. The lines of the group are between `{` and `}`.
- `cloth` and `hair` give different default values.
- `//` starts a comment.
- Upper case and lower case are the same.

The renderer moves all the surfaces of a group as one piece of cloth.
Vertices at the same position become one point, also on different surfaces.
Thus a robe in several surfaces stays closed.

## Surfaces

`surfaces <name> [<name> ...]`

- Each name is a surface of the model.
- `*` matches a set of characters. `?` matches one character.
- A surface that is off (skin `*off`, `_off`, `G2_SetSurfaceOnOff`) is not in the group. When the surface is on again, it is in the group again.

## Pins

A pin line sets which part of the cloth stays on the body.
Each point gets a value from 0 (fixed to the animation) to 1 (free).

- With no pin line, all the points are free.
- With more than one pin line, each point gets the smallest value.
- All distances are in the bind pose of the model. Z is up.

| Rule | Meaning |
| --- | --- |
| `pin gradient <bone> <axis> <fixed> <free>` | Distance from the bone along the axis (`x`, `y`, `z`, `-x`, `-y` or `-z`). At `<fixed>` the point is fixed. At `<free>` the point is free. |
| `pin radius <bone> <fixed> <free> [<x> <y> <z>]` | Distance from the bone. At `<fixed>` the point is fixed. At `<free>` the point is free. The offset moves the center, for example to the middle of the head. |
| `pin bones <bone> [<bone> ...]` | A point that the listed bones move is fixed. |
| `pin uv <fixed> <free>` | Same as `gradient`, with the texture coordinate v. |

## Material

These values tell what the cloth is.

| Keyword | Default cloth / hair | Meaning |
| --- | --- | --- |
| `material <name>` | none | Sets `weight`, `stretch`, `stiffness` and `air` for a type of cloth: `silk`, `cotton`, `leather` or `heavy`. A line after this line changes one of the values. |
| `weight` | 0.5 / 0.3 | Weight of the cloth. Up: the cloth hangs lower. Down: the cloth is light and floats. |
| `stretch` | 0.9 / 1 | 0 to 1. Resistance to stretch. 1: the cloth does not stretch. Down: the cloth is elastic and gets longer. |
| `stiffness` | 0.15 / 0.3 | 0 to 1. Resistance to folds. Up: leather, large soft folds. Down: silk, many small folds. |
| `air` | 0.5 / 0.6 | 0 to 1. Effect of the air. Up: the cloth fills with air when it moves, moves slowly and flaps in the wind of the map. 0: no air. |

The values of each material:

| Material | Weight | Stretch | Stiffness | Air |
| --- | --- | --- | --- | --- |
| `silk` | 0.3 | 0.9 | 0.05 | 0.8 |
| `cotton` | 0.5 | 0.9 | 0.15 | 0.5 |
| `leather` | 0.8 | 1 | 0.5 | 0.3 |
| `heavy` | 1 | 1 | 0.3 | 0.2 |

## Fit on the body

These values tell how the character wears the cloth.

| Keyword | Default cloth / hair | Meaning |
| --- | --- | --- |
| `hold` | 0 / 0.5 | 0 to 1. Pull back to the animated shape, as the anim drive of the Unreal cloth. Up: the cloth stays on the animation. 0: no pull, as the Unity cloth. |
| `bounce` | 0.8 / 0.5 | 0 to 1. 0: the cloth stops without a bounce. Up: the cloth swings for a long time. The brake applies to the speed relative to the model frame. |
| `inertia` | 0.7 / 0.5 | 0 to 1. Part of the change of the speed of the model frame (move and turn) that the cloth does not take, as the inertia of NvCloth. Up: the cloth trails behind when the character starts, stops, jumps or turns. Down: the cloth moves with the body. The model frame is the pinned particles of the group, as one rigid piece (the reference bone of Chaos), not the entity: the entity turns with the legs. |
| `shape` | 0 | 0 to 1. Each zone of the cloth keeps its animated shape (shape matching, Muller et al. 2005). Up: a tube (lekku, tail, sleeve) stays round when it swings. 0: off. |
| `reach <r> [<across>]` | 24 / 8 | Largest distance from the animation, in units. `r` is along the cloth. `across` is across the cloth (default `r`). A small `across` keeps the thickness of the garment, and the cloth does not go into the legs or the body. |
| `mode cloth` or `mode lag` | cloth | `cloth`: each part of the cloth moves on its own. `lag`: the free part moves as one soft piece around the fixed part. Use `lag` for hair that is part of the head surface. In `lag` mode, `hold`, `bounce`, `air` and `reach` apply. |

With `hold`, at rest the weight pulls the cloth down from the animation by about
`weight * 2 / (hold * hold)` units. Keep this value less than `reach`.
If it is more, the cloth hangs on its limit.

Each free point stays within its bind pose distance of the nearest fixed point (tether, as in NvCloth
and the Unreal cloth). Thus the cloth does not stretch away from the fixed part.

## Collisions

| Line | Meaning |
| --- | --- |
| `collide humanoid [<scale>]` | Adds capsules for the stock `_humanoid` skeleton: hips, torso, head, arms and legs. `scale` multiplies their radius. |
| `collide model [<scale>]` | Adds capsules that fit the model. Each bone that moves body vertices gets a capsule to its child bone, or a sphere at the end of a chain. The surfaces of the groups, the tags and the caps are not used. `scale` multiplies the radius. 32 colliders at most. |
| `collide radius <bone> <value>` | Sets the radius of the `humanoid` or `model` collider that starts on the bone. |
| `collide remove <bone>` | Removes the `humanoid` or `model` collider that starts on the bone. |
| `collide capsule <boneA> <boneB> <radius>` | Adds a capsule between two bones. The short form `collide <boneA> <boneB> <radius>` is the same. |
| `collide sphere <bone> <radius> [<x> <y> <z>]` | Adds a sphere on a bone. The offset is in the bind pose of the model. |
| `collide box <bone> <sx> <sy> <sz> [<round>] [<x> <y> <z>]` | Adds a box on a bone. `sx`, `sy` and `sz` are the half sizes along X, Y and Z of the bind pose. `round` is the radius of the edges and the corners (0: sharp). The offset moves the center from the bone. The box moves and turns with the bone. |

`collide radius` and `collide remove` do not change a `capsule`, `sphere` or `box` line.

How the collisions work:

- A point never goes nearer to a collider than in the bind pose of the model. Thus a collider that already holds a part of the cloth at rest (a collar, hair on the head) does not push it out.
- The animation does not change the colliders. When an arm or a leg goes through the animated cloth, it pushes the simulated cloth.
- The colliders move smoothly between two frames. The cloth step is 1/480 s, so in one step a fast arm or leg moves less than its radius. Thus it does not jump over the cloth.
- A collider with an unknown bone is not used.
- The floor is the shadow plane of the entity (`refEntity_t::shadowPlane`), if the entity has `RF_SHADOW_PLANE`.

## Settings

| Keyword | Default | Meaning |
| --- | --- | --- |
| `detail` | 1 | Density of the simulation mesh. 1 gives cells of 4 units, 2 gives cells of 2 units. Up: more folds, slower. Down: fewer folds, faster. A cell is fixed when most of it is fixed. |
| `subdivide` | 0 | 0 to 2. Makes the drawn mesh smoother: each level cuts each triangle in 4. The simulation does not change. The fixed part is not changed, so the cloth stays on the body. A group draws 16384 vertices at most. A level that gives more vertices is not used. |
| `simsubdivide` | 0 | 0 to 2. Adds particles to the simulation: each level cuts each triangle of the model mesh in 4 (linear split, the shape of the model does not change). Each level also halves the cell of `detail`. Up: smaller folds, slower. The drawn mesh is the refined mesh; `subdivide` adds its levels on top. A refined mesh has 8192 vertices at most. A level that gives more vertices is not used. Not used with `lag`. |
| `range` | 0 | Largest distance from the view, in units, that moves the cloth. 0: no limit. |
| `enabled` | 1 | `enabled 0` stops the group. |

## Old keywords

The renderer reads the old keywords and changes them to the new ones.
The editor writes only the new keywords.
One `air` value replaces `drag`, `aero` and `wind`. Thus an old file with one of them keeps its value, and the other two change.

| Old | New |
| --- | --- |
| `maxdist` | `reach` |
| `gravity` | `weight` |
| `bend` | `stiffness` |
| `follow`, `lagfreq` | `hold` = value / 20 |
| `damping` | `bounce` |
| `lagdamp` | `bounce` = 1 - value |
| `drag`, `aero`, `wind` | `air` |
| `proxy` | `detail` = 4 / value |
| `maxspeed`, `margin`, `iterations` | no change. The editor keeps these lines but has no slider for them. Do not use them in a new file. `maxspeed` applies only to `mode lag`. |
| `passback` | read and ignored. The editor does not write it. |

## Editor in game

The console command `ph_menu` opens the editor on the model of the player.

Lists:

- **Unused** shows the surfaces of the model that are in no group. It does not show the surfaces with less than 20 vertices (tags, caps, helper surfaces). Select a surface to see its outline. Push **Add** to make a group.
- **Used** shows the groups of the file. Select a group to edit its text and to see its outline. Push **Remove** to delete the group.
- **Outline** is a thin cyan line on the selected surfaces: the open edges and the silhouette.

Text:

- **Text** shows the text of the selected group.
- Shift + arrows, Home and End select text. A drag with the left button selects text. A double click selects a word. A triple click selects the line. Ctrl + A selects all.
- Ctrl + C copies, Ctrl + X cuts and Ctrl + V pastes, with the Windows clipboard.
- Ctrl + left and Ctrl + right move by one word. Ctrl + Home and Ctrl + End go to the start and to the end.
- **Refresh** applies the text to the model.

Settings:

- Each value of the group has a slider. Drag the bar or use the mouse wheel. The text and the model change at once.
- The mouse wheel changes a value by 10 % of the value. Thus small values get small steps.
- If a value of the file is more than the end of the bar, the bar gets longer.
- Put the cursor on a slider: the bar at the bottom of the menu tells what the value does.
- **Sag**, below the sliders, is the distance that the weight pulls the cloth down at rest. It is red when it is near `reach`.

Preview:

- **Show: Pins** shows the pin lines of the selected group. A white dot is the bone. For `gradient`: a red ring at `<fixed>`, a green ring at `<free>`, and the axis in white. For `radius`: a red sphere (fixed) and a green sphere (free). For `bones`: a magenta dot on each bone.
- **Show: Pins + particles** also shows one dot on each point: red is fixed, yellow is half free, green is free.
- **Show: All + reach** also shows the `reach` limit of some free points, in blue.
- The **Pins** list shows the pin lines. Select a line to show it bright and to change its fixed and free values.
- **Colliders: Show** shows the colliders of the group. Orange: `body` (`collide humanoid`). Yellow: `fit` (`collide model`). Green: `line` (a collide line). Cyan: the selected collider.
- **On / Off** adds or removes `collide remove <bone>` for a `body` or `fit` collider. For a `line` collider, it removes the line.
- **Radius** changes the selected collider. **Body scale** changes the scale of `collide humanoid`. For a box, the sliders are the half sizes, **Round** and the offset.
- **Anim** and **Turn** play a walk, a run or a jump, and turn the model. Use them to see the inertia of the cloth.
- The mouse wheel zooms to the cursor. The right button moves the model. The left button turns the model. **Reset view** puts back the zoom and the position.
- A part of a line behind the model is faint, so you can see it.

Save:

- **Apply** writes `models/players/<name>/model.phys` in the game folder of the player, and closes the editor.
- **Back** closes the editor and does not keep the changes.

## Console

| Name | Meaning |
| --- | --- |
| `ph_enable` | 0 stops all the physics. |
| `ph_range` | Multiplies the `range` of each group. |
| `ph_debug` | 1 writes the loaded files, the groups and the resets. 2 also shows the colliders as orange lines. 4 writes, two times per second, the mean and the largest distance of the free points from the animation. |
| `ph_reload` | Clears the loaded `.phys` data. The files load again when the renderer draws a model. |

## Limits

- The simulation and the draw use LOD 0 for the surfaces of a group.
- The cloth simulation uses fixed steps of 1/480 s, 32 steps per frame at most. Thus the result does not change with the frame rate. Below 15 frames per second, the cloth moves slowly.
- The cloth is drawn one step (8 ms) behind the simulation, so that it moves smoothly between two steps. The fixed part stays on the animation.
- A collision stops the speed into the collider. It does not throw the cloth out.
- Shadow volumes and gore use the animated surface, not the simulated surface.
- Hair that is part of the head surface moves as one soft piece. Hair cards or strips in their own surface give a better result.
- The first person model of the player has no simulation.
