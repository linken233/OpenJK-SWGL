# Validation de Z_SWGL_CharMigration.pk3

Généré le 2026-10-23 par script (parse des `.cha`, `_def.cfg`, `.str`, `.npc` et index de tous les pk3 de `GameData/SWGL` + `base`).

## Résumé

| Contrôle | Résultat |
|---|---|
| Variantes SWGL_Characters (`.cha` d'origine) | **1443** |
| Variantes Z_SWGL_CharMigration | **1411** : **32 de moins** (liste plus bas) |
| Persos (origine / migration) | 1093 / 1052 |
| Variantes sans `_def.cfg` dans le pk3 | **0** (1411 cfg pour 1411 variantes, aucun cfg orphelin) |
| `.cha` d'origine non masqués par un stub | 0 (41 stubs pour 41 `.cha`) |
| Persos : nom / icône (fichier existe) / ≥1 faction / ≥1 tag | **OK pour les 1052** |
| Persos : description | **Pas de champ description au niveau perso dans le format** ; 98 persos n'ont aucune description de variante |
| Variantes : nom / icône (fichier existe) | **OK pour les 1411** |
| Variantes : `.npc` | **OK pour les 1411** (cfg `ui_npc_type`, `npcName` ou `npcRandom`, tous existent) |
| Variantes : description | **116 manquantes** (clé absente des `.str`) |
| Variantes : modèle | 1410 OK, **1 sans `model.glm`** (ORGUS) |

## 1. Les 32 variantes absentes de la migration

Chacune avait son propre `.npc` et son `_def.cfg` dans SWGL_Characters. Aucune ne se retrouve dans le nouveau pk3 (ni sous un autre code).

| Ancien perso / variante | Nom | Modèle | NPC |
|---|---|---|---|
| BH_AURRA / HUNTER | Aurra Sing (Jedi Hunter) | aurrasing | Aurra_Sing_Hunter |
| DRSE_DARKJEDI / MASTER | Dark Jedi Master | darkjedi | Dark_Jedi_Master |
| DRSE_TRAYA / KREIA | Kreia | traya | kreia |
| GE_PURGE / BATONS | Purge Trooper (Batons) | purgetrooper | Purge_Trooper_Batons |
| GE_PURGE / COMMANDER | Purge Trooper Commander | purgetrooper | Purge_Trooper_Commander |
| GE_EMPEROR_PALPATINE / SABER | Emperor Palpatine | lord_sidious | Darth_Sidious |
| INF_MAUL_OW / DEFAULT | Darth Maul (Old Wounds) | cybernetic_maul | Maul_OW |
| IR_CULTIST / SABER | Cultist (Saber) | cultist | cultist_saber |
| IR_CULTIST / GRIP | Cultist (Grip) | cultist | cultist_grip |
| IR_CULTIST / LIGHTNING | Cultist (Lightning) | cultist | cultist_lightning |
| IR_CULTIST / DRAIN | Cultist (Drain) | cultist | cultist_drain |
| IR_REBORN / ACROBAT | Reborn Acrobat | reborn | rebornacrobat |
| IR_REBORN / FORCEUSER | Reborn Force User | reborn | rebornforceuser |
| IR_REBORN / FENCER | Reborn Fencer | reborn | rebornfencer |
| IR_REBORN / BOSS | Reborn Boss | reborn | rebornboss |
| IR_TAVION / POSSESSED | Tavion Axmis (Possessed) | tavion_new | tavion_sith_sword |
| JO_JEDI12 / DEFAULT | Jedi Knight | jedi_spanki2a | Prequel_Jedi13 |
| JO_JEDI8 / DEFAULT | Jedi Knight | jedi_spanki | Prequel_Jedi13 |
| NR_JEDI_NR2 / DEFAULT | Jedi Knight | jedi_hf | Jedi_hf1 |
| REP_PALPATINE / SITH | Darth Sidious | palpatine | Palpatine_Sith |
| RES_REY / JEDI_TRAINING | Rey (Jedi Training) | rey | Rey_Jedi_Training |
| RES_REY / SKYWALKER | Rey Skywalker | rey_skywalker | Rey_Skywalker |
| SO_SITH2 / DEFAULT | Sith Lord | jedi_sithspanki | SWTOR_Sith7 |
| TOR_TOR_JEDI3 / DEFAULT | Jedi Knight | kotorjedi_twilekf | KOTOR_Jedi3 |
| TOR_ATRIS / DARK | Atris (Fallen) | atris | Atris_dark |
| TOR_BAODUR / JEDI | Bao-Dur (Jedi) | bao_dur | baodur_jedi |
| TOR_BRIANNA / JEDI | Brianna (Jedi) | brianna | brianna_jedi |
| TOR_MIRA / JEDI | Mira (Jedi) | mira | Mira_Jedi |
| TOR_BAKARN / FIRST_SON | The First Son | bakarn | First_Son |
| TOR_ATTON / JEDI | Atton Rand (Jedi) | atton | atton_rand_jedi |
| TOR_BASTILA / DARK | Bastila Shan (Fallen) | bastila | bastila_dark |
| TOR_MICAL / JEDI | Mical (Jedi) | mical | Mical_Jedi |

Plusieurs sont peut-être voulues (Reborn 4 rôles et Cultist 4 pouvoirs remplacés par un tirage ?). `REBORN`/`CULTIST` n'ont dans le nouveau pk3 qu'une variante `DEFAULT` (+ `ELITE` pour Reborn) sans `npcRandom`. A confirmer.

## 2. Descriptions manquantes (116 variantes, 99 persos)

La `descriptionKey` existe mais la clé n'est dans aucun `.str` (`chardesc*.str` de SWGL_Menu.pk3). Même situation dans l'ancien pk3 (les clés n'ont pas changé).

- `CHARDESCJEDI_SITH_JEDI#` : 38 variantes (JEDI10/DEFAULT, JEDI11/DEFAULT, JEDI13/DEFAULT, JEDI14/DEFAULT, JEDI15/DEFAULT, JEDI16/DEFAULT …)
- `CHARDESCJEDI_SITH_SITH#` : 28 variantes (SITH1/DEFAULT, SITH10/DEFAULT, SITH11/DEFAULT, SITH12/DEFAULT, SITH13/DEFAULT, SITH14/DEFAULT …)
- `CHARDESCREPUBLIC_JEDI#` : 14 variantes (JEDI1/DEFAULT, JEDI10/JO_DEFAULT, JEDI11/JO_DEFAULT, JEDI12/DEFAULT, JEDI13/JO_DEFAULT, JEDI3/JO_DEFAULT …)
- `CHARDESCREBELLION_JEDI_NR#` : 6 variantes (JEDI_NR1/DEFAULT, JEDI_NR3/DEFAULT, JEDI_NR4/DEFAULT, JEDI_NR5/DEFAULT, JEDI_NR6/DEFAULT, JEDI_NR7/DEFAULT)
- `CHARDESCJO_JEDI_YOUNGLING` : 6 variantes (JEDI_YOUNGLING/DEFAULT, JEDI_YOUNGLING/FEMALE, JEDI_YOUNGLING/TOGRUTA, JEDI_YOUNGLING/ALIEN1, JEDI_YOUNGLING/ALIEN2, JEDI_YOUNGLING/RANDOM)
- `CHARDESCCRIMINALS_ALIEN_MERC` : 3 variantes (ALIEN_MERC/ALIEN, ALIEN_MERC/GRAN, ALIEN_MERC/NIKTO)
- `CHARDESCCIS_AERIAL` : 1 variantes (AERIAL/DEFAULT)
- `CHARDESCREPUBLIC_AQUA` : 1 variantes (AQUA_REP/DEFAULT)
- `CHARDESCCRIMINALS_BRUTUS` : 1 variantes (BRUTUS/DEFAULT)
- `CHARDESCCIS_BUZZ` : 1 variantes (BUZZ_CIS/DEFAULT)
- `CHARDESCINFINITIES_CREMISI` : 1 variantes (CREMISI/DEFAULT)
- `CHARDESCCIS_ENGINEER` : 1 variantes (ENGINEER_CIS/DEFAULT)
- `CHARDESCCIVILIANS_FALLEEN` : 1 variantes (FALLEEN/DEFAULT)
- `CHARDESCCIS_HEAVY` : 1 variantes (HEAVY_CIS/DEFAULT)
- `CHARDESCREB_JAXXON` : 1 variantes (JAXXON/DEFAULT)
- `CHARDESCNR_JEDI_RANDOM` : 1 variantes (JEDI_RANDOM/DEFAULT)
- `CHARDESCTOR_KOTOR_JEDI` : 1 variantes (KOTOR_JEDI/DEFAULT)
- `CHARDESCREBELLION_LENSI` : 1 variantes (LENSI/DEFAULT)
- `CHARDESCCRIMINALS_NIZUC` : 1 variantes (NIZUC/DEFAULT)
- `CHARDESCTSE_OFFICER` : 1 variantes (OFFICER_DRSE/DEFAULT)
- `CHARDESCREP_PREQUEL_JEDI` : 1 variantes (PREQUEL_JEDI/DEFAULT)
- `CHARDESCEMPIRE_SUPERCOMMANDO` : 1 variantes (SUPERCOMMANDO/DEFAULT)
- `CHARDESCJO_SWTOR_JEDI` : 1 variantes (SWTOR_JEDI/DEFAULT)
- `CHARDESCSO_SWTOR_SITH` : 1 variantes (SWTOR_SITH/DEFAULT)
- `CHARDESCREBELLION_TRA` : 1 variantes (TRA/DEFAULT)
- `CHARDESCCRIMINALS_VIGOMORN` : 1 variantes (VIGOMORN/DEFAULT)
- `CHARDESCREBELLION_X#` : 1 variantes (X2/DEFAULT)

Liste complète :

```
AERIAL/DEFAULT [SWGL_A.cha:237] -> CHARDESCCIS_AERIAL
ALIEN_MERC/ALIEN [SWGL_A.cha:451] -> CHARDESCCRIMINALS_ALIEN_MERC
ALIEN_MERC/GRAN [SWGL_A.cha:461] -> CHARDESCCRIMINALS_ALIEN_MERC
ALIEN_MERC/NIKTO [SWGL_A.cha:471] -> CHARDESCCRIMINALS_ALIEN_MERC
AQUA_REP/DEFAULT [SWGL_A.cha:867] -> CHARDESCREPUBLIC_AQUA
BRUTUS/DEFAULT [SWGL_B.cha:1181] -> CHARDESCCRIMINALS_BRUTUS
BUZZ_CIS/DEFAULT [SWGL_B.cha:1271] -> CHARDESCCIS_BUZZ
CREMISI/DEFAULT [SWGL_C.cha:1055] -> CHARDESCINFINITIES_CREMISI
ENGINEER_CIS/DEFAULT [SWGL_E.cha:173] -> CHARDESCCIS_ENGINEER
FALLEEN/DEFAULT [SWGL_F.cha:9] -> CHARDESCCIVILIANS_FALLEEN
HEAVY_CIS/DEFAULT [SWGL_H.cha:249] -> CHARDESCCIS_HEAVY
JAXXON/DEFAULT [SWGL_J.cha:389] -> CHARDESCREB_JAXXON
JEDI_NR1/DEFAULT [SWGL_J.cha:425] -> CHARDESCREBELLION_JEDI_NR1
JEDI_NR3/DEFAULT [SWGL_J.cha:443] -> CHARDESCREBELLION_JEDI_NR3
JEDI_NR4/DEFAULT [SWGL_J.cha:461] -> CHARDESCREBELLION_JEDI_NR4
JEDI_NR5/DEFAULT [SWGL_J.cha:479] -> CHARDESCREBELLION_JEDI_NR5
JEDI_NR6/DEFAULT [SWGL_J.cha:497] -> CHARDESCREBELLION_JEDI_NR6
JEDI_NR7/DEFAULT [SWGL_J.cha:515] -> CHARDESCREBELLION_JEDI_NR7
JEDI_RANDOM/DEFAULT [SWGL_J.cha:533] -> CHARDESCNR_JEDI_RANDOM
JEDI_YOUNGLING/DEFAULT [SWGL_J.cha:570] -> CHARDESCJO_JEDI_YOUNGLING
JEDI_YOUNGLING/FEMALE [SWGL_J.cha:581] -> CHARDESCJO_JEDI_YOUNGLING
JEDI_YOUNGLING/TOGRUTA [SWGL_J.cha:592] -> CHARDESCJO_JEDI_YOUNGLING
JEDI_YOUNGLING/ALIEN1 [SWGL_J.cha:603] -> CHARDESCJO_JEDI_YOUNGLING
JEDI_YOUNGLING/ALIEN2 [SWGL_J.cha:614] -> CHARDESCJO_JEDI_YOUNGLING
JEDI_YOUNGLING/RANDOM [SWGL_J.cha:625] -> CHARDESCJO_JEDI_YOUNGLING
JEDI1/DEFAULT [SWGL_J.cha:647] -> CHARDESCREPUBLIC_JEDI1
JEDI10/DEFAULT [SWGL_J.cha:665] -> CHARDESCJEDI_SITH_JEDI10
JEDI10/JO_DEFAULT [SWGL_J.cha:675] -> CHARDESCREPUBLIC_JEDI10
JEDI11/DEFAULT [SWGL_J.cha:693] -> CHARDESCJEDI_SITH_JEDI11
JEDI11/JO_DEFAULT [SWGL_J.cha:703] -> CHARDESCREPUBLIC_JEDI11
JEDI12/DEFAULT [SWGL_J.cha:721] -> CHARDESCREPUBLIC_JEDI12
JEDI13/DEFAULT [SWGL_J.cha:739] -> CHARDESCJEDI_SITH_JEDI13
JEDI13/JO_DEFAULT [SWGL_J.cha:749] -> CHARDESCREPUBLIC_JEDI13
JEDI14/DEFAULT [SWGL_J.cha:767] -> CHARDESCJEDI_SITH_JEDI14
JEDI15/DEFAULT [SWGL_J.cha:785] -> CHARDESCJEDI_SITH_JEDI15
JEDI16/DEFAULT [SWGL_J.cha:803] -> CHARDESCJEDI_SITH_JEDI16
JEDI17/DEFAULT [SWGL_J.cha:821] -> CHARDESCJEDI_SITH_JEDI17
JEDI18/DEFAULT [SWGL_J.cha:839] -> CHARDESCJEDI_SITH_JEDI18
JEDI19/DEFAULT [SWGL_J.cha:857] -> CHARDESCJEDI_SITH_JEDI19
JEDI2/DEFAULT [SWGL_J.cha:875] -> CHARDESCJEDI_SITH_JEDI2
JEDI20/DEFAULT [SWGL_J.cha:893] -> CHARDESCJEDI_SITH_JEDI20
JEDI21/DEFAULT [SWGL_J.cha:911] -> CHARDESCJEDI_SITH_JEDI21
JEDI22/DEFAULT [SWGL_J.cha:929] -> CHARDESCJEDI_SITH_JEDI22
JEDI23/DEFAULT [SWGL_J.cha:947] -> CHARDESCJEDI_SITH_JEDI23
JEDI24/DEFAULT [SWGL_J.cha:965] -> CHARDESCJEDI_SITH_JEDI24
JEDI25/DEFAULT [SWGL_J.cha:983] -> CHARDESCJEDI_SITH_JEDI25
JEDI26/DEFAULT [SWGL_J.cha:1001] -> CHARDESCJEDI_SITH_JEDI26
JEDI27/DEFAULT [SWGL_J.cha:1019] -> CHARDESCJEDI_SITH_JEDI27
JEDI28/DEFAULT [SWGL_J.cha:1037] -> CHARDESCJEDI_SITH_JEDI28
JEDI29/DEFAULT [SWGL_J.cha:1055] -> CHARDESCJEDI_SITH_JEDI29
JEDI3/DEFAULT [SWGL_J.cha:1073] -> CHARDESCJEDI_SITH_JEDI3
JEDI3/JO_DEFAULT [SWGL_J.cha:1083] -> CHARDESCREPUBLIC_JEDI3
JEDI30/DEFAULT [SWGL_J.cha:1101] -> CHARDESCJEDI_SITH_JEDI30
JEDI31/DEFAULT [SWGL_J.cha:1119] -> CHARDESCJEDI_SITH_JEDI31
JEDI32/DEFAULT [SWGL_J.cha:1137] -> CHARDESCJEDI_SITH_JEDI32
JEDI33/DEFAULT [SWGL_J.cha:1155] -> CHARDESCJEDI_SITH_JEDI33
JEDI34/DEFAULT [SWGL_J.cha:1173] -> CHARDESCJEDI_SITH_JEDI34
JEDI35/DEFAULT [SWGL_J.cha:1191] -> CHARDESCREPUBLIC_JEDI35
JEDI35/ALT [SWGL_J.cha:1201] -> CHARDESCREPUBLIC_JEDI35
JEDI36/DEFAULT [SWGL_J.cha:1219] -> CHARDESCJEDI_SITH_JEDI36
JEDI37/DEFAULT [SWGL_J.cha:1237] -> CHARDESCJEDI_SITH_JEDI37
JEDI38/DEFAULT [SWGL_J.cha:1255] -> CHARDESCJEDI_SITH_JEDI38
JEDI39/DEFAULT [SWGL_J.cha:1273] -> CHARDESCJEDI_SITH_JEDI39
JEDI4/DEFAULT [SWGL_J.cha:1291] -> CHARDESCJEDI_SITH_JEDI4
JEDI4/JO_DEFAULT [SWGL_J.cha:1301] -> CHARDESCREPUBLIC_JEDI4
JEDI40/DEFAULT [SWGL_J.cha:1319] -> CHARDESCJEDI_SITH_JEDI40
JEDI41/DEFAULT [SWGL_J.cha:1337] -> CHARDESCJEDI_SITH_JEDI41
JEDI42/DEFAULT [SWGL_J.cha:1355] -> CHARDESCJEDI_SITH_JEDI42
JEDI5/DEFAULT [SWGL_J.cha:1373] -> CHARDESCJEDI_SITH_JEDI5
JEDI5/JO_DEFAULT [SWGL_J.cha:1383] -> CHARDESCREPUBLIC_JEDI5
JEDI6/DEFAULT [SWGL_J.cha:1401] -> CHARDESCJEDI_SITH_JEDI6
JEDI6/JO_DEFAULT [SWGL_J.cha:1411] -> CHARDESCREPUBLIC_JEDI6
JEDI7/DEFAULT [SWGL_J.cha:1429] -> CHARDESCJEDI_SITH_JEDI7
JEDI7/JO_DEFAULT [SWGL_J.cha:1439] -> CHARDESCREPUBLIC_JEDI7
JEDI8/DEFAULT [SWGL_J.cha:1457] -> CHARDESCREPUBLIC_JEDI8
JEDI9/DEFAULT [SWGL_J.cha:1475] -> CHARDESCJEDI_SITH_JEDI9
JEDI9/JO_DEFAULT [SWGL_J.cha:1485] -> CHARDESCREPUBLIC_JEDI9
KOTOR_JEDI/DEFAULT [SWGL_K.cha:1017] -> CHARDESCTOR_KOTOR_JEDI
LENSI/DEFAULT [SWGL_L.cha:443] -> CHARDESCREBELLION_LENSI
NIZUC/DEFAULT [SWGL_N.cha:479] -> CHARDESCCRIMINALS_NIZUC
OFFICER_DRSE/DEFAULT [SWGL_O.cha:117] -> CHARDESCTSE_OFFICER
PREQUEL_JEDI/DEFAULT [SWGL_P.cha:553] -> CHARDESCREP_PREQUEL_JEDI
SITH1/DEFAULT [SWGL_S.cha:1453] -> CHARDESCJEDI_SITH_SITH1
SITH10/DEFAULT [SWGL_S.cha:1471] -> CHARDESCJEDI_SITH_SITH10
SITH11/DEFAULT [SWGL_S.cha:1489] -> CHARDESCJEDI_SITH_SITH11
SITH12/DEFAULT [SWGL_S.cha:1507] -> CHARDESCJEDI_SITH_SITH12
SITH13/DEFAULT [SWGL_S.cha:1525] -> CHARDESCJEDI_SITH_SITH13
SITH14/DEFAULT [SWGL_S.cha:1543] -> CHARDESCJEDI_SITH_SITH14
SITH15/DEFAULT [SWGL_S.cha:1561] -> CHARDESCJEDI_SITH_SITH15
SITH16/DEFAULT [SWGL_S.cha:1579] -> CHARDESCJEDI_SITH_SITH16
SITH17/DEFAULT [SWGL_S.cha:1597] -> CHARDESCJEDI_SITH_SITH17
SITH18/DEFAULT [SWGL_S.cha:1615] -> CHARDESCJEDI_SITH_SITH18
SITH19/DEFAULT [SWGL_S.cha:1633] -> CHARDESCJEDI_SITH_SITH19
SITH20/DEFAULT [SWGL_S.cha:1651] -> CHARDESCJEDI_SITH_SITH20
SITH21/DEFAULT [SWGL_S.cha:1669] -> CHARDESCJEDI_SITH_SITH21
SITH22/DEFAULT [SWGL_S.cha:1687] -> CHARDESCJEDI_SITH_SITH22
SITH23/DEFAULT [SWGL_S.cha:1705] -> CHARDESCJEDI_SITH_SITH23
SITH24/DEFAULT [SWGL_S.cha:1723] -> CHARDESCJEDI_SITH_SITH24
SITH25/DEFAULT [SWGL_S.cha:1741] -> CHARDESCJEDI_SITH_SITH25
SITH26/DEFAULT [SWGL_S.cha:1759] -> CHARDESCJEDI_SITH_SITH26
SITH27/DEFAULT [SWGL_S.cha:1777] -> CHARDESCJEDI_SITH_SITH27
SITH28/DEFAULT [SWGL_S.cha:1795] -> CHARDESCJEDI_SITH_SITH28
SITH29/DEFAULT [SWGL_S.cha:1813] -> CHARDESCJEDI_SITH_SITH29
SITH3/DEFAULT [SWGL_S.cha:1831] -> CHARDESCJEDI_SITH_SITH3
SITH4/DEFAULT [SWGL_S.cha:1849] -> CHARDESCJEDI_SITH_SITH4
SITH5/DEFAULT [SWGL_S.cha:1867] -> CHARDESCJEDI_SITH_SITH5
SITH6/DEFAULT [SWGL_S.cha:1885] -> CHARDESCJEDI_SITH_SITH6
SITH7/DEFAULT [SWGL_S.cha:1903] -> CHARDESCJEDI_SITH_SITH7
SITH8/DEFAULT [SWGL_S.cha:1921] -> CHARDESCJEDI_SITH_SITH8
SITH9/DEFAULT [SWGL_S.cha:1939] -> CHARDESCJEDI_SITH_SITH9
SUPERCOMMANDO/DEFAULT [SWGL_S.cha:2429] -> CHARDESCEMPIRE_SUPERCOMMANDO
SWTOR_JEDI/DEFAULT [SWGL_S.cha:2483] -> CHARDESCJO_SWTOR_JEDI
SWTOR_SITH/DEFAULT [SWGL_S.cha:2506] -> CHARDESCSO_SWTOR_SITH
TRA/DEFAULT [SWGL_T.cha:1107] -> CHARDESCREBELLION_TRA
VIGOMORN/DEFAULT [SWGL_V.cha:331] -> CHARDESCCRIMINALS_VIGOMORN
X2/DEFAULT [SWGL_X.cha:47] -> CHARDESCREBELLION_X2
```

## 3. Autres anomalies

### Modèle introuvable
- `ORGUS/DEFAULT` : modèle `MasterOrgusDin`, aucun fichier `models/players/MasterOrgusDin/` dans les pk3. L'icône et le NPC existent.

### Skin `model_default.skin` introuvable (informatif)

Le modèle existe mais pas ce skin ; probable pour les modèles à skins par partie (head/torso/lower) ou sans skin (mark1, mark2). A vérifier en jeu.

- ARC_HAVOC/DEFAULT [SWGL_A.cha:949] -> TCWArcs/model_default
- IMPOFFICER/B_COAT [SWGL_I.cha:317] -> imperial_trench2/model_default
- IMPOFFICER/I_COAT [SWGL_I.cha:327] -> imperial_trench3/model_default
- JEDI_YOUNGLING/DEFAULT [SWGL_J.cha:570] -> youngmale/model_default
- JEDI_YOUNGLING/FEMALE [SWGL_J.cha:581] -> youngfemale/model_default
- JEDI_YOUNGLING/ALIEN1 [SWGL_J.cha:603] -> alienyounglings1/model_default
- JEDI_YOUNGLING/ALIEN2 [SWGL_J.cha:614] -> alienyounglings2/model_default
- JEDI_YOUNGLING/RANDOM [SWGL_J.cha:625] -> youngmale/model_default
- JEDI41/DEFAULT [SWGL_J.cha:1337] -> jedi_chewy/model_default
- MARK1/DEFAULT [SWGL_M.cha:377] -> mark1/model_default
- MARK2/DEFAULT [SWGL_M.cha:395] -> mark2/model_default
- ORGUS/DEFAULT [SWGL_O.cha:261] -> MasterOrgusDin/model_default
- TIE_PILOT/RESERVE [SWGL_T.cha:763] -> TIEReserve/model_default
- TIE_PILOT/F_RESERVE [SWGL_T.cha:773] -> TIEReserveFem/model_default

### Nom avec espace final
- `CHAZRACH` (perso et variante) : `"Chazrach "`, `SWGL_C.cha:441` et `:449`.

### Variantes d'un même perso avec le même nom

Indiscernables dans le menu. **72 persos** concernés :

- `212TH` (SWGL_0-9.cha:55) : « 212th airborne trooper » x2
- `41ST` (SWGL_0-9.cha:211) : « 41st clone trooper » x2
- `91ST` (SWGL_0-9.cha:405) : « 91st clone trooper » x2
- `ALIEN_MERC` (SWGL_A.cha:443) : « alien mercenary » x3
- `ANAKIN_SKYWALKER` (SWGL_A.cha:545) : « anakin skywalker (redeemed) » x4, « darth vader » x3
- `ARC` (SWGL_A.cha:913) : « arc trooper » x2
- `BATTLE_DROID` (SWGL_B.cha:291) : « battle droid » x2
- `BEN_SKYWALKER` (SWGL_B.cha:535) : « ben skywalker » x2
- `BO_KATAN` (SWGL_B.cha:771) : « bo-katan kryze » x2
- `CLONE_PILOT` (SWGL_C.cha:729) : « clone pilot » x2
- `CROSSHAIR` (SWGL_C.cha:1083) : « crosshair » x2
- `DANT_MIL` (SWGL_D.cha:55) : « dantooine militiaman » x2
- `DEPA` (SWGL_D.cha:363) : « depa billaba » x2
- `DEVARONIAN` (SWGL_D.cha:445) : « devaronian » x2
- `DUROS` (SWGL_D.cha:837) : « duros » x2
- `ECHO` (SWGL_E.cha:19) : « echo » x2
- `EVEN` (SWGL_E.cha:291) : « even piell » x2
- `HAN_SOLO` (SWGL_H.cha:37) : « han solo » x2
- `HAVOC` (SWGL_H.cha:177) : « havoc squad trooper » x2
- `HONDO` (SWGL_H.cha:439) : « hondo ohnaka » x2
- `HUMAN_F` (SWGL_H.cha:539) : « human female » x2
- `IDEN_VERSIO` (SWGL_I.cha:19) : « iden versio » x2
- `IG88` (SWGL_I.cha:83) : « ig-88 » x3
- `ISB_AGENT` (SWGL_I.cha:483) : « isb agent » x2
- `ITHORIAN` (SWGL_I.cha:529) : « ithorian » x2
- `JAN` (SWGL_J.cha:223) : « jan ors » x2, « jan ors (mercenary) » x2
- `JAWA` (SWGL_J.cha:353) : « jawa » x2
- `JEDI35` (SWGL_J.cha:1183) : « gran jedi knight » x2
- `KIRA` (SWGL_K.cha:669) : « kira carsen » x2
- `KREEL` (SWGL_K.cha:1069) : « sergeant kreel » x2
- `KYLE` (SWGL_K.cha:1215) : « kyle katarn (journeyman) » x2
- `KYLO` (SWGL_K.cha:1293) : « ben solo » x2
- `LANA` (SWGL_L.cha:55) : « lana beniko » x2
- `LANDO_CALRISSIAN` (SWGL_L.cha:83) : « lando calrissian » x2
- `LEIA_ORGANA` (SWGL_L.cha:249) : « leia organa (mimban) » x2
- `LUKE_SKYWALKER` (SWGL_L.cha:579) : « luke skywalker » x2, « luke skywalker (mimban) » x2
- `LUMIYA` (SWGL_L.cha:813) : « lumiya » x2
- `MALGUS` (SWGL_M.cha:145) : « darth malgus » x2
- `MANDALORIAN` (SWGL_M.cha:191) : « mandalorian » x3
- `MOHC` (SWGL_M.cha:843) : « general rohm mohc » x2
- `MON_MOTHMA` (SWGL_M.cha:871) : « mon mothma » x2
- `NAHDAR` (SWGL_N.cha:109) : « nahdar vebb » x2
- `NOGHRI` (SWGL_N.cha:489) : « noghri » x2
- `NR_OFFICER` (SWGL_N.cha:625) : « new republic officer » x2
- `PADME` (SWGL_P.cha:19) : « senator amidala » x2
- `PLO` (SWGL_P.cha:327) : « plo koon » x2
- `PURGE` (SWGL_P.cha:640) : « purge trooper » x2
- `PYKE` (SWGL_P.cha:668) : « pyke syndicate member » x2
- `QUARREN` (SWGL_Q.cha:37) : « quarren » x2
- `RAKGHOUL` (SWGL_R.cha:183) : « rakghoul » x2
- `REBEL` (SWGL_R.cha:367) : « rebel trooper » x4
- `REBEL_PILOT` (SWGL_R.cha:469) : « rebel pilot » x4
- `REX` (SWGL_R.cha:947) : « rex » x2
- `RODIAN` (SWGL_R.cha:1095) : « rodian » x2
- `SABINE` (SWGL_S.cha:37) : « sabine wren » x2
- `SAESEE` (SWGL_S.cha:119) : « saesee tiin » x2
- `SHADOW_COLLECTIVE` (SWGL_S.cha:779) : « shadow collective member » x2
- `SIRI` (SWGL_S.cha:1327) : « siri tachi » x2
- `SNOWTROOPER` (SWGL_S.cha:2075) : « snowtrooper » x2
- `STORMTROOPER` (SWGL_S.cha:2303) : « stormtrooper » x2
- `TALON` (SWGL_T.cha:127) : « darth talon » x2
- `TIE_PILOT` (SWGL_T.cha:735) : « tie pilot » x5
- `TRANDOSHAN` (SWGL_T.cha:1207) : « trandoshan » x2
- `TUSKEN` (SWGL_T.cha:1407) : « tusken raider » x3
- `TWILEK` (SWGL_T.cha:1463) : « twi'lek male » x2
- `VALERIA` (SWGL_V.cha:1) : « valeria » x2
- `VANGUARD` (SWGL_V.cha:83) : « rebel vanguard » x2
- `VETTE` (SWGL_V.cha:277) : « vette » x2
- `WARRIOR` (SWGL_W.cha:65) : « sith juggernaut » x2
- `X1` (SWGL_X.cha:1) : « x1 » x2
- `X2` (SWGL_X.cha:39) : « x2 » x2
- `ZAHRA` (SWGL_Z.cha:19) : « ellian zahra » x2

### Noms de persos identiques (12 noms)

« Jedi Knight » x50 (JEDI1…JEDI42, JEDI_NR*, TOR_JEDI*), « Sith Lord » x29, « Imperial Officer » x3, « Sith Trooper » x3, « Coruscant Guard », « Death Trooper » (DEATHTROOPER / ZOMBIE), « First Order Officer », « TIE Pilot », « Praetorian Guard », « Old Republic Jedi », « Stormtrooper » (STORM_TFA / STORMTROOPER), « Wookiee Warrior ». Normal pour les génériques ; `DEATHTROOPER`/`ZOMBIE` est plus étonnant.

### Modèle du `.npc` différent du modèle de la variante (262)

Hérité de l'ancien pk3 (les `_def.cfg` sont identiques octet pour octet à ceux de SWGL_Characters). Le NPC spawné n'a alors pas le modèle affiché dans le menu. 125 NPC sont partagés entre plusieurs variantes, ce qui l'explique en grande partie (ex. `Adi_Gallia/TCW` utilise le NPC `Adi_Gallia` du modèle `Adi_Gallia`, pas `adi_gallia_cw`). Liste complète :

```
212TH/AIRBORNE2 [SWGL_0-9.cha:83] variante=cbe npc(212th_Jet)=clonetrooper_ab
327TH/DEFAULT [SWGL_0-9.cha:165] variante=327th_p1 npc(327th_Trooper)=clone_rgb
41ST/PHASEI [SWGL_0-9.cha:229] variante=41stcorps npc(41st_Trooper)=cloneultima
ADI/TCW [SWGL_A.cha:219] variante=adi_gallia_cw npc(Adi_Gallia)=Adi_Gallia
AHSOKA_TANO/PADAWAN [SWGL_A.cha:293] variante=ahsoka_totj npc(Ahsoka_Tano)=AhsokaV3
AHSOKA_TANO/NOMAD [SWGL_A.cha:323] variante=Ahsoka_Mando npc(Ahsoka_Tano_Rebels)=ahsoka_rebels
AHSOKA_TANO/INF_DEFAULT [SWGL_A.cha:333] variante=Ahsoka_Gen npc(Ahsoka_Tano_Siege)=ahsoka_S7
ALIEN_MERC/GRAN [SWGL_A.cha:461] variante=gran2 npc(alien_merc)=alien_mercs
ALIEN_MERC/NIKTO [SWGL_A.cha:471] variante=nikto npc(alien_merc)=alien_mercs
ANAKIN_SKYWALKER/GENERAL [SWGL_A.cha:573] variante=anakin_totj npc(Ep3_Anakin)=anakin
ANAKIN_SKYWALKER/MIDWAR [SWGL_A.cha:583] variante=anakin_totj2 npc(Ep3_Anakin)=anakin
ANAKIN_SKYWALKER/NELVAAN [SWGL_A.cha:593] variante=ani_nlvn npc(Ep3_Anakin)=anakin
ANAKIN_SKYWALKER/COMMANDER [SWGL_A.cha:603] variante=ani_comm npc(Ep2_Anakin)=anakin_ep2
ANAKIN_SKYWALKER/ALT [SWGL_A.cha:663] variante=vader npc(Darth_Vader)=am_vader
ANAKIN_SKYWALKER/ALT2 [SWGL_A.cha:673] variante=vader2 npc(Darth_Vader)=am_vader
ANAKIN_SKYWALKER/INF_ALT [SWGL_A.cha:703] variante=anakin_suit npc(Vader_Infinities)=vader_infinities
ARC/ALT [SWGL_A.cha:931] variante=cloneultarck npc(ARC_Trooper)=arc
ATRIS/TRAYA [SWGL_A.cha:1241] variante=atris_sith npc(Atris_dark)=Atris
BANE/DEFAULT [SWGL_B.cha:135] variante=DarthBane npc(Darth_Bane)=darth_bane
BANE/SPIRIT [SWGL_B.cha:155] variante=bane_cw npc(Darth_Bane)=darth_bane
BATTLE_DROID/RC [SWGL_B.cha:309] variante=bd_rc npc(Battle_Droid)=sad_bd
BATTLEDROID/DEFAULT [SWGL_B.cha:327] variante=bd_concept npc(Battle_Droid)=sad_bd
BEN_SKYWALKER/TUNIC [SWGL_B.cha:553] variante=ben_tunic npc(Ben_Skywalker)=benskywalker
BO_KATAN/DEFAULT [SWGL_B.cha:779] variante=bokatan_mnd npc(Bo-Katan)=bokatan
BOBA/DEFAULT [SWGL_B.cha:807] variante=bobafett npc(boba_fett)=boba_fett
BRUTUS/DEFAULT [SWGL_B.cha:1181] variante=brutus npc(Brutus)=burg
CLONE_PILOT/DEFAULT [SWGL_C.cha:737] variante=cp npc(Clone_Pilot)=clonepilot
COMMANDER/DEFAULT [SWGL_C.cha:855] variante=sithcommander npc(Sith_Officer)=sithofficer
CORRAN/PILOT [SWGL_C.cha:1001] variante=Corran npc(Corran_Horn)=HM_CorranHorn
CROSSHAIR/REP_DEFAULT [SWGL_C.cha:1101] variante=pcws7_crosshair npc(Crosshair)=BB_Crosshair
DANT_MIL/ALT [SWGL_D.cha:73] variante=dant_sniper npc(Dant_Militia)=dant_off
DEPA/DEFAULT [SWGL_D.cha:371] variante=depa_skirt npc(Depa_Billaba)=depabillaba
DEVARONIAN/ALT [SWGL_D.cha:463] variante=dev npc(Devaronian)=devaronian
DIN/NOBESKAR [SWGL_D.cha:491] variante=the_mando npc(The_Mandalorian)=dindjarin
DOOKU/RITUAL [SWGL_D.cha:583] variante=dr_dooku npc(Count_Dooku)=dooku
DUROS/ALT [SWGL_D.cha:855] variante=duros2 npc(Duros)=Duros
EVEN/TCW [SWGL_E.cha:309] variante=evenpiellcw npc(Even_Piell)=EvenPiell
FIREFIGHTER/DEFAULT [SWGL_F.cha:209] variante=bd_firefighter npc(Battle_Droid)=sad_bd
GALEN_MAREK/MEDICAL [SWGL_G.cha:83] variante=stk_medic_robes npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/HUNTER [SWGL_G.cha:93] variante=stk_bounty npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/CORELLIAN [SWGL_G.cha:103] variante=stk_corellian_jkt npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/TEMPLE [SWGL_G.cha:113] variante=stk_temple_eg npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/JUNGLE [SWGL_G.cha:123] variante=stk_jungle_cg npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/INDUSTRIAL [SWGL_G.cha:133] variante=stk_industrial npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/TIE [SWGL_G.cha:153] variante=starkiller_tie npc(Starkiller)=Starkiller_Clone
GALEN_MAREK/DREAM [SWGL_G.cha:163] variante=starkiller_dago npc(Starkiller_Arena)=Starkiller_Arena
GALEN_MAREK/JEDI [SWGL_G.cha:183] variante=starkiller_armor npc(Starkiller_Arena)=Starkiller_Arena
GALEN_MAREK/ARMOR [SWGL_G.cha:193] variante=starkiller_jedi npc(Starkiller_Arena)=Starkiller_Arena
GALEN_MAREK/TATOOINE [SWGL_G.cha:213] variante=lord_stk_tat npc(Lord_Starkiller)=lord_stk
GALEN_MAREK/STALKER [SWGL_G.cha:223] variante=sithstalker npc(Lord_Starkiller)=lord_stk
GALEN_MAREK/CYBERNETIC [SWGL_G.cha:233] variante=cyber_recon npc(Lord_Starkiller)=lord_stk
GALEN_MAREK/ASSASSIN [SWGL_G.cha:263] variante=stk_sith_assassin npc(Galen_Marek)=stk_training_gear
GALEN_MAREK/GENERAL [SWGL_G.cha:273] variante=stk_general npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/HCG [SWGL_G.cha:283] variante=stk_hooded_cg npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/UTILITY [SWGL_G.cha:293] variante=stk_utility_gear npc(Galen_Marek_Adventurer)=stk_adventurer
GALEN_MAREK/HEAVY [SWGL_G.cha:313] variante=stk_htraining_gear npc(Galen_Marek)=stk_training_gear
GALEN_MAREK/LIGHT [SWGL_G.cha:323] variante=stk_ltraining_gear npc(Galen_Marek)=stk_training_gear
GALEN_MAREK/EARLY [SWGL_G.cha:333] variante=stk_training npc(Galen_Marek)=stk_training_gear
GALEN_MAREK/INF_HUNTER [SWGL_G.cha:353] variante=galen_jedi_hunter npc(Starkiller_Dark)=stk_Dark
GRAN/ALT [SWGL_G.cha:769] variante=gran_jk npc(gran)=gran
GUNRAY/SEPARATIST [SWGL_G.cha:1135] variante=gunrayep3 npc(Nute_Gunray)=gunray
HAN_SOLO/HOTH [SWGL_H.cha:65] variante=han_hoth npc(Han_Solo)=hansolo
HAN_SOLO/RES_DEFAULT [SWGL_H.cha:85] variante=Han_TFA npc(Han_Solo)=hansolo
HAVOC/DEFAULT [SWGL_H.cha:185] variante=havoctrooper_tor npc(Republic_Trooper)=pRTrooperMale
HAVOC/ALT [SWGL_H.cha:195] variante=republictrooper npc(Republic_Trooper)=pRTrooperMale
HONDO/CW [SWGL_H.cha:457] variante=hondo npc(Hondo_Ohnaka)=hondo_r
HUMAN_F/ALT [SWGL_H.cha:557] variante=Female_Civilian_1 npc(human_female)=civf3
IDEN_VERSIO/REB_DEFAULT [SWGL_I.cha:37] variante=Iden_Rebel npc(Iden_Versio)=iden
IG88/ALT [SWGL_I.cha:101] variante=IG88 npc(IG-88)=IG-88
IG88/SHINY [SWGL_I.cha:111] variante=IG88_Shiny npc(IG-88)=IG-88
IMPOFFICER/DEFAULT [SWGL_I.cha:237] variante=imperial1 npc(impcommander)=imperial
IMPOFFICER/BLACK [SWGL_I.cha:247] variante=imperial2 npc(impcommander)=imperial
IMPOFFICER/ISB [SWGL_I.cha:257] variante=imperial3 npc(impcommander)=imperial
IMPOFFICER/GREY [SWGL_I.cha:267] variante=imperial1_grey npc(impcommander)=imperial
IMPOFFICER/PONCHO [SWGL_I.cha:277] variante=imperial_poncho npc(impcommander)=imperial
IMPOFFICER/B_PONCHO [SWGL_I.cha:287] variante=imperial_poncho2 npc(impcommander)=imperial
IMPOFFICER/I_PONCHO [SWGL_I.cha:297] variante=imperial_poncho3 npc(impcommander)=imperial
IMPOFFICER/COAT [SWGL_I.cha:307] variante=imperial_trench npc(impcommander)=imperial
IMPOFFICER/B_COAT [SWGL_I.cha:317] variante=imperial_trench2 npc(impcommander)=imperial
IMPOFFICER/I_COAT [SWGL_I.cha:327] variante=imperial_trench3 npc(impcommander)=imperial
IMPOFFICER_F/BLACK [SWGL_I.cha:355] variante=imperial_fem2 npc(impofficer_f)=imperial_fem1
IMPOFFICER_F/ISB [SWGL_I.cha:365] variante=imperial_fem3 npc(impofficer_f)=imperial_fem1
ITHORIAN/DEFAULT [SWGL_I.cha:537] variante=itho npc(Ithorian)=ithorian
JAESA/DEFAULT [SWGL_J.cha:91] variante=jaesa_dark npc(Jaesa_Willsaam)=jaesa
JAINA/DEFAULT [SWGL_J.cha:137] variante=jaina_solo npc(Jaina_Solo)=jaina
JAINA/JEDI [SWGL_J.cha:157] variante=jaina_jedi npc(Jaina_Solo)=jaina
JAINA/PILOT [SWGL_J.cha:167] variante=jaina_pilot npc(Jaina_Solo)=jaina
JAINA/ROGUE [SWGL_J.cha:177] variante=jaina_rogue npc(Jaina_Solo)=jaina
JAN/DFII [SWGL_J.cha:241] variante=DFII_Jan npc(jan)=jan
JAN/DF [SWGL_J.cha:251] variante=Jan_DF npc(jan)=jan
JAN/MERC [SWGL_J.cha:261] variante=jan_ors npc(jan)=jan
JAWA/DEFAULT [SWGL_J.cha:361] variante=jawa2 npc(jawa)=jawa
JEDI10/DEFAULT [SWGL_J.cha:665] variante=jedi_spanki1b npc(Prequel_Jedi13)=jedi_spanki
JEDI10/JO_DEFAULT [SWGL_J.cha:675] variante=jedi_moncal npc(Prequel_Jedi13)=jedi_spanki
JEDI11/DEFAULT [SWGL_J.cha:693] variante=jedi_spanki2 npc(Prequel_Jedi13)=jedi_spanki
JEDI11/JO_DEFAULT [SWGL_J.cha:703] variante=aqualish_jedi npc(Prequel_Jedi13)=jedi_spanki
JEDI12/DEFAULT [SWGL_J.cha:721] variante=khil npc(Prequel_Jedi13)=jedi_spanki
JEDI13/DEFAULT [SWGL_J.cha:739] variante=jedi_spanki2b npc(Prequel_Jedi13)=jedi_spanki
JEDI13/JO_DEFAULT [SWGL_J.cha:749] variante=abednedo npc(Prequel_Jedi13)=jedi_spanki
JEDI14/DEFAULT [SWGL_J.cha:767] variante=jedi_spanki3 npc(Prequel_Jedi13)=jedi_spanki
JEDI15/DEFAULT [SWGL_J.cha:785] variante=jedi_spanki3a npc(Prequel_Jedi13)=jedi_spanki
JEDI16/DEFAULT [SWGL_J.cha:803] variante=jedi_spanki3b npc(Prequel_Jedi13)=jedi_spanki
JEDI17/DEFAULT [SWGL_J.cha:821] variante=jedi_spanki4 npc(Prequel_Jedi13)=jedi_spanki
JEDI18/DEFAULT [SWGL_J.cha:839] variante=jedi_spanki4a npc(Prequel_Jedi13)=jedi_spanki
JEDI19/DEFAULT [SWGL_J.cha:857] variante=jedi_spanki4b npc(Prequel_Jedi13)=jedi_spanki
JEDI2/DEFAULT [SWGL_J.cha:875] variante=Jedi_GenericFemale1 npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI20/DEFAULT [SWGL_J.cha:893] variante=jedi_spanki5 npc(Prequel_Jedi13)=jedi_spanki
JEDI21/DEFAULT [SWGL_J.cha:911] variante=jedi_spanki5a npc(Prequel_Jedi13)=jedi_spanki
JEDI22/DEFAULT [SWGL_J.cha:929] variante=jedi_spanki5b npc(Prequel_Jedi13)=jedi_spanki
JEDI23/DEFAULT [SWGL_J.cha:947] variante=jedi_spanki6 npc(Prequel_Jedi13)=jedi_spanki
JEDI24/DEFAULT [SWGL_J.cha:965] variante=jedi_spanki6a npc(Prequel_Jedi13)=jedi_spanki
JEDI25/DEFAULT [SWGL_J.cha:983] variante=jedi_spanki6b npc(Prequel_Jedi13)=jedi_spanki
JEDI26/DEFAULT [SWGL_J.cha:1001] variante=jedi_stkotor_afro npc(Prequel_Jedi13)=jedi_spanki
JEDI3/JO_DEFAULT [SWGL_J.cha:1083] variante=jedi_vurk npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI33/DEFAULT [SWGL_J.cha:1155] variante=CJP2 npc(Classic_Jedi)=CJP1
JEDI34/DEFAULT [SWGL_J.cha:1173] variante=CJP3 npc(Classic_Jedi)=CJP1
JEDI35/ALT [SWGL_J.cha:1201] variante=jedi_gran npc(Gran_Jedi)=Gran_Jedi
JEDI36/DEFAULT [SWGL_J.cha:1219] variante=UJP_humanSP npc(SWTOR_Jedi8)=jedi_stkotor_elder
JEDI37/DEFAULT [SWGL_J.cha:1237] variante=UJP_quaraSP npc(aqualish_jedi)=Aqualish_Jedi
JEDI38/DEFAULT [SWGL_J.cha:1255] variante=UJP_quarrenSP npc(Prequel_Jedi28)=jedi_stkotor_aliens_1
JEDI4/DEFAULT [SWGL_J.cha:1291] variante=Jedi_GenericFemale2 npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI4/JO_DEFAULT [SWGL_J.cha:1301] variante=jedi_nikto npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI42/DEFAULT [SWGL_J.cha:1355] variante=jedi_nm npc(Nautolan_Jedi)=JediNautolan
JEDI5/DEFAULT [SWGL_J.cha:1373] variante=Jedi_GenericFemale2A npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI5/JO_DEFAULT [SWGL_J.cha:1383] variante=jedinautolan npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI6/DEFAULT [SWGL_J.cha:1401] variante=Jedi_GenericFemale3 npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI6/JO_DEFAULT [SWGL_J.cha:1411] variante=jedichistori npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI7/DEFAULT [SWGL_J.cha:1429] variante=Jedi_GenericFemale3A npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI7/JO_DEFAULT [SWGL_J.cha:1439] variante=jedi_cerean npc(Prequel_Jedi1)=Jedi_GenericFemale1A
JEDI8/DEFAULT [SWGL_J.cha:1457] variante=jedi_bothan npc(Prequel_Jedi13)=jedi_spanki
JEDI9/DEFAULT [SWGL_J.cha:1475] variante=jedi_spanki1a npc(Prequel_Jedi13)=jedi_spanki
JEDI9/JO_DEFAULT [SWGL_J.cha:1485] variante=female_jedi_knight npc(Prequel_Jedi13)=jedi_spanki
JUROKK/GUARD [SWGL_J.cha:1757] variante=jurokk_guard npc(Jurokk)=jurokk
JYN/DISGUISE [SWGL_J.cha:1785] variante=jyn_imperial npc(Jyn_Erso)=jynerso
KANAN/KNIGHT [SWGL_K.cha:289] variante=kanan_s3 npc(Kanan_Jarrus)=Kanan
KIRA/ONSLAUGHT [SWGL_K.cha:687] variante=kira npc(Kira_Carsen)=kiracarsen
KIT/MONCALA [SWGL_K.cha:751] variante=kit_cw npc(Kit_Fisto)=kit_fisto
KOTA/DRUNK [SWGL_K.cha:933] variante=kota_drunk npc(Rahm_Kota)=Kota_tfu
KOTA/REBEL [SWGL_K.cha:943] variante=kota_tfu2 npc(Rahm_Kota)=Kota_tfu
KREEL/DEFAULT [SWGL_K.cha:1077] variante=Kreel_GM npc(ST_Kreel)=Kreel
KYLE/JOURNEYMAN2 [SWGL_K.cha:1253] variante=dfii_kyle2 npc(Kyle_DF2)=DFII_Kyle
KYLE/CUSTOMIZATION [SWGL_K.cha:1273] variante=kyle_customisation npc(kyle)=kyle
KYLE/STEALTH [SWGL_K.cha:1283] variante=kyle_sjg npc(kyle)=kyle
LANA/DEFAULT [SWGL_L.cha:63] variante=lana_beniko npc(Lana_Beniko)=pLana_Beniko
LANDO_CALRISSIAN/SMUGGLER [SWGL_L.cha:111] variante=lando_han npc(Lando_Calrissian)=lando_ot
LANDO_CALRISSIAN/THRAWN [SWGL_L.cha:121] variante=lando_thrawn npc(Lando_Calrissian)=lando_ot
LEIA_ORGANA/LOTHAL [SWGL_L.cha:267] variante=leia_rbs npc(Ep4_Leia)=leia_anh
LEIA_ORGANA/MIMBAN2 [SWGL_L.cha:287] variante=leia_sotme npc(Leia_Mimban)=Leia_Mimban
LEIA_ORGANA/MANTELL [SWGL_L.cha:297] variante=leia_om npc(Ep4_Leia)=leia_anh
LEIA_ORGANA/CAPTIVE [SWGL_L.cha:327] variante=leia_6e npc(Ep5_Leia_Dress)=leia_bespin
LEIA_ORGANA/SLAVE [SWGL_L.cha:347] variante=leia_slave npc(Ep5_Leia_Dress)=leia_bespin
LEIA_ORGANA/HONOGHR [SWGL_L.cha:367] variante=leia_honoghr npc(Ep6_Leia)=Leia_Ep6
LEIA_ORGANA/DE [SWGL_L.cha:377] variante=leia_de npc(Ep6_Leia)=Leia_Ep6
LEIA_ORGANA/TRAINING [SWGL_L.cha:387] variante=leia_ep9 npc(Leia_Jedi)=Leia_Jedi
LEIA_ORGANA/INF_DEFAULT [SWGL_L.cha:407] variante=leia_tfu2 npc(Leia_Jedi)=Leia_Jedi
LUKE_SKYWALKER/REB_MIMBAN [SWGL_L.cha:637] variante=luke_sotme npc(Luke_Mimban)=luke_mim
LUKE_SKYWALKER/GM [SWGL_L.cha:747] variante=luke_eu npc(Luke)=luke
LUMIYA/SUIT [SWGL_L.cha:831] variante=lumiya_suit npc(Lumiya)=Lumiya
MALGUS/ROGUE [SWGL_M.cha:163] variante=malgus npc(Darth_Malgus)=PSDarthMalgus
MANDALORIAN/DEFAULT [SWGL_M.cha:199] variante=DeviantMando npc(Mandalorian_Male)=mbmandy
MANDALORIAN_WARRIOR/DEFAULT [SWGL_M.cha:237] variante=tormando npc(Mandalorian_Male)=mbmandy
MARA/SMUGGLER [SWGL_M.cha:293] variante=he_MaraJade npc(Mara_Jade)=mara
MARA/WINTER [SWGL_M.cha:303] variante=Mara_NJO npc(Mara_Jade)=mara
MARA/PONCHO [SWGL_M.cha:313] variante=mara_poncho npc(Mara_Jade)=mara
MARA/VONG [SWGL_M.cha:323] variante=mara_eh npc(Mara_Jade)=mara
MARINE/DEFAULT [SWGL_M.cha:341] variante=bd_marine npc(Battle_Droid)=sad_bd
MARK3_BD/DEFAULT [SWGL_M.cha:431] variante=bd_mk3 npc(Battle_Droid)=sad_bd
MAUL/SHIRTLESS [SWGL_M.cha:571] variante=maul_wots npc(Darth_Maul)=darthmaul
MOHC/DEFAULT [SWGL_M.cha:851] variante=dl_mohc npc(General_Mohc)=mohc
MUSTAFARIAN/DEFAULT [SWGL_M.cha:1061] variante=Mustafarian npc(Mustafarian)=n_mustafarian
NAHDAR/ROBE [SWGL_N.cha:117] variante=nahdar_robe npc(Nahdar_Vebb)=Nahdar
NARKINA_OFFICER/DEFAULT [SWGL_N.cha:181] variante=impnarkina npc(impofficer)=imperial
NEIMOIDIAN_GUARD/CIS_DEFAULT [SWGL_N.cha:263] variante=NeimoidianSec npc(Neimoidian_Guard)=Neimoidian_Guard
NOGHRI/ALT [SWGL_N.cha:507] variante=noghri2 npc(noghri)=noghri
NR_OFFICER/DEFAULT [SWGL_N.cha:633] variante=mots_officer npc(NR_Guard)=NewRepGuard
NR_OFFICER/ALT [SWGL_N.cha:643] variante=mots_officer2 npc(NR_Guard)=NewRepGuard
OBI_WAN_KENOBI/HERMIT [SWGL_O.cha:29] variante=obiwan_exileB npc(Obiwan_Exile)=obiwan_exile
OBI_WAN_KENOBI/JABIIM [SWGL_O.cha:39] variante=obiwan_exileC npc(Obiwan_Exile)=obiwan_exile
OBI_WAN_KENOBI/DEATH_WATCH [SWGL_O.cha:49] variante=obi_dw npc(Ep3_Obiwan)=obiwan_ep3
OBI_WAN_KENOBI/MIDWAR [SWGL_O.cha:59] variante=kenobi_tcw npc(Ep3_Obiwan)=obiwan_ep3
OBI_WAN_KENOBI/GENERAL [SWGL_O.cha:69] variante=jedi_kenobi npc(Ep3_Obiwan)=obiwan_ep3
OFFICER_GE/DEFAULT [SWGL_O.cha:135] variante=imperial_bf2 npc(impcommander)=imperial
ORGUS/DEFAULT [SWGL_O.cha:261] variante=MasterOrgusDin npc(Orgus_Din)=orgus
PADME/GREEN [SWGL_P.cha:47] variante=padme_green npc(Ep3_Padme)=Padme_purple
PADME/NIGHTGOWN [SWGL_P.cha:57] variante=padme_n-gown npc(Ep3_Padme)=Padme_purple
PADME/SNOW [SWGL_P.cha:77] variante=padme_snow npc(TCW_Padme)=padme_cw
PADME/PILOT [SWGL_P.cha:97] variante=padme_e2 npc(Ep2_Padme_Geonosis)=Padme_EpII
PLO/TCW [SWGL_P.cha:345] variante=newplo npc(Plo_Koon)=plo-koon
PLO/JPB [SWGL_P.cha:355] variante=jpb_plo npc(Plo_Koon)=plo-koon
PYKE/ALT [SWGL_P.cha:686] variante=pyke2 npc(Pyke)=pyke
PYKE/BH [SWGL_P.cha:696] variante=pyke_bh npc(Pyke)=pyke
QUARREN/ALT [SWGL_Q.cha:55] variante=quarren_soldier npc(Quarren)=quarren
QUINLAN/TCW [SWGL_Q.cha:121] variante=qvos npc(Quinlan_Vos)=st_vos
R2D2/JABBA [SWGL_R.cha:55] variante=r2-d2 npc(r2d2)=r2d2
RAKGHOUL/DEFAULT [SWGL_R.cha:191] variante=rakghoul_boss npc(Rakghoul)=Rakghoul
RANCOR/BULL [SWGL_R.cha:229] variante=bullrancor npc(rancor)=rancor
RANCOR/FELUCIAN [SWGL_R.cha:239] variante=feluciarancor npc(rancor)=rancor
RANCOR/F_BULL [SWGL_R.cha:249] variante=felucia_rancor npc(rancor)=rancor
REP_TROOPER/UNDERWORLD [SWGL_R.cha:771] variante=trooper_tor npc(Republic_Trooper)=pRTrooperMale
REVAN/GENERAL [SWGL_R.cha:827] variante=revan_c npc(Revan_Jedi)=revan
REX/ENDOR [SWGL_R.cha:965] variante=rex_endor npc(Rex_Rebel)=rex_old
ROCKET_BD/DEFAULT [SWGL_R.cha:1085] variante=bd_rocket npc(Battle_Droid_Rocket)=bd_heavy
RODIAN/ALT [SWGL_R.cha:1113] variante=rodian_alt npc(Rodian2)=rodian
RUUSAAN/DEFAULT [SWGL_R.cha:1339] variante=ruusaan_head npc(Ruusaan_Skirata)=ruusaan
SABINE/DEFAULT [SWGL_S.cha:45] variante=sabine_goh npc(Sabine_Wren)=sabine
SAESEE/CW [SWGL_S.cha:137] variante=cw_tiin npc(Saesee_Tiin)=saesee_tiin
SATELE/EXILE [SWGL_S.cha:255] variante=satele_kotet npc(satele_shan)=satele_tor
SHADOW_COLLECTIVE/DEFAULT [SWGL_S.cha:787] variante=dwmando npc(Shadow_Collective)=slk_dw
SHEEV_PALPATINE/SENATE [SWGL_S.cha:987] variante=palp_sen npc(Darth_Sidious)=lord_sidious
SHEEV_PALPATINE/APPRENTICE [SWGL_S.cha:997] variante=Palpatine_App npc(Darth_Sidious)=lord_sidious
SION/TFU [SWGL_S.cha:1317] variante=darthsion_tfu npc(Darth_Sion)=darthsion
SIRI/SUIT [SWGL_S.cha:1345] variante=siri_suit npc(Siri_Tachi)=siri
SITH_COMMANDO/DEFAULT [SWGL_S.cha:1381] variante=sith_comm npc(Sith_Trooper)=sithtrooper
SITH1/DEFAULT [SWGL_S.cha:1453] variante=aviar npc(SWTOR_Sith7)=jedi_sithspanki
SITH10/DEFAULT [SWGL_S.cha:1471] variante=jedi_sithspanki1h npc(SWTOR_Sith7)=jedi_sithspanki
SITH11/DEFAULT [SWGL_S.cha:1489] variante=jedi_sithspanki2 npc(SWTOR_Sith7)=jedi_sithspanki
SITH12/DEFAULT [SWGL_S.cha:1507] variante=jedi_sithspanki2a npc(SWTOR_Sith7)=jedi_sithspanki
SITH13/DEFAULT [SWGL_S.cha:1525] variante=jedi_sithspanki2b npc(SWTOR_Sith7)=jedi_sithspanki
SITH14/DEFAULT [SWGL_S.cha:1543] variante=jedi_sithspanki2c npc(SWTOR_Sith7)=jedi_sithspanki
SITH15/DEFAULT [SWGL_S.cha:1561] variante=jedi_sithspanki2d npc(SWTOR_Sith7)=jedi_sithspanki
SITH16/DEFAULT [SWGL_S.cha:1579] variante=jedi_sithspanki2e npc(SWTOR_Sith7)=jedi_sithspanki
SITH17/DEFAULT [SWGL_S.cha:1597] variante=jedi_sithspanki2f npc(SWTOR_Sith7)=jedi_sithspanki
SITH18/DEFAULT [SWGL_S.cha:1615] variante=jedi_sithspanki2g npc(SWTOR_Sith7)=jedi_sithspanki
SITH19/DEFAULT [SWGL_S.cha:1633] variante=jedi_sithspanki2h npc(SWTOR_Sith7)=jedi_sithspanki
SITH20/DEFAULT [SWGL_S.cha:1651] variante=jedi_sithspanki3 npc(SWTOR_Sith7)=jedi_sithspanki
SITH21/DEFAULT [SWGL_S.cha:1669] variante=jedi_sithspanki3a npc(SWTOR_Sith7)=jedi_sithspanki
SITH22/DEFAULT [SWGL_S.cha:1687] variante=jedi_sithspanki3b npc(SWTOR_Sith7)=jedi_sithspanki
SITH23/DEFAULT [SWGL_S.cha:1705] variante=jedi_sithspanki3c npc(SWTOR_Sith7)=jedi_sithspanki
SITH24/DEFAULT [SWGL_S.cha:1723] variante=jedi_sithspanki3d npc(SWTOR_Sith7)=jedi_sithspanki
SITH25/DEFAULT [SWGL_S.cha:1741] variante=jedi_sithspanki3e npc(SWTOR_Sith7)=jedi_sithspanki
SITH26/DEFAULT [SWGL_S.cha:1759] variante=jedi_sithspanki3f npc(SWTOR_Sith7)=jedi_sithspanki
SITH27/DEFAULT [SWGL_S.cha:1777] variante=jedi_sithspanki3g npc(SWTOR_Sith7)=jedi_sithspanki
SITH28/DEFAULT [SWGL_S.cha:1795] variante=jedi_sithspanki3h npc(SWTOR_Sith7)=jedi_sithspanki
SITH3/DEFAULT [SWGL_S.cha:1831] variante=jedi_sithspanki1a npc(SWTOR_Sith7)=jedi_sithspanki
SITH4/DEFAULT [SWGL_S.cha:1849] variante=jedi_sithspanki1b npc(SWTOR_Sith7)=jedi_sithspanki
SITH5/DEFAULT [SWGL_S.cha:1867] variante=jedi_sithspanki1c npc(SWTOR_Sith7)=jedi_sithspanki
SITH6/DEFAULT [SWGL_S.cha:1885] variante=jedi_sithspanki1d npc(SWTOR_Sith7)=jedi_sithspanki
SITH7/DEFAULT [SWGL_S.cha:1903] variante=jedi_sithspanki1e npc(SWTOR_Sith7)=jedi_sithspanki
SITH8/DEFAULT [SWGL_S.cha:1921] variante=jedi_sithspanki1f npc(SWTOR_Sith7)=jedi_sithspanki
SITH9/DEFAULT [SWGL_S.cha:1939] variante=jedi_sithspanki1g npc(SWTOR_Sith7)=jedi_sithspanki
SNOWTROOPER/ALT [SWGL_S.cha:2093] variante=snowtrooper2 npc(snowtrooper)=snowtrooper
STORMTROOPER/ALT [SWGL_S.cha:2321] variante=stormie npc(stormtrooper)=stormtrooper
TALON/DEFAULT [SWGL_T.cha:135] variante=talon_goh npc(Darth_Talon)=darth_talon
TENEBRAE/PSYCHE [SWGL_T.cha:471] variante=vitiate_new npc(Vitiate)=vitiate
TIE_PILOT/DEFAULT [SWGL_T.cha:743] variante=TIEPilot npc(Stormpilot)=stormpilot
TOR_JEDI6/DEFAULT [SWGL_T.cha:1017] variante=derrik npc(KOTOR_Jedi2)=kotorjedi_male
TRANDOSHAN/TOR [SWGL_T.cha:1225] variante=trando_tor npc(Trandoshan)=trandoshan
TUSKEN/DEFAULT [SWGL_T.cha:1415] variante=tusken2 npc(tusken)=tusken
TUSKEN/TOR [SWGL_T.cha:1435] variante=tor_tusken npc(tusken)=tusken
TWILEK/ALT [SWGL_T.cha:1481] variante=jedi_tm npc(Twi'lek_Male)=twilek_male
VALERIA/LOTUS [SWGL_V.cha:19] variante=valeria_l npc(Valeria)=valeria
VANGUARD/ALT [SWGL_V.cha:101] variante=rebel_vanguard npc(Rebel_Vanguard)=rebelvanguard
VENTRESS/YAVIN [SWGL_V.cha:201] variante=asajj03 npc(Asajj_Ventress)=AssajjCW
VENTRESS/NIGHTSISTER [SWGL_V.cha:211] variante=Asajj_NS npc(Asajj_Ventress)=AssajjCW
VENTRESS/DISCIPLE [SWGL_V.cha:231] variante=asajjmask npc(Asajj_Ventress_Disciple)=Ventress
VETTE/KOTFE [SWGL_V.cha:295] variante=vette_fe npc(Vette)=vette
VROOK/DEFAULT [SWGL_V.cha:493] variante=vrook_kotor npc(vrook_lamar)=vrook
WAMPA/SWAMP [SWGL_W.cha:37] variante=swampwampa npc(wampa)=wampa
WARRIOR/PUREBLOOD [SWGL_W.cha:93] variante=Sith_Pureblood_TOR npc(Sith_Juggernaut)=sith_apprentice
XALEK/LORD [SWGL_X.cha:85] variante=xalek_lord npc(Xalek)=xalek
ZAHRA/ARMOR [SWGL_Z.cha:37] variante=zahra2 npc(Ellian_Zahra)=zahra
ZANN_DEKA/DEFAULT [SWGL_Z.cha:73] variante=droideka3 npc(Destroyer_Droid)=droideka
```

### Divers

- `Z_custom_chars.pk3` (`arbiter.cha`) et `Z_custom_odst.pk3` (`odst.cha`) ont leurs propres `.cha` et cfg (`inf_*`) : non touchés par la migration.
- `SWGL_Map_Eurocorp_Citadel.pk3` n'est pas un zip valide (sans rapport avec les persos).
- `strings/English/chardesctse.str` et `chardesccis.str` sont en double dans SWGL_Menu.pk3.
- Les `_def.cfg` migrés sont **identiques** à ceux d'origine pour les 1411 variantes appariées (seul le nom de fichier change).
- Tags : les 22 tags sont tous traduits dans `tag_swgl.str`; toutes les factions ont un nom dans `swgl_factions.str` et existent dans `swgl.fac`.