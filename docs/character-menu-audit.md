# Audit du menu de personnage (ancien vs nouveau)

Date : 2026-10-10. Branche : `feature/vulkan-raster`.

Comparaison :

- **Ancien menu** : menu statique produit par le générateur Java `source/local/characterMenu` (copie du 2 sept., 2886 items). Aucun pk3 de `swgl-master` n'a été retrouvé ; cette copie sert de référence.
- **Nouveau menu** : `ui/IngameSWGLChars.menu` de `SWGL_Menu.pk3` (10 oct.), avec `code/cgame/cg_characters.cpp` et `code/game/g_charactersLoad.cpp`.
- **Code** : `code/ui/ui_main.cpp` de `swgl-master` comparé à HEAD.

Les évolutions (armes en icônes, onglets Stats/Powers/Weapons, configs nommées, couleurs du lightning) ne sont pas listées.

## Pertes

| Fait | # | Fonctionnalité | Avant | Maintenant | Cause |
|---|---|---|---|---|---|
| V | 1 | Config par défaut de 34 variantes (toutes corrigées le 2026-10-10) | Chaque personnage charge son `_def.cfg` | Aucun cfg chargé : stats, pouvoirs, armes, sabre, couleurs et `ui_npc_type` restent ceux du personnage précédent | Le code `.cha` ne correspond plus au nom du cfg (voir tableau suivant). Il n'existe pas de `ext_data/characters/default.cfg` de repli. |
| V | 2 | Spawners NPC aléatoires (corrigé le 2026-10-10, voir « Variantes aléatoires ») (Random KOTOR / TOR / Prequel / NR Jedi, Sith, Youngling) | Visibles en mode NPC seulement. Le cfg met `ui_npc_type KOTOR_Jedi`… : le NPC prend un modèle au hasard. Message « sabre aléatoire » dans l'onglet armes. | Modèle fixe (`kotorjedi_twilekf`…) avec des réglages périmés. Visibles aussi en mode joueur. `CantSaberEdit` jamais affiché. | `random true` est ignoré par `g_charactersLoad.cpp`. Le préfixe `Rand_` a disparu du code (`TOR_KOTOR_Jedi` au lieu de `tor_rand_kotor_jedi_default_def.cfg`). |
| V | 3 | Champ NPC Targetname (corrigé le 2026-10-11 : remis dans l'onglet Stats, mode NPC) | Edit field `ui_npc_targetname` dans l'onglet Stats | Absent | Item retiré du menu. Le code lit toujours la cvar (`UI_UpdateNPCCvars` → `g_NPCtargetname`). |
| V | 4 | Munitions à l'Apply (corrigé le 2026-10-11 : `applycharweapons` ajoute 150 munitions par type, plafonné au max) | `exec "give ammo; give force;"` | `exec "give force;"` seulement | `applycharweapons` donne les armes sans munitions : arme vide possible. |
| X | 5 | Comportement de spawn « Invincible » (non retenu) | Option `@NPCMENU_INVINCIBLE` → `spawnscripts/invincible` | Absente de la liste `npcspawnscript` | Option retirée du menu. Le script `scripts/spawnscripts/invincible.IBI` est absent de tous les pk3 de SWGL_DEV : à restaurer avec l'option. |
| V | 6 | Personnages absents des `.cha` (corrigé le 2026-10-11 : ajoutés avec un `_def.cfg` tiré de leur `.npc` ; leur biographie n'existe pas dans les `.str`) | Brutus (CRIM), Falleen (CV), Jaxxon (REB), Nizuc Bek (HC) | Absents | Les modèles existent toujours (`brutus`, `falleenfemale`, `jedi_jaxxon`, `jab_bg`). À confirmer si c'est voulu. |
| X | 7 | Réapplication automatique du dernier loadout (non retenu : la config par défaut reste le défaut) | Save écrit `<code>_<variante>.cfg`, rechargé automatiquement à la sélection | Save écrit une config nommée qu'il faut recharger avec Load | L'ancien fichier est encore lu, mais seul « default configuration » + Load l'écrit (avec les valeurs par défaut). Peut-être un choix de design. |

### Pertes mineures (ergonomie)

| Fait | # | Fonctionnalité | Avant | Maintenant |
|---|---|---|---|---|
| V | 8 | Échap (corrigé le 2026-10-11 : fait l'action du bouton Back de l'écran affiché, `uiScript characterEscape`) | Retour au menu profil (`GameSelectionMenu`) | Ferme toute l'UI (`close all` + `closeingame`), même depuis l'écran du personnage |
| V | 9 | Position dans la liste à la réouverture (corrigé le 2026-10-11 : une page par grille, page 1 seulement si le nombre de pages change ou sur Reset search ; le nom recherché reste) | Dernière page de faction (`savePage` / `goToSavedPage`) | `onOpen` remet la page à 0 et vide la recherche par nom. Les filtres faction/tag restent. |

## Détail du point 1 : variantes sans `_def.cfg`

Corrigé le 2026-10-10 : les 21 fichiers du groupe « tiret → underscore » de `SWGL_Characters.pk3` sont renommés avec `_` (par exemple `rep_qui-gon_default_def.cfg` → `REP_Qui_gon_default_def.cfg`). Les 3 fichiers du groupe « code renommé » sont aussi renommés (`DRSE_Officer_default_def.cfg`, `NR_X2_default_def.cfg`, `REP_Aqua_default_def.cfg`). Les 5 cfg `Rand_` sont renommés sur le code `.cha` (par exemple `TOR_KOTOR_Jedi_default_def.cfg`). `YNG_Rand_Youngling_default_def.cfg` est remplacé par 5 copies : `JO_jedi_youngling_{default,female,togruta,alien1,alien2}_def.cfg`. Toutes les variantes des `.cha` ont maintenant un `_def.cfg`. Le flag `random true` est remplacé par les variantes aléatoires (point 2).

| Fait | Cause | Code `.cha` (variante) | Ancien fichier |
|---|---|---|---|
| V | Tiret → underscore (corrigé) | `CRIM_Q9_0` | `crim_q9-0_default_def.cfg` |
| V | | `CRIM_0_0_0` | `crim_0-0-0_default_def.cfg` |
| V | | `CRTRS_RANCOR` (`F_BULL`) | `crtrs_rancor_f-bull_def.cfg` |
| V | | `GE_ID_9` | `ge_id-9_default_def.cfg` |
| V | | `GE_IMPOFFICER` (`B_PONCHO`, `I_PONCHO`, `B_COAT`, `I_COAT`) | `ge_impofficer_b-poncho_def.cfg`… |
| V | | `GE_TIE_PILOT` (`F_RESERVE`) | `ge_tie_pilot_f-reserve_def.cfg` |
| V | | `INF_R0_GR` | `inf_r0-gr_default_def.cfg` |
| V | | `MANDO_BO_KATAN` (`DEFAULT`, `CW`) | `mando_bo-katan_default_def.cfg`, `mando_bo-katan_cw_def.cfg` |
| V | | `MNC_NEO_CRUSADER` | `mnc_neo-crusader_default_def.cfg` |
| V | | `REB_2_1B` | `reb_2-1b_default_def.cfg` |
| V | | `REB_FX_7` | `reb_fx-7_default_def.cfg` |
| V | | `REP_QUI_GON` | `rep_qui-gon_default_def.cfg` |
| V | | `REP_SIFO_DYAS` | `rep_sifo-dyas_default_def.cfg` |
| V | | `REP_EL_LES` | `rep_el-les_default_def.cfg` |
| V | | `RSE_2V_R8` | `rse_2v-r8_default_def.cfg` |
| V | | `TOR_GNOST_DURAL` | `tor_gnost-dural_default_def.cfg` |
| V | | `TOR_ZEZ_KAI` | `tor_zez-kai_default_def.cfg` |
| V | Code renommé (corrigé) | `DRSE_OFFICER` | `drse_s_officer_default_def.cfg` |
| V | | `NR_X2` | `nr_x2r_default_def.cfg` |
| V | | `REP_AQUA` | `rep_clo_aqua_default_def.cfg` |
| V | Préfixe `Rand_` retiré (corrigé) | `TOR_KOTOR_Jedi` | `tor_rand_kotor_jedi_default_def.cfg` |
| V | | `JO_SWTOR_Jedi` | `jo_rand_tor_jedi_default_def.cfg` |
| V | | `REP_Prequel_Jedi` | `rep_rand_pre_jedi_default_def.cfg` |
| V | | `NR_jedi_random` | `nr_rand_nr_jedi_default_def.cfg` |
| V | | `SO_SWTOR_Sith` | `so_rand_sith_default_def.cfg` |
| V | | `JO_jedi_youngling` (5 variantes) | `yng_rand_youngling_default_def.cfg` |

Les loadouts sauvegardés par le joueur (`ext_data/characters/<code>_<variante>[_NPC].cfg`) de ces personnages ne sont plus retrouvés non plus.

## Variantes aléatoires (remplace `random true`)

Une variante du `.cha` peut avoir des parties aléatoires. Le jeu les tire à chaque spawn NPC et à chaque Apply joueur (`code/game/g_characterRandom.cpp`).

| Champ | Effet |
|---|---|
| `npcRandom { "npc1" "npc2" … }` | Un NPC est tiré. Le modèle, le skin et le sabre viennent de ce `.npc`. Pour un NPC spawné, le tirage évite le modèle du joueur. |
| `skinRandom true` | Head, torso et lower aléatoires parmi les `.skin` du modèle (seul : modèle de la variante). |
| `saberRandom { … }` | Lame simple. |
| `saberDualRandom { … }` | Lame de la main gauche d'une lame simple (1 chance sur 5). |
| `saberStaffRandom { … }` | Lame double (1 chance sur 6, toujours si `saberRandom` est vide). |
| `saberColorRandom { … }` | Couleur de chaque lame. Une couleur répétée a plus de chances. Le menu affiche « Random saber color ». |

Une lame choisie dans le menu (`ui_saber` non vide) a priorité sur les lames tirées. Les cfg `_def` de ces variantes ont `ui_saber ""` et pas de `ui_npc_type`.

- Spawn : l'UI met `g_NPCcharKey` et `g_NPCvariant` (`updatenpccvars`). `Svcmd_Spawn_f` appelle `CHA_RandomizeSpawner`.
- Apply : le menu exécute `characterRandom` après `playermodel player`. Le résultat va dans `g_char_model`, `g_char_skin_*`, `g_saber*` : il reste après un changement de map.
- Les 6 personnages aléatoires reprennent les listes qui étaient codées en dur dans `SP_NPC_SWGL_Jedi`, `SP_NPC_Jedi` et `GetRandomNPCSaber`. `SP_NPC_SWGL_Jedi` et `SP_NPC_Jedi` restent pour les maps.
- Youngling : variante `RANDOM` en plus. Les 5 variantes fixes ont leur NPC (`Jedi_Youngling1` à `5`) et leur sabre.

## Écarté (ce ne sont pas des pertes)

| Élément | Raison |
|---|---|
| Holocron NPC + boîte « CharMechDesc » | Jamais affichés dans l'ancien menu (seulement des `hide`), texte placeholder |
| `getNPCcode` | Écrit `npcCode`, qui n'est lu nulle part (déjà sur master) |
| `IconPrev` | Décoration à fond `null` |
