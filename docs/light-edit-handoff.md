# Handoff : light edit (2026-10-11)

Branche `feature/vulkan-rtx`. Référence : `docs/light-edit.md` (à jour jusqu'à l'API 6). Rien n'est commité. Rien n'a été lancé en jeu depuis le début de cette série : tout est vérifié par compilation seulement.

## Fait (non commité, compilé en Debug et Release)
1. **Menu** : trois pages (Lights, Sky and sun, Emissive), champs nom et cônes, suivi de la sélection du jeu (`ledit_sel`, `ledit_emi_sel`). Syntaxe `cvarTest` / `showCvar` non vue en jeu.
2. **Émissifs par shader** : le multiplicateur règle aussi le glow visible (code d'échelle d'émission des matériaux, pas de 4 %).
3. **`ledit_compact` / `ledit_restore`** (API 5) : une suppression ne change rien ; le compactage rend les slots, la lumière disparaît ; le mode restore les réaffiche.
4. **Émissifs par surface BSP** (API 6) : un groupe par face, `source emissive_surface` dans le `.lgt`, désactivation, facteur, teinte, style, muet, solo ; compactage et restauration des slots émissifs.
5. **Règles de test commentées** dans `code/rd-vulkan/rtx/vk_rtx_bsp.cpp` (verre forcé par `glass`, `botton`, `door02`). Pas compilé après ce changement seul ; les builds des points 3 et 4 le contiennent. Les règles `glass` / `Shadow` de `vk_rtx_material.cpp` (`RB_IsTransparent`, `RB_SkipObject`) sont gardées.
6. Cases `weapon 10` à `13` : le message « tool 13 is not in this version » vient d'un bind. **Non corrigé** (voir ci-dessous).

## Fait aussi : sky_diffuse et plancher ambiant (API 7)
`sky_diffuse` (0 à 1, ciel couvert) et le plancher de luminosité par map (`ambient_color`, `ambient_intensity`, cvar `pt_ambient_floor_scale`) sont implémentés, compilés en Debug et Release, commités et poussés. Les blobs SPIR-V correspondants sont dans `shader_data_rtx.c`, commité avec le travail NRD. Valeurs de calibrage (tangente 0,6 du disque, plancher 0,1 sur sin(élévation), unité du plancher) à régler à l'œil.

## À faire
1. **Corriger le message « tool 13 »** : ignorer en silence `weapon 10` à `13` pendant le mode (`LE_SetTool` dans `code/cgame/cg_lightedit.cpp`, appelé depuis le traitement de `weapon`). L'utilisateur n'a pas encore dit non au choix « ignorer en silence ».
3. **Passe de test en jeu** (demander l'accord de l'utilisateur avant de lancer le jeu ; instance `SWGL_CLAUDE`, voir la mémoire `reference_swgl_claude_test_instance.md`) : étapes 30 à 54 de `docs/light-edit.md` §12, plus celles du sky/ambient. Points à surveiller :
   - compactage avec déplacement de slots émissifs, restauration, relecture du `.lgt` ;
   - ReSTIR après un tassement de slots (perte d'historique une ou deux images) ;
   - coût par image de `Get` sur quelques milliers de surfaces ;
   - menu : défilement vers la ligne suivie, texte de stats trop long (70 caractères dans 308 px), syntaxe `cvarTest` ;
   - aspect d'un multiplicateur sur un néon et sur un écran.
4. **Risques connus à traiter si confirmés** :
   - `ledit_reload` ou `lightedit 0 discard` : échec silencieux de la réallocation de slots d'une lumière compactée si la réserve est pleine ;
   - un groupe émissif de plus de 1024 lumières dans un cluster ne se restaure jamais ;
   - un solo éteint aussi toutes les surfaces émissives ;
   - un `ui/lightedit.menu` externe n'a pas les nouvelles pages ni le filtre « Emissive ».
5. **Commit** quand l'utilisateur le demande (ne rien commiter avant). Le tree contient aussi du travail NRD sans rapport (`vk_rtx_nrd.cpp`, `nrd_prepare.comp`, rgen, `tr_init.cpp`) : à ne pas mélanger dans le commit du light edit.

## Rappels de méthode (préférences de l'utilisateur)
- Ne jamais lancer le jeu sans demander ; tests en Release (asserts coupés), Debug seulement pour compiler ; builder Debug ET Release.
- Fichiers CRLF : pas de `sed -i` ni `perl -pi`, utiliser l'outil Edit.
- Commentaires : une ou deux lignes, Simplified Technical English.
- Une fonctionnalité nouvelle : proposer un plan puis attendre un « go » explicite ; un problème signalé se corrige sans demander.
- Un agent neuf par itération ; ne pas reprendre un ancien agent pour du travail nouveau.
- Déléguer à un agent Sonnet plutôt que d'éditer soi-même, sauf correction triviale.
