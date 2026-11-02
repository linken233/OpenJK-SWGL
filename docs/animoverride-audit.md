# Animation overrides: audit of the missing `animoverride.cfg`

Source: `models/players/<model>/` of every `GameData/SWGL/*.pk3` (skin packs included), 1429 model folders. Not read: the `_def.cfg` files (unchanged), the `.npc` files (none sets `animOverride`).

A model uses an override only if its folder has `animoverride.cfg` (`overridename _xxx`). Everything else uses `_humanoid.gla`, unless the player picks a moveset in the character menu.

## 1. State today

Models that have an `animoverride.cfg` (12 of 1429):

| Model | Override | Pk3 |
|---|---|---|
| `501st2` | `_clones` | SWGL_Skins_REPUBLIC.pk3 |
| `501stct` | `_clones` | SWGL_Skins_REPUBLIC.pk3 |
| `appo` | `_clones` | SWGL_Skins_REPUBLIC.pk3 |
| `calo` | `_kotor` | SWGL_Skins_BH.pk3 |
| `cloneii` | `_clones` | SWGL_Skins_REPUBLIC.pk3 |
| `czerka` | `_kotor` | SWGL_Skins_CIVILIANS.pk3 |
| `dopak` | `_kotor` | SWGL_Skins_CIVILIANS.pk3 |
| `elween` | `_elween` | Z_custom_chars.pk3 |
| `sbd` | `_sbd` | SWGL_Skins_CIS.pk3 |
| `sister` | `_clones` | SWGL_Skins_REPUBLIC.pk3 |
| `twinsuns_bh` | `_kotor` | SWGL_Skins_BH.pk3 |
| `wattambor` | `_vader` | SWGL_Skins_CIS.pk3 |

The other 34 overrides are used by **no model** (only reachable from the moveset list).

## 2. Broken reference

- `elween` (Z_custom_chars.pk3) points at `_elween`, and no `models/players/_elween/_elween.gla` exists in any pk3. The model falls back to the stock animations. *To do: create `_elween` or point it at an existing override.*

## 3. Models that probably miss their override

Match by folder name only. Confidence = how sure I am that the model is the character of the override (high: same character; medium: same character, other outfit/era; low: generic family, the animations may not fit). *Please strike what you do not want.*

### `_dooku`: Count Dooku (high), 3 models

`dooku` (Skins_CIS), `dooku_totj` (Skins_CIS), `dr_dooku` (Skins_CIS)

### `_vader`: Darth Vader (high), 5 models

`am_vader` (Skins_EMPIRE), `vader` (Skins_EMPIRE), `vader2` (Skins_EMPIRE), `vader_dk` (Skins_EMPIRE), `vader_infinities` (Skins_INFINITIES)

### `_lanakin`: Anakin Skywalker (medium), 8 models

`anakin` (Skins_REPUBLIC), `anakin_dl` (Skins_INFINITIES), `anakin_ep2` (Skins_REPUBLIC), `anakin_kid` (Skins_REPUBLIC), `anakin_rotj` (Skins_REPUBLIC), `anakin_suit` (Skins_INFINITIES), `anakin_totj` (Skins_REPUBLIC), `anakin_totj2` (Skins_REPUBLIC)

### `_obi3`: Obi-Wan Kenobi (medium), 9 models

`jedi_kenobi` (Skins_REPUBLIC), `kenobi_tcw` (Skins_REPUBLIC), `obi_dw` (Skins_REPUBLIC), `obiwan_ep1` (Skins_REPUBLIC), `obiwan_ep2` (Skins_REPUBLIC), `obiwan_ep3` (Skins_REPUBLIC), `obiwan_exile` (Skins_REPUBLIC), `obiwan_exileb` (Skins_REPUBLIC), `obiwan_exilec` (Skins_REPUBLIC)

### `_oldben`: Old Ben Kenobi (high), 1 models

`oldben` (Skins_REBELLION)

### `_luke`: Luke Skywalker (medium), 18 models

`luke_anh` (Skins_REBELLION), `luke_ceremonial` (Skins_REBELLION), `luke_crait` (Skins_RESISTANCE), `luke_cs` (Skins_REBELLION), `luke_dago` (Skins_REBELLION), `luke_esb` (Skins_REBELLION), `luke_eu` (Skins_REBELLION), `luke_fallen` (Skins_INFINITIES), `luke_heir` (Skins_REBELLION), `luke_hoth` (Skins_REBELLION), `luke_mim` (Skins_REBELLION), `luke_pilot` (Skins_REBELLION), `luke_rebellion` (Skins_REBELLION), `luke_rotj` (Skins_REBELLION), `luke_sith` (Skins_REBELLION), `luke_sotme` (Skins_REBELLION), `luke_tfa` (Skins_RESISTANCE), `luke_trainer` (Skins_REBELLION)

### `_maul`: Darth Maul (high), 9 models

`cybernetic_maul` (Skins_CRIMINALS), `darthmaul` (Skins_CRIMINALS), `maul_infinities` (Skins_INFINITIES), `maul_rebels` (Skins_CRIMINALS), `maul_solo` (Skins_CRIMINALS), `maul_tcw` (Skins_CRIMINALS), `maul_wots` (Skins_CRIMINALS), `maulkiller` (Skins_OTHER), `savage_opress` (Skins_CRIMINALS)

### `_yoda`: Yoda (high), 2 models

`gunshipyoda` (Map_BattleOfGeonosis), `yoda` (Skins_REPUBLIC)

### `_mace`: Mace Windu (high), 2 models

