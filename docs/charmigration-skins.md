# Skins disponibles pour les modèles sans `model_default.skin`

Généré le 2026-10-23. Pour chaque variante dont le `.cha` demande `skin model_default` alors que `models/players/<modèle>/model_default.skin` n'existe dans aucun pk3 : fichiers `.skin` réellement présents, et pk3 qui les contient.

Un modèle sans aucun `.skin` (MARK1, MARK2) n'est pas forcément cassé : le `.glm` utilise alors ses shaders par défaut. Les modèles à skins par partie (`head_*`, `torso_*`, `lower_*`) se composent par tirage ou par choix dans le menu.

## `TCWArcs`

- Variantes : `ARC_HAVOC/DEFAULT` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_REPUBLIC.pk3
- Skins (3) :
  - model (3) : `model_blitz`, `model_colt`, `model_havoc`
- `ARC_HAVOC/DEFAULT` : NPC `Clone_Havoc`

## `imperial_trench2`

- Variantes : `IMPOFFICER/B_COAT` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_EMPIRE.pk3
- Skins (61) :
  - head (17) : `head_a1`, `head_a2`, `head_a3`, `head_a4`, `head_a5`, `head_a6`, `head_b1`, `head_b2`, `head_b3`, `head_b4`, `head_b5`, `head_b6`, `head_c1`, `head_c2`, `head_c3`, `head_c4`, `head_c5`
  - torso (28) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_a4`, `torso_a5`, `torso_b1`, `torso_b2`, `torso_b3`, `torso_b4`, `torso_b5`, `torso_b6`, `torso_c1`, `torso_c2`, `torso_c3`, `torso_c4`, `torso_c5`, `torso_c6`, `torso_c7`, `torso_c8`, `torso_d1`, `torso_d2`, `torso_e1`, `torso_e2`, `torso_e3`, `torso_e4`, `torso_e5`, `torso_e6`, `torso_g1`
  - lower (16) : `lower_b1111`, `lower_b1112`, `lower_b1121`, `lower_b1122`, `lower_b1211`, `lower_b1212`, `lower_b1221`, `lower_b1222`, `lower_b2111`, `lower_b2112`, `lower_b2121`, `lower_b2122`, `lower_b2211`, `lower_b2212`, `lower_b2221`, `lower_b2222`
- `IMPOFFICER/B_COAT` : NPC `impcommander`

## `imperial_trench3`

- Variantes : `IMPOFFICER/I_COAT` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_EMPIRE.pk3
- Skins (61) :
  - head (17) : `head_a1`, `head_a2`, `head_a3`, `head_a4`, `head_a5`, `head_a6`, `head_b1`, `head_b2`, `head_b3`, `head_b4`, `head_b5`, `head_b6`, `head_c1`, `head_c2`, `head_c3`, `head_c4`, `head_c5`
  - torso (28) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_a4`, `torso_a5`, `torso_b1`, `torso_b2`, `torso_b3`, `torso_b4`, `torso_b5`, `torso_b6`, `torso_c1`, `torso_c2`, `torso_c3`, `torso_c4`, `torso_c5`, `torso_c6`, `torso_c7`, `torso_c8`, `torso_d1`, `torso_d2`, `torso_e1`, `torso_e2`, `torso_e3`, `torso_e4`, `torso_e5`, `torso_e6`, `torso_g1`
  - lower (16) : `lower_c1111`, `lower_c1112`, `lower_c1121`, `lower_c1122`, `lower_c1211`, `lower_c1212`, `lower_c1221`, `lower_c1222`, `lower_c2111`, `lower_c2112`, `lower_c2121`, `lower_c2122`, `lower_c2211`, `lower_c2212`, `lower_c2221`, `lower_c2222`
- `IMPOFFICER/I_COAT` : NPC `impcommander`

## `youngmale`

- Variantes : `JEDI_YOUNGLING/DEFAULT` (skin demandé `model_default`, `skinRandom` = None), `JEDI_YOUNGLING/RANDOM` (skin demandé `model_default`, `skinRandom` = true)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_JEDI_SITH.pk3
- Skins (30) :
  - model (1) : `model_sors`
  - head (20) : `head_a1`, `head_a2`, `head_b1`, `head_b2`, `head_c1`, `head_c2`, `head_d1`, `head_d2`, `head_e1`, `head_e2`, `head_f1`, `head_f2`, `head_g1`, `head_g2`, `head_h1`, `head_h2`, `head_i1`, `head_i2`, `head_j1`, `head_j2`
  - torso (8) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_b1`, `torso_b2`, `torso_c1`, `torso_d1`, `torso_e1`
  - lower (1) : `lower_a1`
- `JEDI_YOUNGLING/DEFAULT` : NPC `Jedi_Youngling1`
- `JEDI_YOUNGLING/RANDOM` : NPC `None`

## `youngfemale`

- Variantes : `JEDI_YOUNGLING/FEMALE` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_JEDI_SITH.pk3
- Skins (15) :
  - head (6) : `head_a1`, `head_a2`, `head_b1`, `head_b2`, `head_c1`, `head_c2`
  - torso (8) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_b1`, `torso_b2`, `torso_c1`, `torso_d1`, `torso_e1`
  - lower (1) : `lower_a1`
- `JEDI_YOUNGLING/FEMALE` : NPC `Jedi_Youngling2`

## `alienyounglings1`

