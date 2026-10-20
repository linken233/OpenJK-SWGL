# Audit du système d'armes dynamique (SP)

Périmètre : chargeur (`g_weaponLoad.*`, `weapons.h`), logique de tir (`g_weapon.cpp`, `wp_fire_logic.cpp`, `wp_*.cpp`),
pmove (`bg_pmove.cpp`), missiles (`g_missile.cpp`), cgame (`cg_weapons.cpp`, `FX_GenericWeaponEffects.cpp`, `cg_players.cpp`),
et les données réelles (`ext_data/weapons.dat` + 28 `.wpn` extraits de `SWGL_Weapons.pk3` et `Z_SWGL_dw_*.pk3`).
Référence « avant migration » : `git show 465bf7848^` (une classe `wp_*.cpp` par arme).

Méthode : lecture du code, build Debug vérifié après chaque correction, rien n'a été lancé en jeu. Niveaux de confiance :
**[C]** confirmé par lecture, **[R]** régression confirmée contre le code d'avant migration, **[P]** probable, à vérifier en jeu.
`weaponStat <weapon_class> [attackIndex]` (cheats) affiche les valeurs réellement chargées, utile pour vérifier l'héritage.
Les numéros de ligne sont ceux de l'audit initial et ont bougé depuis.

Statuts : **Corrigé**, **Voulu** (décision assumée, rien à faire), **Ouvert**.

---

## 1. Plantages, hangs, comportements indéfinis

