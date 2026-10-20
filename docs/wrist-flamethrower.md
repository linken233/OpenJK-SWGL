# Wrist flamethrower item

A bracer worn on the left forearm (`item_wrist_flamethrower`, tag `INV_WRIST_FLAMER`). It uses the same
inventory mechanism as the jetpack (the "fuel items" block in `code/game/g_items.cpp`).

Test pack: `docs/Z_SWGL_Wrist_Flamer.pk3`. It needs `weapon_flame_thrower` (SWGL_Weapons.pk3), the texture of
`models/players/hvy_mando` (SWGL_Skins_MANDALORIAN.pk3) and the base game `models/weapons2/jetpack/jetpack.gla`.
The model is the turret (`l_hand_wrist_turret`, the nozzle box) of `hvy_mando`, plus a mirrored copy of the box without the nozzle (`wrist_flamer_inner`) that shares its inner face and sits inside the arm.

## Rules

- Use item (select it in the inventory, `invuse`, or `use_wristflamer`) switches the flame on and off.
- While on: the torso takes the pose of the flamethrower of Boba Fett (`BOTH_FORCELIGHTNING_HOLD`), the flame
  effect is `boba/fthrw` bolted to the `*flash` tag of the model, and the burn is the main attack of the
  `flameweapon` weapon (`WP_FlameThrowerBurn`, the same code as the weapon): damage, range, burn time, sounds. The damage is twice that of the weapon (`WRIST_FLAMER_DAMAGE_SCALE`).
  The flame burns from the bolt along the aim of the player. It stops in water.
- A full tank is 100 units. The fuel is used twice as fast as `fueldrain` says (`WRIST_FLAMER_DRAIN_SCALE`): with the default of 500 msec per unit that is 25 seconds of fire.
- Touching a wrist flamethrower of the same kind as the one worn refills the tank (as for the jetpack). Otherwise the fuel never refills. An empty wrist flamethrower leaves the inventory.
- Touching another wrist flamethrower drops the one worn on the ground and takes the new one. A dropped one keeps its
  fuel (`count` = fuel + 1); one that was never used has a full tank.
- The HUD shows a fuel gauge left of the jetpack one, and the fuel as the number of the inventory icon.

## Making another one

Add an `.itm` file in `ext_data/` (see `itm_wrist_flamer.itm` in the pack):

| Key | Meaning |
|---|---|
| `tag INV_WRIST_FLAMER` | makes it a wrist flamethrower |
| `wornmodel` | ghoul2 model attached to the `lradius` bone (left forearm). It needs a `*flash` tag surface: the origin of the flame is the third vertex, the flame goes out opposite to the first-to-second side |
| `flameweapon` | weapon whose main attack gives the flame, default `weapon_flame_thrower` |
| `fueldrain` | msec per unit of fuel, default 500 |

The vertices of the model are in the space of the `lradius` bone (inverse of its `BasePoseMat` rotation, without the bone scale of 0.64 in `_humanoid.gla`).

`wornmodel` and `fueldrain` also work for jetpacks (`jetmodel` and `jetdrain` are still read).

## Not done

- Not run in game: the build compiles and the model was checked by parsing and software render only.
- Savegame version is 4 (`wristFlameModel`), and the inventory slots of the jetpack state moved by one.
