# Analyse de SWGL_Characters.pk3

Source : `sandbox/game/SWGL/SWGL_Characters.pk3` (42 fichiers `.cha`, 1 093 entrées), bios tirées de `SWGL_Menu.pk3` → `strings/English/CharDesc*.str`.
Version consultable : [characters-factions.html](characters-factions.html) (page autonome, s'ouvre par double-clic).
Détail complet par personnage (identité, factions suggérées, tags, justification) : [characters-factions-tags.csv](characters-factions-tags.csv).

Constat de départ : chaque entrée a **un seul** code dans `factions` et toutes ont `tags none`. Le champ s'appelle `factions` (pluriel) : s'il accepte plusieurs codes, la plupart des « doublons » ci-dessous peuvent fusionner en une seule entrée multi-factions.

---

## 1. Personnages en double dans plusieurs factions

### 1a. Même personne, entrées séparées

| Personnage | Entrées | Remarque |
|---|---|---|
| Anakin Skywalker / Vader | `REP_ANAKIN`, `GE_VADER` (+ `INF_ANAKIN_INFINITIES`, `INF_VADER_INFINITIES`) | `REP_ANAKIN` contient déjà les variantes « Lord Vader » et « Anakin (Redeemed) ×2 », qui recouvrent GE_VADER et INF_VADER_INFINITIES. |
| Palpatine / Sidious | `REP_PALPATINE`, `GE_EMPEROR_PALPATINE`, `SE_ETERNAL_PALPATINE` (+ `INF_PALPATINE_INFINITIES`) | REP a une variante « Darth Sidious » ; GE a « Darth Sidious (Sith Apprentice) » et **deux variantes identiques** (`lord_sidious/model_default`). |
| Maul | `ROT_DARTH_MAUL`, `CD_MAUL` (+ `INF_MAUL_OW`, `INF_MAUL_INFINITIES`) | `CD_MAUL` « Maul (Reborn) » et `INF_MAUL_OW` = **même modèle et même skin** (`cybernetic_maul`). |
| Galen Marek / Starkiller | `GE_GALEN_IMP`, `REB_GALEN_REBEL`, `REB_STARKILLER_REBEL` (+ 3 INF) | Doublon **à l'intérieur de REB** (Galen et Starkiller = même personne/clone). |
| Revan | `DRSE_REVAN`, `TOR_JKREVAN`, `REV_REVAN_REBORN` | Fusion possible avec des variantes Jedi/Sith/Reborn. |
| Tenebrae / Vitiate / Valkorion | `ASE_TENEBRAE`, `RSE_VITIATE`, `EE_VALKORION` | Même être, trois noms. |
| A'Sharad Hett / Darth Krayt | `REP_ASHARAD`, `DKGE_KRAYT` | |
| Jacen Solo / Darth Caedus | `NR_JACEN`, `SO_CAEDUS` | |
| Ahsoka Tano | `REP_AHSOKA`, `REB_AHSOKA_REBEL` (+ 2 INF) | |
| Obi-Wan Kenobi | `REP_KENOBI`, `REB_BEN` | REP_KENOBI contient déjà « Hermit » et « Exile ». |
| Rex | `REP_REX`, `REB_REX_REBEL` | |
| Luke Skywalker | `REB_LUKE`, `RES_LUKE_ST` | REB_LUKE contient déjà « Grand Master », « Jedi Master ». |
| Leia Organa | `REB_LEIA`, `RES_LEIA_ST` (+ `INF_LEIA_INFINITIES`) | REB_LEIA contient déjà « Leia Organa Solo » ×3 → même nom que RES_LEIA_ST. |
| Han Solo | `REB_HAN`, `RES_HAN_ST` | |
| Lando Calrissian | `REB_LANDO`, `RES_LANDO_ST` | |
| Mon Mothma | `REB_MONMOTHMA`, `REP_MOTHMA` | |
| Iden Versio | `GE_IDEN`, `REB_IDEN` | Bio identique. |
| Crosshair | `GE_CROSSHAIR`, `REP_CROSSHAIR` | Bio identique. |
| Wullf Yularen | `REP_YULAREN`, `GE_YULAREN_IMP` | |
| X1 | `REP_X1`, `IR_X1_EMPIRE` | |
| X2 | `REP_X2`, `NR_X2` | Bio identique. |
| Krix (Trandoshan) | `BS_KRIX` « Krix Swiftshadow », `CRIM_KRIX` « Krix » | **Même clé de bio** (`CHARDESCCRIMINALS_KRIX`) → vrai doublon. |
| Qui-Gon Jinn | `REP_QUI_GON` (+ `INF_QUIGON_INFINITIES`) | |
| Jar Jar | `CV_JARJAR` (+ `INF_DARTH_JARJAR`) | |

Les entrées **INF** (Infinities) sont des « what-if » volontaires. Je les liste pour info ; ce ne sont pas des erreurs.

### 1b. Assets strictement identiques (même modèle et même skin)

- `JO_JEDI8` = `REP_Prequel_Jedi` (`jedi_spanki`)
- `JO_JEDI12` = `JO_SWTOR_Jedi` (`jedi_spanki2a`)
- `NR_JEDI_NR2` = `NR_jedi_random` (`jedi_hf`)
- `SO_SITH2` = `SO_SWTOR_Sith` (`jedi_sithspanki`)
- `TOR_TOR_JEDI3` = `TOR_KOTOR_Jedi` (`kotorjedi_twilekf`)
- `EE_ARCANN` « Prince Arcann » = `EE_THEXAN` (`thexan`) : jumeaux, peut-être voulu
- `GE_EMPEROR_PALPATINE` : deux variantes « Emperor Palpatine » identiques

### 1c. Homonymes génériques (pas des doublons, mais ambigus dans un menu)

« Stormtrooper » (GE/FO), « TIE Pilot » (GE/FO), « Praetorian Guard » (FO/IR), « Coruscant Guard » (REP/GE), « Sith Trooper » (DRSE/DKGE/SE), « Imperial Officer » (GE ×2 : `GE_IMPOFFICER` **et** `GE_OFFICER` dans la même faction, + DKGE), « Shadow Trooper » (GE/IR), « Death Trooper » (GE et le zombie CRTRS), « Wookiee Warrior » (CV/REB), « Old Republic Jedi » (JO/TOR).

### 1d. Atris « Darth Traya »

`TOR_ATRIS` a une variante nommée « Darth Traya », alors que `DRSE_TRAYA` (Kreia) porte déjà ce titre. Au mieux, ça prête à confusion : à renommer « Atris (Fallen) » seulement.

---

## 2. Personnages qui devraient être dans plus d'une faction

Liste triée et corrigée (j'ai écarté ou corrigé environ 60 propositions erronées des agents). La liste complète, avec justifications, est dans la colonne `extra` du CSV.

### Changement de camp majeur
- **Anakin/Vader** : REP + GE + JO + ROT
- **Palpatine** : REP + GE + ROT + SE
- **Dooku** : CIS + JO + ROT
- **Maul** : ROT + CD + DW/MANDO (Shadow Collective, Mandalore)
- **Revan** : TOR + DRSE + JO + REV
- **Galen Marek** : GE + REB (+ SO)
- **Kylo Ren / Ben Solo** : FO + JO (+ SO)
- **Jacen Solo** : NR + GA + SO
- **Darth Krayt** : DKGE + JO
- **Ventress** : CIS + BH + NS
- **Savage Opress** : CD + NS
- **Kallus**, **Iden Versio**, **Juno Eclipse**, **Bodhi Rook**, **Crix Madine**, **Fenn Rau** : GE + REB
- **Finn** : RES + FO
- **Sora Bulq**, **Pong Krell** : REP/JO + CIS (traîtres séparatistes)
- **Lord Scourge** : TOR + RSE (Emperor's Wrath avant de rejoindre les Jedi)
- **Bastila**, **Atton Rand**, **Visas Marr** : TOR + DRSE
- **Jaesa Willsaam**, **Ashara Zavros** : RSE + JO
- **Lana Beniko**, **Vette**, **Kaliyo** : EA + RSE ; **Senya Tirall**, **Koth Vortena** : EA + EE ; **Theron Shan** : EA + TOR
- **Mara Jade** : NR + GE (Emperor's Hand) + CRIM (Karrde)
- **Tahiri Veila**, **Alema Rar** : GA + SO
- **Zekk** : NR + IR (Shadow Academy) + GA
- **Taron Malicos**, **Maris Brood**, **Dagan Gera** (+ HR), **Trilla/Second Sister**, **Grand Inquisitor** : + JO (ex-Jedi)
- **Snoke** : FO + SE (strandcast du Sith Eternal)

### Continuité d'une ère à l'autre
- **Luke**, **Leia**, **Han**, **Lando**, **Chewbacca**, **Ackbar**, **Nien Nunb**, **C-3PO**, **R2-D2** : REB + NR + RES
- **Mon Mothma**, **Garm Bel Iblis** : REP + REB + NR
- **Bail** et **Breha Organa**, **Cham Syndulla** : REP + REB
- **Rex**, **Obi-Wan**, **Ahsoka**, **Rahm Kota**, **Kanan** : REP + REB (+ JO)
- **Clones** (Cody, Fox, Appo, Thire, Crosshair, 501st, Clone Trooper…) : REP + GE
- **Tarkin**, **Yularen**, **Mas Amedda**, **Panaka** : REP + GE
- **Pellaeon**, **Daala**, **Isard** : GE + IR (+ GA/NR)
- **Wedge**, **Kyle Katarn**, **Jan Ors**, **Cara Dune** : REB + NR
- **Jagged Fel** : FE + GA ; **Morrigan Corde** : DKGE → plutôt FE

### Mandaloriens et chasseurs
- **Jango**, **Boba Fett**, **Din Djarin**, **Sabine Wren**, **Mirta Gev**, **Torian Cadera**, **Gar Saxon**, **Imperial Supercommando** : + MANDO / BH
- **Bo-Katan**, **Pre Vizsla** : MANDO + DW
- **Canderous Ordo** : MNC + MANDO + TOR
- **Shae Vizla** : MANDO + RSE + BH
- **Clan Skirata** : tout le clan + REP (commandos/Null ARC) + MANDO
- **Tarre Vizsla** : MANDO + JO

### Criminalité
- **Han**, **Lando**, **Dash Rendar**, **Talon Karrde**, **Mirax Terrik**, **Niles Ferrier**, **Zorii Bliss** (+ RES), **Cad Bane** : + CRIM
- **Krrsantan**, **Greedo**, **Droopy McCool** : + HC
- **Evazan** et **Ponda Baba** : rangés en HC, mais ce sont des indépendants → CRIM
- **Aphra**, **0-0-0**, **BT-1** : CRIM + GE (agents de Vader)
- **Enfys Nest** : CRIM + REB

### Cas systématique : JO et SO
Environ 150 Jedi de REP/TOR/NR/GA et environ 60 Sith de RSE/DRSE/IR mériteraient JO ou SO. Plutôt que d'ajouter JO/SO à 200 entrées (ce qui viderait ces factions de leur sens), **je recommande d'utiliser les tags `jedi`/`sith`** et de réserver JO/SO aux personnages sans attache politique.

---

## 3. Proposition de liste de tags

Vocabulaire fermé, à combiner librement. Les comptes viennent de l'application test sur les 1 009 entrées nommées et génériques.

| Axe | Tags | Usage |
|---|---|---|
| Type | `named` (700), `generic` (309) | filtrer les héros / la piétaille, sélection aléatoire |
| Canon | `canon` (514), `legends` (454), `noncanon` (36) | |
| Ère | `era_ancient` (33), `era_old_republic` (180), `era_high_republic` (3), `era_prequel` (374), `era_imperial` (333), `era_new_republic` (112), `era_sequel` (45), `era_legacy` (29) | indépendant des factions : un perso peut couvrir plusieurs ères |
| Force | `jedi` (152), `sith` (116), `dark_jedi` (26), `force_user` (41), `fallen_jedi` (48), `redeemed` (23), `non_sensitive` (660) | `non_sensitive` = aucun autre tag de Force |
| Genre | `male` (494), `female` (176) | |

Tags à envisager ensuite, à dériver automatiquement des `characters_conf/*.cfg` plutôt qu'à la main : `saber`, `dual_saber`, `saberstaff`, `blaster`, `heavy_weapon`, `melee` (d'après `ui_weaponOne`/`ui_saber`), et `boss` (d'après `ui_health` élevé).

---

## 4. Problèmes de données relevés au passage

1. **296 bios jamais affichées → corrigé.** Les `.cha` utilisaient `CHARDESCREBELLION_*` et `CHARDESCREPUBLIC_*`, alors que les fichiers s'appelaient `CharDescREB.str` / `CharDescREP.str`. Le moteur préfixe chaque clé par le nom du fichier en majuscules ([stringed_ingame.cpp:257](../code/qcommon/stringed_ingame.cpp:257)). Les fichiers ont été renommés en `CharDescREBELLION.str` / `CharDescREPUBLIC.str` dans `GameData/SWGL/SWGL_Menu.pk3`, en anglais et en français. Le doublon identique de `CharDescREP.str` a été supprimé et une sauvegarde est dans `SWGL_Menu.pk3.bak`. Résultat : 996 clés sur 1 105 résolues au lieu de 700, sans aucune clé cassée. Pas encore vu en jeu.
2. **Clés de bio absentes** : CIS_HEAVY, CIS_ENGINEER, CIS_BUZZ, DRSE_OFFICER, GA_TRA, CRIM_ALIEN_MERC, CRIM_Brutus, CV_Falleen, HC_Nizuc, REB_Jaxxon, NR_jedi_random, tous les `JO_JEDIn`, `SO_SITHn`, `NR_JEDI_NRn`.
3. **Bios copiées-collées d'un autre personnage** : REP_AQUA (décrit les droïdes aquatiques), REP_ALDERAAN (décrit les scientifiques), DRSE_OFFICER (décrit les officiers impériaux), DRSE_WARRIOR_MALAK = RSE_WARRIOR, GE_IMPGUNNER = REP_GUNNER, CIS_HEAVY = textes mandaloriens.
4. Le champ `random` est vide sur 10 entrées (au lieu de `true`/`false`).

## Fiabilité
Les identités et les factions des personnages connus sont solides. Wookieepedia a renvoyé une erreur HTTP 402 aux agents : les vérifications en ligne sont passées par des miroirs (archivum, etc.) et seulement pour les personnages obscurs. Les tags `canon`/`legends` et `media` des personnages mineurs restent à relire. Le CSV indique « unverified » dans la justification quand c'est le cas.