- Variantes : `JEDI_YOUNGLING/ALIEN1` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_JEDI_SITH.pk3
- Skins (21) :
  - head (12) : `head_a1`, `head_a2`, `head_a3`, `head_b1`, `head_b2`, `head_b3`, `head_c1`, `head_c2`, `head_d1`, `head_d2`, `head_d3`, `head_d4`
  - torso (8) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_b1`, `torso_b2`, `torso_c1`, `torso_d1`, `torso_e1`
  - lower (1) : `lower_a1`
- `JEDI_YOUNGLING/ALIEN1` : NPC `Jedi_Youngling4`

## `alienyounglings2`

- Variantes : `JEDI_YOUNGLING/ALIEN2` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_JEDI_SITH.pk3
- Skins (22) :
  - head (13) : `head_a1`, `head_a2`, `head_a3`, `head_a4`, `head_a5`, `head_a6`, `head_a7`, `head_b1`, `head_b2`, `head_c1`, `head_c2`, `head_d1`, `head_d2`
  - torso (8) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_b1`, `torso_b2`, `torso_c1`, `torso_d1`, `torso_e1`
  - lower (1) : `lower_a1`
- `JEDI_YOUNGLING/ALIEN2` : NPC `Jedi_Youngling5`

## `jedi_chewy`

- Variantes : `JEDI41/DEFAULT` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_JEDI_SITH.pk3
- Skins (125) :
  - head (17) : `head_a1`, `head_a2`, `head_b1`, `head_b2`, `head_c1`, `head_c2`, `head_d1`, `head_d2`, `head_e1`, `head_e2`, `head_f1`, `head_f2`, `head_g1`, `head_g2`, `head_g3`, `head_h1`, `head_h2`
  - torso (99) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_a4`, `torso_a5`, `torso_b1`, `torso_b2`, `torso_b3`, `torso_b4`, `torso_b5`, `torso_c1`, `torso_c2`, `torso_c3`, `torso_c4`, `torso_c5`, `torso_d1`, `torso_d2`, `torso_d3`, `torso_d4`, `torso_d5`, `torso_e1`, `torso_e2`, `torso_e3`, `torso_e4`, `torso_e5`, `torso_f1`, `torso_f2`, `torso_f3`, `torso_f4`, `torso_f5`, `torso_f6`, `torso_g1`, `torso_g2`, `torso_ha1`, `torso_ha2`, `torso_ha3`, `torso_ha4`, `torso_ha5`, `torso_hb1`, `torso_hb2`, `torso_hb3`, `torso_hb4`, `torso_hb5`, `torso_hc1`, `torso_hc2`, `torso_hc3`, `torso_hc4`, `torso_hc5`, `torso_hd1`, `torso_hd2`, `torso_hd3`, `torso_hd4`, `torso_hd5`, `torso_he1`, `torso_he2`, `torso_he3`, `torso_he4`, `torso_he5`, `torso_hf1`, `torso_hf2`, `torso_hf3`, `torso_hf4`, `torso_hf5`, `torso_hg1`, `torso_hg2`, `torso_hg3`, `torso_ia1`, `torso_ia2`, `torso_ia3`, `torso_ia4`, `torso_ia5`, `torso_ib1`, `torso_ib2`, `torso_ib3`, `torso_ib4`, `torso_ib5`, `torso_ic1`, `torso_ic2`, `torso_ic3`, `torso_ic4`, `torso_ic5`, `torso_id1`, `torso_id2`, `torso_id3`, `torso_id4`, `torso_id5`, `torso_ie1`, `torso_ie2`, `torso_ie3`, `torso_ie4`, `torso_ie5`, `torso_if1`, `torso_if2`, `torso_if3`, `torso_if4`, `torso_if5`, `torso_ig1`, `torso_ig2`, `torso_igi3`
  - lower (9) : `lower_a1`, `lower_b1`, `lower_c1`, `lower_d1`, `lower_e1`, `lower_f1`, `lower_g1`, `lower_h1`, `lower_k1`
- `JEDI41/DEFAULT` : NPC `Wookiee_Jedi`

## `mark1`

- Variantes : `MARK1/DEFAULT` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `mark1.gla`, `model.glm`
- pk3 : 0_JKO_Assets0.pk3
- Skins : **aucun fichier `.skin`**
- `MARK1/DEFAULT` : NPC `mark1`

## `mark2`

- Variantes : `MARK2/DEFAULT` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `mark2.gla`, `model.glm`
- pk3 : 0_JKO_Assets0.pk3
- Skins : **aucun fichier `.skin`**
- `MARK2/DEFAULT` : NPC `mark2`

## `TIEReserve`

- Variantes : `TIE_PILOT/RESERVE` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_EMPIRE.pk3
- Skins (13) :
  - head (4) : `head_c1`, `head_c2`, `head_c3`, `head_c4`
  - torso (5) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_a4`, `torso_a5`
  - lower (4) : `lower_a1`, `lower_a2`, `lower_a3`, `lower_a4`
- `TIE_PILOT/RESERVE` : NPC `TIE_Pilot_Nohelm`

## `TIEReserveFem`

- Variantes : `TIE_PILOT/F_RESERVE` (skin demandé `model_default`, `skinRandom` = None)
- Fichiers modèle : `model.glm`
- pk3 : SWGL_Skins_EMPIRE.pk3
- Skins (13) :
  - head (4) : `head_c1`, `head_c2`, `head_c3`, `head_c4`
  - torso (5) : `torso_a1`, `torso_a2`, `torso_a3`, `torso_a4`, `torso_a5`
  - lower (4) : `lower_a1`, `lower_a2`, `lower_a3`, `lower_a4`
- `TIE_PILOT/F_RESERVE` : NPC `TIE_P_NH_F`