| # | Statut | Où | Problème et traitement |
|---|--------|----|------------------------|
| 1.1 [C] | Corrigé (résidu) | `WPN_WeaponClass` | Au-delà de `MAX_WEAPONS`, `weaponNum` était écrasé puis utilisé : écriture hors de `weaponData`. Un `return` a été ajouté. **Résidu :** `wpnParms.weaponNum` garde l'arme précédente, donc les clés du bloc refusé s'écrivent dans elle (voir 4.6). Piste : un slot poubelle `weaponData[MAX_WEAPONS]`. |
| 1.2 [C] | Corrigé (chargeur d'armes) | `WP_ParseAtkParms`, `WP_ParseWeaponParms` | `while (holdBuf)` testait un `const char**` (toujours vrai) : un `.wpn` sans `}` bouclait à l'infini. Un jeton vide (EOF) fait maintenant un `Com_Error(ERR_DROP)` qui nomme l'arme. La récupération sur un `weaponclass` inattendu a été écartée. **Reste :** `g_itemLoad.cpp` (1 boucle) et `g_charactersLoad.cpp` (7 boucles) ont le même `while (holdBuf)` sans sortie sur EOF ; `g_vehicleLoad.cpp` et les loaders `while (1)` testent déjà `!token[0]`. |
| 1.3 [C] | Corrigé | `ATK_MissileDFlags` | Boucle jusqu'à lire `;`. Un jeton invalide affiche un warning, est rendu à l'appelant (`tokenStart`) et sort de la boucle. |
| 1.4 [C] | Corrigé | tri des buckets | `weaponBucket = WB_UNSET (0)` donnait `buckets[-1]`. La boucle démarre à l'index 1 (`weapon_none`) et refuse tout index hors `[0, -WB_OTHERS[` avec un warning nommant l'arme. |
| 1.5 [C] | Corrigé | `CG_RegisterWeapon` | Le garde-fou de `bg_itemlist` ne se déclenchait pas pour le second item (ammo, après `bg_numItems++`). Corrigé dans `cg_weapons.cpp` (item d'ammo créé d'abord, tests `MAX_ITEMS` / `MAX_ITEMS - 1`). Effet de bord : l'item enregistré en dernier est l'arme, et l'ammo a un index plus bas. |
| 1.6 [C] | Voulu | `cg_weapons.cpp` (~385, 404) | `baseHitWallEffects[baseWeaponNum]` / `baseHitFleshEffects[...]` : tables de 43 entrées. Une arme dynamique **sans** `baseweapon` a `baseWeaponNum >= 43` : lecture hors tableau. Aucune arme actuelle n'est dans ce cas, mais « créer une arme de zéro » est le but du système. |
| 1.7 [C] | Corrigé | `WP_GetSpread` | Pour `g_spskill < 0`, un `return` évite l'écrasement par `npcSpread[diff]` (index négatif). |
| 1.8 [C] | Corrigé au chargement | `wp_fire_logic.cpp`, `bg_pmove.cpp` | Division par `chargeUnitTime`. Défaut de 0.25 appliqué après héritage quand la valeur est 0. `chargeUnitTime` garde 0 = « non défini » (0 est invalide : division). |
| 1.9 [C] | Corrigé | `ATK_MissileLight` | Après un échec de lecture, `tokenFlt` n'était pas initialisé : warning + `return`. Valeurs hors `[0, 500]` bornées. |
| 1.10 [C] | Atténué | `WP_LoadWeaponParms` | `weaponFileList` passe de 2048 à 5096 octets. Le dépassement reste silencieux (`5096` est sans doute une faute pour `5120`). |

## 2. Régressions par rapport à l'avant-migration

| # | Statut | Où | Avant → maintenant |
|---|--------|----|--------------------|
| 2.1 [R] | Corrigé | bowcaster (`wp_fire_logic.cpp` ~603) | Vitesse `BOWCASTER_VELOCITY (1300) * (0.7..1.3)` (ancien `weapons.h`, `BOWCASTER_VEL_RANGE 0.3`) → `velocity * (Q_flrand(0.8,1.2) + 1.0)`, soit 1.8 à 2.2 × la vitesse si `velocity` vaut 1300. Vérifier la valeur dans `weapons.dat` (`weaponStat weapon_bowcaster 0`). Correction possible : `velocity * (Q_flrand(-1,1) * 0.3 + 1)`. Touche aussi `weapon_plasma_cutter`. |
| 2.2 [R] | Corrigé | bowcaster (~636) | Dégâts PNJ par difficulté (12/24/36) → `attackData->damage` fixe (45). `npcDamage` du `.dat` est ignoré. |
| 2.3 [R] | Corrigé | `WP_FireGenericBeam` | Le tir principal traversait jusqu'à 10 cibles. `maxHits` (1 pour `FL_BEAM`, 1/2/3 pour le chargé, 3 pour `FL_FULL_BEAM`) limite les cibles touchées ; une esquive de Jedi ne la consomme pas. |
| 2.4 [R] | Corrigé | `WP_FireGenericBeam` | `DAMAGE_DEATH_KNOCKBACK` rétabli pour `FL_BEAM` ; `NO_KNOCKBACK \| NO_HIT_LOC` pour le chargé et `FL_FULL_BEAM`. |
| 2.5 [R] | Corrigé | `WP_FireGenericBlaster` | La condition `!(class == VEHICLE)` était toujours vraie : la dispersion s'appliquait toujours. Rétabli : pas de dispersion pour un véhicule ni avec Sense actif niveau 2+ ; sans client, dispersion conservée. |
| 2.6 [R] | Corrigé (données à ajouter) | concussion alt | Poussée, recul, chute, boîte de trace, `MOD_CONC_ALT` et anneau d'effet sont pilotés par de nouvelles clés d'attaque (voir « Ajouts »). **À faire dans les données :** ajouter les clés au bloc de la concussion alt (section 7). |

## 3. Combos de propriétés

### Décidés voulus
- **3.1 `attackType` absent sur un bloc `scoped_*`.** Une alt scoped absente retombe sur `scoped_main`. Un bloc `scoped_*` sans `attackType` prend maintenant celui de son équivalent (`scoped_main` ← `main`, `scoped_alt` ← `alt`) quand `scopeType != ST_NONE`. Sinon, ou si l'attaque de référence n'en a pas, un warning au chargement le signale (`parsed` marque les blocs présents). Les armes concernées (`weapon_thefirstorder`, `weapon_rebelrifle`, `weapon_CR-2`, `weapon_S5_Heavy_Pistol`, `weapon_e5c`) voient donc leurs `spread` scoped appliqués pour la première fois : à tester.
- **3.6 `weaponCategory` pilote animation, dual-wield, tir double et cadence.** Voulu, données comprises (`weapon_plasma_rifle`, `weapon_plasma_cutter`, `weapon_DT-57`, `weapon_S5_Heavy_Pistol` en `WC_PISTOL`). Ajout de `WC_UNSET = -1` : l'héritage se fait sur `WC_UNSET`, et une dérivée peut choisir `WC_NONE` explicitement ; les `WC_UNSET` restants deviennent `WC_NONE` après héritage.
- **3.9 sons conditionnés par `muzzleEffect`, attaques 0 et 1 seulement.** Voulu. **Question ouverte :** `startSnd` et `readySnd` ne sont pas hérités alors que `firingSnd` l'est (une dérivée reçoit la boucle sans le son d'amorçage ni d'attente). Deux tests `[0] == 0` à ajouter si ce n'est pas voulu.

### Corrigés
- **3.8 et 3.10 index d'attaque du missile.** `G_MissileImpacted`, `G_MissileImpact`, la lumière (`cg_ents.cpp`) et tous les `FX_*Think` lisent `ent->attack_index`, que le missile porte depuis `CreateMissile`. Plus aucun recalcul avec `cg.zoomMode`.
- **3.3 `npcSpread` lu comme entier.** Lu en float, initialisé à `-1` (l'héritage `== -1` fonctionne enfin), repli sur le `spread` du joueur seulement pour `< 0` (un `npcSpread 0` explicite donne une visée parfaite). Les PNJ visent plus précisément qu'avant sur les armes à `npcSpread` fractionnaire (`thefirstorder` 1.2 → 0.4, `repeater` 1.4 → 0.7...).
- **3.7 zoom et visée sniper câblés.** `IsScopedZoom()` (zoom = sniper ou lunette) remplace `cg.zoomMode == 2` pour l'animation sniper (`weapon_cis_sniper` en profite). La visée PNJ de `FireWeapon` redevient `weaponCategory == WC_SNIPER && alt_fire` (forme d'avant migration), sans dépendre du zoom du joueur ni d'IDs en dur. Le gain de l'animation est surtout invisible en première personne ; celui de la visée PNJ est réel.

### Ouverts
- **3.2 `spread` des attaques scoped : donnée morte pour le joueur [C].** `WP_FireGenericBlaster` : si `is_player_scoped`, le tir part droit de l'œil, sans dispersion, même quand l'attaque scoped est bien lue.
- **3.5 Héritage champ par champ sur un `attackType` différent [C].** Une arme qui change `attackType` garde le reste de la base : `weapon_geo_sonic_blaster` (base DEMP2, passé en `FL_Blaster`) garde `missileFuncName demp2_func`, `maxChargeUnits 3`, `chargeUnitTime`, `chargeMuzzleShader`, `chargeSound` ; `weapon_flame_thrower` / `weapon_h3_flamer` héritent `blaster_func`, `velocity 2300`, `bounceCount 8`, `hitDroidEffect`, `npc*` ; `weapon_DC_15X` alt (`FL_BEAM`) garde `blaster_alt_func` et `bounceCount 8` ; `weapon_clonepistol` alt définit `chargesound` sur une logique non chargée (`FL_BLASTER`), le son ne joue jamais. Données fantômes, sans effet grave aujourd'hui.

## 4. Héritage et chargeur

### Fait
- **Sentinelles `-1` pour les valeurs pouvant valoir 0 (ex-3.4).** `damage`, `defaultDamage`, `splashDamage`, `splashRadius`, `energyPerShot`, `spread` (par attaque) ; `ammoIndex`, `ammoLow`, `numBarrels`, `scopeType` (par arme) ; plus `weaponCategory` (`WC_UNSET`) et les nouveaux champs de rayon. L'héritage se fait sur `-1`, `WPN_ClearUnset` ramène les `-1` restants à 0 après héritage (aussi pour les slots inutilisés). `fireTime`, `velocity`, `range` et `chargeUnitTime` gardent `0 = non défini` (0 y est invalide). Une dérivée peut donc écrire `spread 0` : cas `weapon_t_21` (hérite à tort `0.5` du blaster avant). Deux gardes ajoutés : `scopeType > ST_NONE` pour l'héritage de l'`attackType` scoped, et accès borné à `ammoData[ammoIndex]` pour les grenades.
- **Messages d'erreur ([4.7]).** Les warnings du chargeur portent le nom de l'arme (`WPN_Warn`) ; les messages `%f` sur des `int` de `npcDamage`/`npcVelocity` sont corrigés.

### Ouverts
- **4.1 [C]** `baseWeaponNum` ne garde qu'**un seul niveau**. `weapon_clonerandom` → `weapon_clonecarbine` → `weapon_blaster` : `baseWeaponNum` = 31, pas 3. Tous les `switch (baseWeapon)` voient `WP_CLONECARBINE`.
- **4.2 [C]** L'ordre de traitement compte : l'héritage lit la base telle qu'elle est. Une base **déjà finalisée** (`npcDamage` calculé à 0.3/0.6/0.9 × dégâts, `maxChargeUnits 1`, `blockability` rempli) transmet ces valeurs dérivées ; une base **pas encore finalisée** (index plus grand) transmet des sentinelles. Aujourd'hui toutes les bases ont un index plus petit.
- **4.3 [C]** `ammoData` n'est pas réinitialisé par `WP_LoadWeaponParms` et `ammoCount` démarre à `AMMO_HC_MAX` : un second appel ajouterait des types de munitions sans fin (erreur après 21 appels).
- **4.4 [C]** Les nouveaux types de munitions ne viennent que des bases `WP_THERMAL`/`WP_DET_PACK`/`WP_TRIP_MINE`. Une dérivée de ces bases reçoit **toujours** une munition générée, même si le `.wpn` écrit `ammotype 7`. `WPN_Ammo` ne connaît que les onze noms en dur.
- **4.5 [C]** `ammotype N` s'écrit en nombre : un changement d'ordre de l'enum `ammo_t` remappe silencieusement les armes. `ammotype 20` est accepté (borne `MAX_AMMO-1`), avec `max 0`.
- **4.6 [C]** `wpnParms.weaponNum`/`ammoNum` persistent entre blocs et fichiers : un bloc sans `weaponclass` écrit dans l'arme précédente (et le résidu de 1.1). Un classname déjà défini dans un autre fichier crée un doublon (premier gagnant dans `WP_GetWeaponID`) au lieu d'écraser.
- **4.7 [C]** Un `npcDamage` à une ou deux valeurs laisse `-1` dans le reste ; le repli `damage * 0.3 * diff` de `WP_GetWeaponDamage` donne alors 0 dégât en facile.
- **4.8 [C]** Un nom d'attaque inconnu fait un `ERR_DROP`, alors que les autres erreurs ne font qu'un avertissement. S'y ajoute maintenant l'erreur d'EOF (1.2).
- **4.9 [C]** `weaponBuckets` : la queue du tableau (taille 135, ~70 remplis) vaut `-1`, la valeur du marqueur `WB_MELEE`. Les boucles de `cg_weapons.cpp` parcourent ~60 faux marqueurs, ignorés.
- **4.10 [C]** `getAttackBlockabilityByIndexes` (`g_missile.cpp` ~62) : borne `> weaponCount` au lieu de `>=`.
- **4.11 [C]** `weaponIndexes[]` n'est jamais lu : mort.
- **4.12 [C] Champs jamais hérités** (ex-3.13) : `missileMass`, `effectDuration`, `startSnd`, `readySnd`, `scopeMask`, `scopeInsert`, `scopeFov`, `explosionEffect`, `shockwaveEffect`. `weapon_plasma_grenade` doit réécrire `explosionEffect`/`shockwaveEffect` en `main` et `alt` ; une dérivée d'un fusil à lunette perd `scopeFov`. Les nouveaux champs de faisceau (voir « Ajouts ») sont hérités.

## 5. Propriétés déclarées mais sans effet, ou données lues au mauvais endroit

Regroupe les anciens 3.10 à 3.12 et le tableau des propriétés mortes.

| Propriété / code | Réalité |
|------------------|---------|
| `range` | Lu seulement par le lance-flammes. Beam = `shotRange 8192` en dur, matraque = `STUN_BATON_RANGE 25` (le `.dat` dit 8192), mêlée = constante, durée de vie des missiles = 10000 ms en dur. |
| `npcDamage` | Ignoré par bowcaster (2.2), grenades (`TD_NPC_DAMAGE_CUT`), flechette, emplacé (×0.1 en dur), lance-flammes. |
| `missileSize` | Ignoré par flechette (`FLECHETTE_SIZE`), emplacé (`EMPLACED_SIZE`). |
| `velocity` | Emplacé = `EMPLACED_VEL` ; piège de mine et alt flechette = constantes (l'alt flechette ajoute `rand * FLECHETTE_ALT_MAX_VEL` au lieu de `randVelocity`, bug d'origine). Pour les grenades, `chargeAmount = temps_de_charge / velocity` : monter la vitesse raccourcit la charge, `velocity 0` divise par zéro. |
| Grenades | `WP_FireGrenade` et `WP_GrenadeExplode` lisent `damage`/`splashDamage`/`splashRadius`/effets de l'attaque **0**, même pour l'alt. `MOD_THERMAL`/`MOD_THERMAL_ALT`/`MOD_EXPLOSIVE_SPLASH` sont en dur : `methodOfDeath`/`splashMethodOfDeath` du `.wpn` sont ignorés. |
| DEMP2 alt | `DEMP2_AltRadiusDamage` lit `attackData[1].damage`, rayon 200 et durée 1300 ms en dur : `splashRadius 256` est ignoré, y compris pour `weapon_ion_blaster`. |
| Droïdes (twin blasters) | `CreateMissile(..., 0)`, `MOD_SBD`, `bounceCount 8`, `dflags` en dur ; `methodOfDeath` du `.dat` ignoré. |
| Bowcaster | `CreateMissile` sans `attackIndex`, `dflags` en dur, `missileDFlags` ignoré. |
| Faisceau (`WP_FireGenericBeam`) | `shotRange 8192` en dur, `range` ignoré. `methodOfDeath` est maintenant lu (repli sur `MOD_SNIPER`/`MOD_DISRUPTOR`). |
| `FT_HIGH_POWERED` | `bg_pmove.cpp` ~14298 écrase `attackData[...].damage` par `HIGH_POWERED_DAMAGE` (200, en dur) avant le tir et restaure au tir suivant de la même attaque : le `damage 25` de l'alt du `weapon_boba` est mort, et toute lecture de `.damage` entre deux tirs (PNJ Boba/Mandalorien/Jango via `WP_GetWeaponDamage`, `weaponStat`) voit 200. |
| `blockability` | Coexiste avec une liste d'IDs en dur dans `g_missile.cpp` ~803 (flechette, DEMP2, CIS sniper, bowcaster, repeater) pour décider si un tir est réfléchi. |
| `beamColor` / `fullBeamColor` (défauts) | `if (attackData->beamColor)` (`FX_GenericWeaponEffects.cpp` ~157, ~180) teste l'adresse d'un tableau : toujours vrai. Les défauts blanc/jaune ne s'appliquent jamais ; un rayon sans `beamColor` est noir. |
| `fullBeamShader` | N'apparaît que si `count != 0` ou `FL_FULL_BEAM` ; `WP_FireGenericBeam` ne met `fullCharge` que pour `FL_BEAM_CHARGED`. |
| Effets de charge | `FX_GenericChargedBlasterHitWall` (~58) : `gent->count / maxChargeUnits` est une division entière ; le palier moyen (`power <= 0.66`) n'est jamais atteint. |
| `Com_Printf(S_COLOR_CYAN, "Found item ...")` | `cg_weapons.cpp` ~236 : le premier argument est le format, rien ne s'affiche. |

## 6. IDs d'armes en dur (256 comparaisons `weapon ==/!= WP_*`, ~50 usages de `baseWeaponNum`)

Les armes dérivées (index >= 43, ou variantes `WP_REBELBLASTER`...) sont invisibles pour ces listes. Tous ces points sont ouverts :
- `NPC_utils.cpp` ~244-263 (×10 de vitesse de rotation) : liste de 20 armes, ni `CIS_SNIPER`, ni disruptor, ni aucune arme dynamique.
- `NPC_combat.cpp` ~576-578 (mauvaise visée à la première alerte) : seulement `BLASTER/REPEATER/THERMAL/BLASTER_PISTOL/BOWCASTER/SBD/DROIDEKA`. Les PNJ avec une carabine clone, un DH-17, un A280... visent parfaitement dès le début.
- `NPC_combat.cpp` ~2288, ~2348 : traces avec boîte seulement pour `BLASTER`/`BLASTER_PISTOL`.
- `g_missile.cpp` ~430, 442, 471, 659, 820, 1545 : roulade, alertes de danger et explosion des grenades pour `WP_THERMAL` seulement. `weapon_plasma_grenade` n'alerte pas les PNJ.
- `g_weapon.cpp` ~1548, 1563 : `shotsFired` incrémenté deux fois pour un bowcaster dérivé (dans le tir et dans `FireWeapon`) ; alerte sonore au tir (les grenades dérivées alertent à 256 comme un blaster).
- `g_combat.cpp` ~312-314 : le drop à la mort exclut `THERMAL/TRIP_MINE/DET_PACK` seulement.
- `g_cmds.cpp` ~248-257 (`give weapons`) : exclut `WP_SBD`/`WP_DROIDEKA` par ID au lieu de `playerUsable` ; `weapon_clonerandom` (`playerusable 0`) est donné quand même.
- `AI_Sniper.cpp` ~723-747 : `DISRUPTOR/CIS_SNIPER/TUSKEN_RIFLE` en dur.
- `wp_trip_mine.cpp`, `wp_stun_baton.cpp`, `wp_emplaced_gun.cpp` lisent `weaponData[WP_...]` (arme de base) au lieu de `ent->s.weapon`.

## 7. Données (`weapons.dat`, `.wpn`)

- **À ajouter au bloc de la concussion alt** (2.6) : `selfKnockback 200`, `pushForce 200`, `knockdownForce 400`, `beamRadius 1`, `beamTrailEffect concussion/alt_ring`, `methodOfDeath MOD_CONC_ALT`, `hitFleshEffect concussion/alt_hit`, `muzzleEffect concussion/altmuzzle_flash`.
- `weapons.dat` ~1716 : `selectSound sound/weapons/blaster/select.wav$` (un `$` littéral) : `weapon_cis_sniper` n'a pas de son de sélection.
- Clés en double (le dernier gagne) : `weapon_det_pack` alt (`damage` ×2), `weapon_DLT20A` scoped_alt (`spread` 1.1 puis 1), `weapon_DLT_19x` scoped_alt (`spread` 0.5 puis 1), `weapon_t_21` main (`spread 0` ×2).
- `weapon_plasma_grenade.wpn` existe dans `SWGL_Weapons.pk3` **et** `Z_SWGL_dw_plasma_grenade.pk3` (contenu identique).
- `missileFuncName` : `weapon_DLT_19x` main utilise `bryar_func`, alt `Blaster_func` (casse différente, la comparaison l'ignore).
- `weapon_stun_baton` : `range 8192` (le code utilise 25), `firingsound` identique à `readySound`.
- `weapon_emplaced_gun`, `weapon_tusken_staff`, `weapon_scepter`, `weapon_turret` : `FL_OTHER`/`FL_MELEE` sont routés par `baseWeaponNum` dans `FireWeapon`. Pour `WP_TURRET` il n'y a pas de cas : `FireWeapon` sort sans rien tirer (non vérifié : les tourelles passent peut-être par un autre chemin).
- `FT_*` : `FT_AUTOMATIC = 1`, la valeur par défaut est 0 ; le code traite 0 comme automatique (pas de `case`), sans incident, mais `firingType > FT_AUTOMATIC` est le seul test fiable.

## 8. Risques d'architecture (à confirmer)

- **[P] Index d'armes instables.** Index = ordre de chargement (43 en dur, puis `weapons.dat`, puis `.wpn` par ordre alphabétique de fichier). Ajouter, retirer ou renommer un `.wpn` décale tous les index suivants. Les `ps.weapons[]`/`ps.ammo[]` (et les index de munitions générées) sont stockés tels quels dans les sauvegardes : d'anciennes sauvegardes pourraient donner les mauvaises armes sans erreur. À confirmer en rechargeant une sauvegarde après ajout d'un `.wpn`.
- **[P] Items créés à la volée côté cgame** (`CG_RegisterWeapon` → `bg_numItems++`). L'index d'un item dépend de l'ordre d'enregistrement ; les armes `playerUsable` sont enregistrées dans l'ordre (`cg_main.cpp` ~1913), les autres à l'usage. Un item stocké avec son `modelindex` dans une sauvegarde pourrait changer d'identité.
- **[C]** `MAX_WEAPONS 128`, `MAX_AMMO 32`, `MAX_ITEMS = MAX_WEAPONS + 64` sont les plafonds ; `MAX_WEAPONS` (1.1) et `MAX_ITEMS` (1.5) sont maintenant testés.
- **[C]** Le dispatch de tir est une liste de `FL_*` figée : un nouveau comportement de tir exige du code. Les `missileFuncName` sont limités à la table `funcs[]` (25 entrées) de `g_weaponLoad.h`.

## 9. Ajouts et changements hors audit

- **Faisceau (`WP_FireGenericBeam`)** : nouvelles clés d'attaque `selfKnockback`, `pushForce`, `knockdownForce`, `beamRadius`, `beamTrailEffect` (`-1` = non défini, héritées, ramenées à 0 après héritage) ; `methodOfDeath` lu dans les données. Les trois premières ont pour borne `[0, 1000]`.
- **Règle Galak** dans `G_Damage` (`g_combat.cpp`) : plafond de dégâts pour `MOD_DISRUPTOR`/`MOD_SNIPER` sur `CLASS_GALAKMECH` (3 pour le tir principal, 10 pour le chargé), à la place du cas codé en dur dans l'arme. Le tir principal est reconnu par `DAMAGE_DEATH_KNOCKBACK` : fragile.
- **`WPN_Warn`** : tous les warnings du chargeur portent le nom de l'arme ; `ATK_NpcSpread` en float.
- **`IsScopedZoom()` / `is_player_scoped()`** déclarés dans `g_local.h`.
- **Notion d'attaque unique (`attack_index`).** Deux boutons restent (`+attack`, `+altattack`) ; `WP_ResolveAttackIndex` (`wp_fire_logic.cpp`) est le seul endroit qui les convertit en index 0 main, 1 alt, 2 scoped_main, 3 scoped_alt (joueur zoomé avec lunette : scoped, une scoped_alt absente retombe sur scoped_main). `PM_AdjustAttackStates` l'écrit dans `ps.attack_index` ; ensuite pmove, animations, événements, cgame et missiles ne lisent que l'index. `EV_ALT_FIRE` / `EV_SCOPED_*` deviennent `EV_FIRE_WEAPON + index` ; `WEAPON_CHARGING_ALT` disparaît (la charge garde son index) ; `EF_ALT_FIRING` devient `EF_LIGHT_CONE` (spots seulement) ; `gentity.alt_fire` ne sert plus aux armes (mouvers, tourelles, sabres) ; l'animation event `AEV_FIRE` donne un index. Restent sur les boutons bruts : sabre, mêlée (`WC_MELEE*`), véhicules (combos, turbo), et les choix des PNJ (`SCF_ALT_FIRE`, `ucmd`). Les anciennes sauvegardes ne sont plus lisibles (`playerState`, `weaponstate_t`, événements).

## 10. Ce que je n'ai pas audité

UI (`uiPcWeapon`, menu de loadout, `ui/`), HUD/viseur (`cg_draw.cpp` hors `GetAttackIndex`), vol de véhicules, IA détaillée des PNJ par classe (`AI_*.cpp`), sabre laser (`wp_saber*.cpp`), `wp_flechette.cpp`/`wp_det_pack.cpp`/`wp_trip_mine.cpp` au-delà des lectures de données, MP (`codemp/`, qui garde l'ancien système).

---

## Reste à faire, par ordre

1. **Données** : ajouter les clés de la concussion alt (section 7), corriger les clés en double et le `$` littéral.
2. **Régressions** : 2.1 (vérifier la vitesse du bowcaster) et 2.2.
3. **Chargeur** : résidu de 1.1/4.6, héritage de `startSnd`/`readySnd` (3.9), champs jamais hérités (4.12), même sortie sur EOF dans `g_itemLoad.cpp` et `g_charactersLoad.cpp`.
4. **Remplacer les listes d'IDs par `baseWeaponNum` ou une propriété de donnée** (section 6), en commençant par PNJ et grenades.
5. **Un test de cohérence au chargement** : avertir pour `scopeType` sans scoped, `FL_*_CHARGED` sans `chargeUnitTime`, clé en double, `chargeSnd` sur une logique non chargée, `weaponBucket` absent. Le warning « scoped sans `attackType` » existe déjà.