`jedi_windu` (Skins_REPUBLIC), `mace_windu` (Skins_REPUBLIC)

### `_palpatine / _sidious`: Palpatine / Sidious (high), 5 models

`darthplagueis` (Skins_TSE), `lord_sidious` (Skins_EMPIRE), `palpatine` (Skins_REPUBLIC), `palpatine_app` (Skins_EMPIRE), `palpatine_crazed` (Skins_INFINITIES)

### `_grievous`: General Grievous (high), 1 models

`grievous` (Vehicles)

### `_jango`: Jango Fett (medium), 5 models

`cassusfett` (Skins_MANDALORIAN), `hsfett` (Skins_BH), `jangofett` (Skins_BH), `khomofett` (Skins_MANDALORIAN), `slave1_jango` (Vehicles)

### `_malak / _kotor2`: Darth Malak / Revan (medium), 12 models

`bastila` (Skins_TOR), `canderous` (Skins_MANDALORIAN), `carth` (Skins_TOR), `darthmalak` (Skins_TSE), `darthrevan` (Skins_TSE), `darthrevan_tor` (Skins_TOR), `hk47` (Skins_TOR), `jolee_bindo` (Skins_TOR), `malakili` (Skins_CRIMINALS), `revan` (Skins_TOR), `revan_c` (Skins_TOR), `revanreborn` (Skins_OTHER)

### `_shaak`: Shaak Ti (high), 2 models

`shaakti` (Skins_REPUBLIC), `shaakti_tfu` (Skins_REPUBLIC)

### `_serra`: Serra Keto (high), 1 models

`serraketo` (Skins_REPUBLIC)

### `_shakkra`: Shak Kra (high), 1 models

`shakkra` (Skins_INFINITIES)

### `_drallig`: Cin Drallig (high), 1 models

`cin_drallig` (Skins_REPUBLIC)

### `_gon`: Qui-Gon Jinn (high), 3 models

`quigon` (Skins_REPUBLIC), `quigon_ghost` (Skins_INFINITIES), `quigon_old` (Skins_INFINITIES)

### `_cal`: Cal Kestis (high), 3 models

`cal_inquisitor` (Skins_JEDI_SITH), `cal_survivor` (Skins_JEDI_SITH), `kestis` (Skins_JEDI_SITH)

### `_rey`: Rey (high), 3 models

`rey` (Skins_RESISTANCE), `rey_skywalker` (Skins_RESISTANCE), `rey_solana` (Skins_INFINITIES)

### `_ren`: Kylo Ren (high), 3 models

`kylo_mask` (Skins_FIRST_ORDER), `kylo_ren` (Skins_FIRST_ORDER), `ren` (Skins_CRIMINALS)

### `_df2`: Kyle Katarn (Dark Forces II) (medium), 11 models

`boc_aseca` (Skins_EMPIRE), `dfii_kyle` (Skins_REBELLION), `dfii_kyle2` (Skins_REBELLION), `gorc` (Skins_EMPIRE), `jerec` (Skins_EMPIRE), `kyle_sjg` (Skins_REBELLION), `merc_kyle` (Skins_REBELLION), `mots_kyle` (Skins_REBELLION), `pic` (Skins_EMPIRE), `sariss` (Skins_EMPIRE), `yun` (Skins_EMPIRE)

### `_dbgalen`: Galen Marek (medium), 8 models

`galen_jedi_hunter` (Skins_INFINITIES), `kentomarek` (Skins_JEDI_SITH), `starkiller_arena` (Skins_REBELLION), `starkiller_armor` (Skins_REBELLION), `starkiller_clone` (Skins_REBELLION), `starkiller_dago` (Skins_REBELLION), `starkiller_jedi` (Skins_REBELLION), `starkiller_tie` (Skins_REBELLION)

### `_tdroid`: Tactical Droid (low), 1 models

`tactical_droid` (Skins_CIS)

### `_clones / _clones_2`: Clone Troopers (low), 23 models

`501starc` (Skins_REPUBLIC), `501starcmk2` (Skins_REPUBLIC), `aquatrooper` (Skins_REPUBLIC), `arc` (Skins_REPUBLIC), `beachtrooper` (Skins_INFINITIES), `clone_gunner` (Skins_REPUBLIC), `clone_p1` (Skins_REPUBLIC), `clone_rgb` (Skins_REPUBLIC), `clone_scout` (Skins_REPUBLIC), `clone_sniper` (Skins_REPUBLIC), `clonebacara` (Skins_REPUBLIC), `clonecommander` (Skins_REPUBLIC), `clonenaval` (Skins_REPUBLIC), `clonepilot` (Skins_REPUBLIC), `cloneponcho` (Skins_REPUBLIC), `clonerc2` (Skins_REPUBLIC), `clonetrainee` (Skins_REPUBLIC), `clonetrooper_ab` (Skins_REPUBLIC), `cloneultarck` (Skins_REPUBLIC), `cloneultima` (Skins_REPUBLIC), `commander_ponds` (Skins_REPUBLIC), `republictrooper` (Skins_TOR), `tcwarcs` (Skins_REPUBLIC)

Total: 140 models.

## 4. Questions

1. Should the cfg go into the skin pk3 of each model (`SWGL_Skins_*.pk3`) or in one `Z_` pk3 that overrides them all? A single `Z_` pk3 is easier to maintain and to remove.
2. `_clones_2`, `_jedi`, `_Jedi2`, `_jbrute`, `_tguard`, `_zombie`, `_reb`, `_kotor2`: I found no obvious model by name. Which models should use them?
3. Medium and low groups: apply all, or only the high ones first?
