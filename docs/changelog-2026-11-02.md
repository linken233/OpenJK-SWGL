# SWGL changelog — Arbiter, 2026-10-21 to 2026-11-02

These are the changes on `develop` that were not in `linken233/swg-develop` (its last merge was PR #58, 2026-10-20).
Key: [+] added, [\*] modified, [-] removed

## Cloth and hair physics (.phys) — new

* [+] Position-based (XPBD) cloth and hair physics for Ghoul2 models in rd-vulkan and rd-vanilla. A `.phys` file next to the `.glm` drives it. Format: `docs/phys-format.md`.
* [+] Sample `.phys` files and the editor menu are in `docs/phys/pk3/` (Vader, Malgus, Jodo Kast, Lana Beniko, Zash, Obi-Wan exile, ...). Put them in a pk3 to use them.
* [+] In-game editor `ph_menu`: groups, surfaces, pins, colliders, material keywords, "Sim split" (`simsubdivide 0..2`). It has views for particles, pins, reach, colliders, bone dots and surface outlines.
* [+] Debug views `ph_debug 5` (model frame speed) and `ph_debug 6` (constraint corrections, pin pops, solver time).
* Solver:
  * The simulation is relative to the model frame, as in NvCloth: inertia, damping relative to the frame, and rigid moves on teleport.
  * The model frame is the transform of a reference bone, with a 100 ms low-pass to remove animation pops.
  * Long-range tethers to the nearest pinned particle use geodesic lengths along the mesh, so a cape cannot be pulled through the body.
  * A group with no pinned particle is simulated in world space.
  * A particle that goes through a collider leaves on the side where it was at the start of the step.
  * Sub-steps are 1/120 s, and a step makes no memory allocation.
* Known issue: long hanging hair on Aralani swings sideways. The cause is not known yet.

## Character menu

* [+] You can select a moveset in the character menu.
* [+] The character menu shows the weapon and the saber stance.
* [+] When you select a saber in the loadout, you can customise it from the character menu.
* [\*] The menu forces a saber stance when the character needs one.
* [\*] Character search also finds variant names.
* [\*] Tags: tags are grouped by type (era, fs, role, canon, sex, kind, then the other types) and the pages list them by type. Displayed names come from `TAG_<tag>` and `TAGTYPE_<type>` strings. The character tags were migrated (`docs/characters-migration.csv`), and `docs/SWGL_Character_Guide.md` is updated.

## Gameplay / rendering fixes

* [\*] A saber with a retracted blade (blade off) does not use the saber stance in the legs animation.
* [\*] rd-vulkan: menu and HUD model scenes do not start the bloom. Before, the blade of the saber menu was blurred over the full screen.

## Documentation

* [+] `docs/animoverride-audit.md`, `docs/charmigration-npc-model.md`, `docs/charmigration-skins.md`, `docs/charmigration-validation.md`.

## From Linken233 (merged into develop)

* [\*] SP: the levelshot path for Jedi Outcast maps is correct.
* [+] `Q_IsAprilFoolsDay()` replaces the duplicated date checks. On April Fools' Day, Ezra Bridger is renamed to Jabba or Lando.
