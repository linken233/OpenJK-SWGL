# Variantes dont le `.npc` ne correspond pas au modèle affiché

Généré le 2026-10-23. Le menu affiche `model` de la variante, mais le NPC spawné utilise le `playerModel` du `.npc` (le `ui_npc_type` du `_def.cfg`). Quand ils diffèrent, le joueur voit un modèle dans le menu et obtient un autre à l'écran.

Même constat dans SWGL_Characters : les `_def.cfg` sont identiques. Un `.npc` partagé entre plusieurs variantes de modèles différents est la cause principale.

`Candidats` = autres `.npc` dont le `playerModel` est exactement le modèle de la variante.

**136 NPC concernés, 261 variantes en décalage** (sur 1159 NPC utilisés).

| NPC | playerModel du NPC | Variante (perso/variante) | Modèle de la variante | Candidats (même modèle) |
|---|---|---|---|---|
| `212th_Jet` | `clonetrooper_ab` | `212TH/AIRBORNE2` | `cbe` | `212th_grenadier` |
| `327th_Trooper` | `clone_rgb` | `327TH/DEFAULT` | `327th_p1` | `327th_trooper_phase1` |
| `41st_Trooper` | `cloneultima` | `41ST/PHASEI` | `41stcorps` | `clone_buzz`, `clone_draa`, `commander_gree_phase1` |
| `Adi_Gallia` | `Adi_Gallia` | `ADI/TCW` | `adi_gallia_cw` | `adi_gallia_tcw` |
| `Ahsoka_Tano` | `AhsokaV3` | `AHSOKA_TANO/PADAWAN` | `ahsoka_totj` | — |
| `Ahsoka_Tano_Rebels` | `ahsoka_rebels` | `AHSOKA_TANO/NOMAD` | `Ahsoka_Mando` | — |
| `Ahsoka_Tano_Siege` | `ahsoka_S7` | `AHSOKA_TANO/INF_DEFAULT` | `Ahsoka_Gen` | — |
| `alien_merc` | `alien_mercs` | `ALIEN_MERC/GRAN` | `gran2` | — |
| `alien_merc` | `alien_mercs` | `ALIEN_MERC/NIKTO` | `nikto` | — |
| `aqualish_jedi` | `Aqualish_Jedi` | `JEDI37/DEFAULT` | `UJP_quaraSP` | — |
| `ARC_Trooper` | `arc` | `ARC/ALT` | `cloneultarck` | `arc_trooper2` |
| `Asajj_Ventress` | `AssajjCW` | `VENTRESS/YAVIN` | `asajj03` | — |
| `Asajj_Ventress` | `AssajjCW` | `VENTRESS/NIGHTSISTER` | `Asajj_NS` | — |
| `Asajj_Ventress_Disciple` | `Ventress` | `VENTRESS/DISCIPLE` | `asajjmask` | — |
| `Atris_dark` | `Atris` | `ATRIS/TRAYA` | `atris_sith` | — |
| `Battle_Droid` | `sad_bd` | `BATTLE_DROID/RC` | `bd_rc` | — |
| `Battle_Droid` | `sad_bd` | `BATTLEDROID/DEFAULT` | `bd_concept` | — |
| `Battle_Droid` | `sad_bd` | `FIREFIGHTER/DEFAULT` | `bd_firefighter` | — |
| `Battle_Droid` | `sad_bd` | `MARINE/DEFAULT` | `bd_marine` | — |
| `Battle_Droid` | `sad_bd` | `MARK3_BD/DEFAULT` | `bd_mk3` | — |
| `Battle_Droid_Rocket` | `bd_heavy` | `ROCKET_BD/DEFAULT` | `bd_rocket` | — |
| `Ben_Skywalker` | `benskywalker` | `BEN_SKYWALKER/TUNIC` | `ben_tunic` | — |
| `Bo-Katan` | `bokatan` | `BO_KATAN/DEFAULT` | `bokatan_mnd` | — |
| `boba_fett` | `boba_fett` | `BOBA/DEFAULT` | `bobafett` | — |
| `Brutus` | `burg` | `BRUTUS/DEFAULT` | `brutus` | — |
| `Classic_Jedi` | `CJP1` | `JEDI33/DEFAULT` | `CJP2` | — |
| `Classic_Jedi` | `CJP1` | `JEDI34/DEFAULT` | `CJP3` | — |
| `Clone_Pilot` | `clonepilot` | `CLONE_PILOT/DEFAULT` | `cp` | `clone_pilot_midwar` |
| `Corran_Horn` | `HM_CorranHorn` | `CORRAN/PILOT` | `Corran` | — |
| `Count_Dooku` | `dooku` | `DOOKU/RITUAL` | `dr_dooku` | — |
| `Crosshair` | `BB_Crosshair` | `CROSSHAIR/REP_DEFAULT` | `pcws7_crosshair` | — |
| `Dant_Militia` | `dant_off` | `DANT_MIL/ALT` | `dant_sniper` | — |
| `Darth_Bane` | `darth_bane` | `BANE/DEFAULT` | `DarthBane` | — |
| `Darth_Bane` | `darth_bane` | `BANE/SPIRIT` | `bane_cw` | — |
| `Darth_Malgus` | `PSDarthMalgus` | `MALGUS/ROGUE` | `malgus` | — |
| `Darth_Maul` | `darthmaul` | `MAUL/SHIRTLESS` | `maul_wots` | — |
| `Darth_Sidious` | `lord_sidious` | `SHEEV_PALPATINE/SENATE` | `palp_sen` | — |
| `Darth_Sidious` | `lord_sidious` | `SHEEV_PALPATINE/APPRENTICE` | `Palpatine_App` | — |
| `Darth_Sion` | `darthsion` | `SION/TFU` | `darthsion_tfu` | — |
| `Darth_Talon` | `darth_talon` | `TALON/DEFAULT` | `talon_goh` | — |
| `Darth_Vader` | `am_vader` | `ANAKIN_SKYWALKER/ALT` | `vader` | — |
| `Darth_Vader` | `am_vader` | `ANAKIN_SKYWALKER/ALT2` | `vader2` | — |
| `Depa_Billaba` | `depabillaba` | `DEPA/DEFAULT` | `depa_skirt` | — |
| `Destroyer_Droid` | `droideka` | `ZANN_DEKA/DEFAULT` | `droideka3` | — |
| `Devaronian` | `devaronian` | `DEVARONIAN/ALT` | `dev` | `devaronian_kotor` |
| `Duros` | `Duros` | `DUROS/ALT` | `duros2` | — |
| `Ellian_Zahra` | `zahra` | `ZAHRA/ARMOR` | `zahra2` | `ellian_zahra_armor` |
| `Ep2_Anakin` | `anakin_ep2` | `ANAKIN_SKYWALKER/COMMANDER` | `ani_comm` | — |
| `Ep2_Padme_Geonosis` | `Padme_EpII` | `PADME/PILOT` | `padme_e2` | — |
| `Ep3_Anakin` | `anakin` | `ANAKIN_SKYWALKER/GENERAL` | `anakin_totj` | — |
| `Ep3_Anakin` | `anakin` | `ANAKIN_SKYWALKER/MIDWAR` | `anakin_totj2` | — |
| `Ep3_Anakin` | `anakin` | `ANAKIN_SKYWALKER/NELVAAN` | `ani_nlvn` | — |
| `Ep3_Obiwan` | `obiwan_ep3` | `OBI_WAN_KENOBI/DEATH_WATCH` | `obi_dw` | — |
| `Ep3_Obiwan` | `obiwan_ep3` | `OBI_WAN_KENOBI/MIDWAR` | `kenobi_tcw` | — |
| `Ep3_Obiwan` | `obiwan_ep3` | `OBI_WAN_KENOBI/GENERAL` | `jedi_kenobi` | — |
| `Ep3_Padme` | `Padme_purple` | `PADME/GREEN` | `padme_green` | — |
| `Ep3_Padme` | `Padme_purple` | `PADME/NIGHTGOWN` | `padme_n-gown` | — |
| `Ep4_Leia` | `leia_anh` | `LEIA_ORGANA/LOTHAL` | `leia_rbs` | — |
| `Ep4_Leia` | `leia_anh` | `LEIA_ORGANA/MANTELL` | `leia_om` | — |
| `Ep5_Leia_Dress` | `leia_bespin` | `LEIA_ORGANA/CAPTIVE` | `leia_6e` | — |
| `Ep5_Leia_Dress` | `leia_bespin` | `LEIA_ORGANA/SLAVE` | `leia_slave` | — |
| `Ep6_Leia` | `Leia_Ep6` | `LEIA_ORGANA/HONOGHR` | `leia_honoghr` | — |
| `Ep6_Leia` | `Leia_Ep6` | `LEIA_ORGANA/DE` | `leia_de` | — |
| `Even_Piell` | `EvenPiell` | `EVEN/TCW` | `evenpiellcw` | — |
| `Galen_Marek` | `stk_training_gear` | `GALEN_MAREK/ASSASSIN` | `stk_sith_assassin` | — |
| `Galen_Marek` | `stk_training_gear` | `GALEN_MAREK/HEAVY` | `stk_htraining_gear` | — |
| `Galen_Marek` | `stk_training_gear` | `GALEN_MAREK/LIGHT` | `stk_ltraining_gear` | — |
| `Galen_Marek` | `stk_training_gear` | `GALEN_MAREK/EARLY` | `stk_training` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/MEDICAL` | `stk_medic_robes` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/HUNTER` | `stk_bounty` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/CORELLIAN` | `stk_corellian_jkt` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/TEMPLE` | `stk_temple_eg` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/JUNGLE` | `stk_jungle_cg` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/INDUSTRIAL` | `stk_industrial` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/GENERAL` | `stk_general` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/HCG` | `stk_hooded_cg` | — |
| `Galen_Marek_Adventurer` | `stk_adventurer` | `GALEN_MAREK/UTILITY` | `stk_utility_gear` | — |
| `General_Mohc` | `mohc` | `MOHC/DEFAULT` | `dl_mohc` | — |
| `gran` | `gran` | `GRAN/ALT` | `gran_jk` | — |
| `Gran_Jedi` | `Gran_Jedi` | `JEDI35/ALT` | `jedi_gran` | — |
| `Han_Solo` | `hansolo` | `HAN_SOLO/HOTH` | `han_hoth` | — |
| `Han_Solo` | `hansolo` | `HAN_SOLO/RES_DEFAULT` | `Han_TFA` | `ep7_han_solo` |
| `Hondo_Ohnaka` | `hondo_r` | `HONDO/CW` | `hondo` | — |
| `human_female` | `civf3` | `HUMAN_F/ALT` | `Female_Civilian_1` | — |
| `Iden_Versio` | `iden` | `IDEN_VERSIO/REB_DEFAULT` | `Iden_Rebel` | `iden_rebel` |
| `IG-88` | `IG-88` | `IG88/ALT` | `IG88` | — |
| `IG-88` | `IG-88` | `IG88/SHINY` | `IG88_Shiny` | — |
| `impcommander` | `imperial` | `IMPOFFICER/DEFAULT` | `imperial1` | — |
| `impcommander` | `imperial` | `IMPOFFICER/BLACK` | `imperial2` | — |
| `impcommander` | `imperial` | `IMPOFFICER/ISB` | `imperial3` | — |
| `impcommander` | `imperial` | `IMPOFFICER/GREY` | `imperial1_grey` | — |
| `impcommander` | `imperial` | `IMPOFFICER/PONCHO` | `imperial_poncho` | — |
| `impcommander` | `imperial` | `IMPOFFICER/B_PONCHO` | `imperial_poncho2` | — |
| `impcommander` | `imperial` | `IMPOFFICER/I_PONCHO` | `imperial_poncho3` | — |
| `impcommander` | `imperial` | `IMPOFFICER/COAT` | `imperial_trench` | — |
| `impcommander` | `imperial` | `IMPOFFICER/B_COAT` | `imperial_trench2` | — |
| `impcommander` | `imperial` | `IMPOFFICER/I_COAT` | `imperial_trench3` | — |
| `impcommander` | `imperial` | `OFFICER_GE/DEFAULT` | `imperial_bf2` | `impofficer_armor` |
| `impofficer` | `imperial` | `NARKINA_OFFICER/DEFAULT` | `impnarkina` | — |
| `impofficer_f` | `imperial_fem1` | `IMPOFFICER_F/BLACK` | `imperial_fem2` | — |
| `impofficer_f` | `imperial_fem1` | `IMPOFFICER_F/ISB` | `imperial_fem3` | — |
| `Ithorian` | `ithorian` | `ITHORIAN/DEFAULT` | `itho` | — |
| `Jaesa_Willsaam` | `jaesa` | `JAESA/DEFAULT` | `jaesa_dark` | `jaesa_willsaam_sith` |
| `Jaina_Solo` | `jaina` | `JAINA/DEFAULT` | `jaina_solo` | — |
| `Jaina_Solo` | `jaina` | `JAINA/JEDI` | `jaina_jedi` | — |
| `Jaina_Solo` | `jaina` | `JAINA/PILOT` | `jaina_pilot` | — |
| `Jaina_Solo` | `jaina` | `JAINA/ROGUE` | `jaina_rogue` | — |
| `jan` | `jan` | `JAN/DFII` | `DFII_Jan` | — |
| `jan` | `jan` | `JAN/DF` | `Jan_DF` | — |
| `jan` | `jan` | `JAN/MERC` | `jan_ors` | — |
| `jawa` | `jawa` | `JAWA/DEFAULT` | `jawa2` | — |
| `Jurokk` | `jurokk` | `JUROKK/GUARD` | `jurokk_guard` | `jurokk_guard` |
| `Jyn_Erso` | `jynerso` | `JYN/DISGUISE` | `jyn_imperial` | — |
| `Kanan_Jarrus` | `Kanan` | `KANAN/KNIGHT` | `kanan_s3` | — |
| `Kira_Carsen` | `kiracarsen` | `KIRA/ONSLAUGHT` | `kira` | — |
| `Kit_Fisto` | `kit_fisto` | `KIT/MONCALA` | `kit_cw` | — |
| `KOTOR_Jedi2` | `kotorjedi_male` | `TOR_JEDI6/DEFAULT` | `derrik` | — |
| `kyle` | `kyle` | `KYLE/CUSTOMIZATION` | `kyle_customisation` | — |
| `kyle` | `kyle` | `KYLE/STEALTH` | `kyle_sjg` | — |
| `Kyle_DF2` | `DFII_Kyle` | `KYLE/JOURNEYMAN2` | `dfii_kyle2` | — |
| `Lana_Beniko` | `pLana_Beniko` | `LANA/DEFAULT` | `lana_beniko` | — |
| `Lando_Calrissian` | `lando_ot` | `LANDO_CALRISSIAN/SMUGGLER` | `lando_han` | — |
| `Lando_Calrissian` | `lando_ot` | `LANDO_CALRISSIAN/THRAWN` | `lando_thrawn` | — |
| `Leia_Jedi` | `Leia_Jedi` | `LEIA_ORGANA/TRAINING` | `leia_ep9` | — |
| `Leia_Jedi` | `Leia_Jedi` | `LEIA_ORGANA/INF_DEFAULT` | `leia_tfu2` | — |
| `Leia_Mimban` | `Leia_Mimban` | `LEIA_ORGANA/MIMBAN2` | `leia_sotme` | — |
| `Lord_Starkiller` | `lord_stk` | `GALEN_MAREK/TATOOINE` | `lord_stk_tat` | — |
| `Lord_Starkiller` | `lord_stk` | `GALEN_MAREK/STALKER` | `sithstalker` | — |
| `Lord_Starkiller` | `lord_stk` | `GALEN_MAREK/CYBERNETIC` | `cyber_recon` | — |
| `Luke` | `luke` | `LUKE_SKYWALKER/GM` | `luke_eu` | — |
| `Luke_Mimban` | `luke_mim` | `LUKE_SKYWALKER/REB_MIMBAN` | `luke_sotme` | — |
| `Lumiya` | `Lumiya` | `LUMIYA/SUIT` | `lumiya_suit` | — |
| `Mandalorian_Male` | `mbmandy` | `MANDALORIAN/DEFAULT` | `DeviantMando` | `mandalorian` |
| `Mandalorian_Male` | `mbmandy` | `MANDALORIAN_WARRIOR/DEFAULT` | `tormando` | `chorn_mandalorian` |
| `Mara_Jade` | `mara` | `MARA/SMUGGLER` | `he_MaraJade` | — |
| `Mara_Jade` | `mara` | `MARA/WINTER` | `Mara_NJO` | — |
| `Mara_Jade` | `mara` | `MARA/PONCHO` | `mara_poncho` | — |
| `Mara_Jade` | `mara` | `MARA/VONG` | `mara_eh` | — |
| `Mustafarian` | `n_mustafarian` | `MUSTAFARIAN/DEFAULT` | `Mustafarian` | — |
| `Nahdar_Vebb` | `Nahdar` | `NAHDAR/ROBE` | `nahdar_robe` | — |
| `Nautolan_Jedi` | `JediNautolan` | `JEDI42/DEFAULT` | `jedi_nm` | — |
| `Neimoidian_Guard` | `Neimoidian_Guard` | `NEIMOIDIAN_GUARD/CIS_DEFAULT` | `NeimoidianSec` | — |
| `noghri` | `noghri` | `NOGHRI/ALT` | `noghri2` | `noghri_berserker` |
| `NR_Guard` | `NewRepGuard` | `NR_OFFICER/DEFAULT` | `mots_officer` | — |
| `NR_Guard` | `NewRepGuard` | `NR_OFFICER/ALT` | `mots_officer2` | — |
| `Nute_Gunray` | `gunray` | `GUNRAY/SEPARATIST` | `gunrayep3` | — |
| `Obiwan_Exile` | `obiwan_exile` | `OBI_WAN_KENOBI/HERMIT` | `obiwan_exileB` | — |
| `Obiwan_Exile` | `obiwan_exile` | `OBI_WAN_KENOBI/JABIIM` | `obiwan_exileC` | — |
| `Plo_Koon` | `plo-koon` | `PLO/TCW` | `newplo` | — |
| `Plo_Koon` | `plo-koon` | `PLO/JPB` | `jpb_plo` | — |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI2/DEFAULT` | `Jedi_GenericFemale1` | `swtor_jedi21`, `swtor_jedi22`, `swtor_jedi23` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI3/JO_DEFAULT` | `jedi_vurk` | `vurk_jedi` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI4/DEFAULT` | `Jedi_GenericFemale2` | `swtor_jedi29`, `swtor_jedi30` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI4/JO_DEFAULT` | `jedi_nikto` | `nikto_jedi` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI5/DEFAULT` | `Jedi_GenericFemale2A` | `prequel_jedi12` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI5/JO_DEFAULT` | `jedinautolan` | `nautolan_jedi` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI6/DEFAULT` | `Jedi_GenericFemale3` | `prequel_jedi7`, `prequel_jedi9`, `prequel_jedi10` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI6/JO_DEFAULT` | `jedichistori` | `chistori_jedi` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI7/DEFAULT` | `Jedi_GenericFemale3A` | `swtor_sith1`, `swtor_sith2`, `swtor_sith3` |
| `Prequel_Jedi1` | `Jedi_GenericFemale1A` | `JEDI7/JO_DEFAULT` | `jedi_cerean` | `cerean_jedi` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI10/DEFAULT` | `jedi_spanki1b` | `prequel_jedi15` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI10/JO_DEFAULT` | `jedi_moncal` | `moncal_jedi` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI11/DEFAULT` | `jedi_spanki2` | `swtor_jedi17`, `swtor_jedi18`, `swtor_jedi19` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI11/JO_DEFAULT` | `aqualish_jedi` | `aqualish_jedi` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI12/DEFAULT` | `khil` | `khil_jedi` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI13/DEFAULT` | `jedi_spanki2b` | `swtor_jedi14`, `prequel_jedi18`, `prequel_jedi36` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI13/JO_DEFAULT` | `abednedo` | `abednedo` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI14/DEFAULT` | `jedi_spanki3` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI15/DEFAULT` | `jedi_spanki3a` | `prequel_jedi20` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI16/DEFAULT` | `jedi_spanki3b` | `prequel_jedi19` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI17/DEFAULT` | `jedi_spanki4` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI18/DEFAULT` | `jedi_spanki4a` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI19/DEFAULT` | `jedi_spanki4b` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI20/DEFAULT` | `jedi_spanki5` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI21/DEFAULT` | `jedi_spanki5a` | `swtor_jedi12`, `swtor_jedi13` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI22/DEFAULT` | `jedi_spanki5b` | `swtor_jedi11`, `prequel_jedi37` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI23/DEFAULT` | `jedi_spanki6` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI24/DEFAULT` | `jedi_spanki6a` | `swtor_sith26`, `swtor_sith27`, `swtor_sith28` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI25/DEFAULT` | `jedi_spanki6b` | `prequel_jedi21`, `prequel_jedi27` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI26/DEFAULT` | `jedi_stkotor_afro` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI8/DEFAULT` | `jedi_bothan` | `bothan_jedi` |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI9/DEFAULT` | `jedi_spanki1a` | — |
| `Prequel_Jedi13` | `jedi_spanki` | `JEDI9/JO_DEFAULT` | `female_jedi_knight` | `female_jedi_knight` |
| `Prequel_Jedi28` | `jedi_stkotor_aliens_1` | `JEDI38/DEFAULT` | `UJP_quarrenSP` | — |
| `Pyke` | `pyke` | `PYKE/ALT` | `pyke2` | — |
| `Pyke` | `pyke` | `PYKE/BH` | `pyke_bh` | — |
| `Quarren` | `quarren` | `QUARREN/ALT` | `quarren_soldier` | — |
| `Quinlan_Vos` | `st_vos` | `QUINLAN/TCW` | `qvos` | — |
| `r2d2` | `r2d2` | `R2D2/JABBA` | `r2-d2` | — |
| `Rahm_Kota` | `Kota_tfu` | `KOTA/DRUNK` | `kota_drunk` | — |
| `Rahm_Kota` | `Kota_tfu` | `KOTA/REBEL` | `kota_tfu2` | — |
| `Rakghoul` | `Rakghoul` | `RAKGHOUL/DEFAULT` | `rakghoul_boss` | — |
| `rancor` | `rancor` | `RANCOR/BULL` | `bullrancor` | `jof_bullrancor` |
| `rancor` | `rancor` | `RANCOR/FELUCIAN` | `feluciarancor` | — |
| `rancor` | `rancor` | `RANCOR/F_BULL` | `felucia_rancor` | — |
| `Rebel_Vanguard` | `rebelvanguard` | `VANGUARD/ALT` | `rebel_vanguard` | — |
| `Republic_Trooper` | `pRTrooperMale` | `HAVOC/DEFAULT` | `havoctrooper_tor` | — |
| `Republic_Trooper` | `pRTrooperMale` | `HAVOC/ALT` | `republictrooper` | `havoc_major`, `havoc_trooper` |
| `Republic_Trooper` | `pRTrooperMale` | `REP_TROOPER/UNDERWORLD` | `trooper_tor` | — |
| `Revan_Jedi` | `revan` | `REVAN/GENERAL` | `revan_c` | — |
| `Rex_Rebel` | `rex_old` | `REX/ENDOR` | `rex_endor` | `rex_endor` |
| `Rodian2` | `rodian` | `RODIAN/ALT` | `rodian_alt` | `rodian_alt` |
| `Ruusaan_Skirata` | `ruusaan` | `RUUSAAN/DEFAULT` | `ruusaan_head` | — |
| `Sabine_Wren` | `sabine` | `SABINE/DEFAULT` | `sabine_goh` | — |
| `Saesee_Tiin` | `saesee_tiin` | `SAESEE/CW` | `cw_tiin` | — |
| `satele_shan` | `satele_tor` | `SATELE/EXILE` | `satele_kotet` | — |
| `Shadow_Collective` | `slk_dw` | `SHADOW_COLLECTIVE/DEFAULT` | `dwmando` | — |
| `Siri_Tachi` | `siri` | `SIRI/SUIT` | `siri_suit` | — |
| `Sith_Juggernaut` | `sith_apprentice` | `WARRIOR/PUREBLOOD` | `Sith_Pureblood_TOR` | `sith_juggernaut2` |
| `Sith_Officer` | `sithofficer` | `COMMANDER/DEFAULT` | `sithcommander` | — |
| `Sith_Trooper` | `sithtrooper` | `SITH_COMMANDO/DEFAULT` | `sith_comm` | — |
| `snowtrooper` | `snowtrooper` | `SNOWTROOPER/ALT` | `snowtrooper2` | — |
| `ST_Kreel` | `Kreel` | `KREEL/DEFAULT` | `Kreel_GM` | `kreel_gm` |
| `Starkiller` | `Starkiller_Clone` | `GALEN_MAREK/TIE` | `starkiller_tie` | — |
| `Starkiller_Arena` | `Starkiller_Arena` | `GALEN_MAREK/DREAM` | `starkiller_dago` | — |
| `Starkiller_Arena` | `Starkiller_Arena` | `GALEN_MAREK/JEDI` | `starkiller_armor` | — |
| `Starkiller_Arena` | `Starkiller_Arena` | `GALEN_MAREK/ARMOR` | `starkiller_jedi` | — |
| `Starkiller_Dark` | `stk_Dark` | `GALEN_MAREK/INF_HUNTER` | `galen_jedi_hunter` | — |
| `Stormpilot` | `stormpilot` | `TIE_PILOT/DEFAULT` | `TIEPilot` | — |
| `stormtrooper` | `stormtrooper` | `STORMTROOPER/ALT` | `stormie` | — |
| `SWTOR_Jedi8` | `jedi_stkotor_elder` | `JEDI36/DEFAULT` | `UJP_humanSP` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH1/DEFAULT` | `aviar` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH10/DEFAULT` | `jedi_sithspanki1h` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH11/DEFAULT` | `jedi_sithspanki2` | `swtor_sith20` |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH12/DEFAULT` | `jedi_sithspanki2a` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH13/DEFAULT` | `jedi_sithspanki2b` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH14/DEFAULT` | `jedi_sithspanki2c` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH15/DEFAULT` | `jedi_sithspanki2d` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH16/DEFAULT` | `jedi_sithspanki2e` | `swtor_sith21` |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH17/DEFAULT` | `jedi_sithspanki2f` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH18/DEFAULT` | `jedi_sithspanki2g` | `swtor_sith18` |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH19/DEFAULT` | `jedi_sithspanki2h` | `swtor_sith23` |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH20/DEFAULT` | `jedi_sithspanki3` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH21/DEFAULT` | `jedi_sithspanki3a` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH22/DEFAULT` | `jedi_sithspanki3b` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH23/DEFAULT` | `jedi_sithspanki3c` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH24/DEFAULT` | `jedi_sithspanki3d` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH25/DEFAULT` | `jedi_sithspanki3e` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH26/DEFAULT` | `jedi_sithspanki3f` | `swtor_sith15`, `swtor_sith16`, `swtor_sith17` |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH27/DEFAULT` | `jedi_sithspanki3g` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH28/DEFAULT` | `jedi_sithspanki3h` | `swtor_sith10`, `swtor_sith11`, `swtor_sith12` |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH3/DEFAULT` | `jedi_sithspanki1a` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH4/DEFAULT` | `jedi_sithspanki1b` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH5/DEFAULT` | `jedi_sithspanki1c` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH6/DEFAULT` | `jedi_sithspanki1d` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH7/DEFAULT` | `jedi_sithspanki1e` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH8/DEFAULT` | `jedi_sithspanki1f` | — |
| `SWTOR_Sith7` | `jedi_sithspanki` | `SITH9/DEFAULT` | `jedi_sithspanki1g` | — |
| `TCW_Padme` | `padme_cw` | `PADME/SNOW` | `padme_snow` | — |
| `The_Mandalorian` | `dindjarin` | `DIN/NOBESKAR` | `the_mando` | — |
| `Trandoshan` | `trandoshan` | `TRANDOSHAN/TOR` | `trando_tor` | — |
| `tusken` | `tusken` | `TUSKEN/DEFAULT` | `tusken2` | — |
| `tusken` | `tusken` | `TUSKEN/TOR` | `tor_tusken` | — |
| `Twi'lek_Male` | `twilek_male` | `TWILEK/ALT` | `jedi_tm` | — |
| `Vader_Infinities` | `vader_infinities` | `ANAKIN_SKYWALKER/INF_ALT` | `anakin_suit` | — |
| `Valeria` | `valeria` | `VALERIA/LOTUS` | `valeria_l` | — |
| `Vette` | `vette` | `VETTE/KOTFE` | `vette_fe` | — |
| `Vitiate` | `vitiate` | `TENEBRAE/PSYCHE` | `vitiate_new` | — |
| `vrook_lamar` | `vrook` | `VROOK/DEFAULT` | `vrook_kotor` | — |
| `wampa` | `wampa` | `WAMPA/SWAMP` | `swampwampa` | — |
| `Xalek` | `xalek` | `XALEK/LORD` | `xalek_lord` | — |

## Cas particulier : DEATHTROOPER et ZOMBIE

Deux persos portent le nom « Death Trooper » :

| | DEATHTROOPER | ZOMBIE |
|---|---|---|
| Fichier | `SWGL_D.cha:273` | `SWGL_Z.cha:209` |
| Nom | Death Trooper | Death Trooper |
| Faction | GE (Empire) | CRTRS (créatures) |
| Modèle | `deathtrooper` | `Death_Trooper` |
| NPC | `Death_Trooper` (modèle `Deathtrooper`) | `Undead_Stormtrooper` (modèle `Death_Trooper`) |
| Équipe / PV (cfg) | enemy / 150 | free / 40 |
| Description | `CHARDESCEMPIRE_DEATHTROOPER` | `CHARDESCOTHER_ZOMBIE` |

Le vrai Death Trooper est `DEATHTROOPER`. `ZOMBIE` est un stormtrooper mort-vivant (NPC `Undead_Stormtrooper`, créature) : son nom devrait être « Undead Stormtrooper » ou « Zombie Trooper » pour ne pas se confondre dans la liste.
