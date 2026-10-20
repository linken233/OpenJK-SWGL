# Audit du renderer Vulkan SP, partie raster

Branche `feature/vulkan`, 2026-10-09. Audit en lecture seule.

- Cible : `code/rd-vulkan` (rdsp-vulkan), hors `code/rd-vulkan/rtx/`.
- Référence : `code/rd-vanilla` (comportement SP de Jedi Academy). `code/rd-rend2` sert de seconde référence.
- Méthode : comparaison fonction par fonction, diff des listes de fonctions, des mots-clés de shader, des cvars, des commandes console et des champs `refexport_t`. Recherche des classes de bugs connues du projet.
- Confiance : **confirmé** = les deux versions sont lues et l'écart est visible dans le code. **probable** = l'écart est lu, mais l'effet dépend des assets ou du chemin d'exécution.

Les corrections déjà notées en mémoire (ghoul2[0], reType, data.glm, GetAnimationCFG, Hunk tag, pool d'images, statics de G2_TransformBone, cache de modèles, gla cinématique, électricité, ombres stencil, texCoords transposés de tr_surface, Z_Free(NULL), fenêtre au changement de renderer) ne figurent pas ici.

## État des corrections

Branche `feature/vulkan-raster`. Les points marqués ✅ sont traités et validés en jeu (`rdsp-vulkan`, `r_rtx 0`, Release).

3.2, 3.7 et les normales faces arrière du G-buffer sont testés en sandbox sur yavin_swamp, avec le renderer de HEAD comme référence. Le zoom passe par les jumelles, pas par la lunette du disruptor. Aucune map n'a la clé `linFogStart`.

Légende : ✅ fait, ❌ sans objet ou non prévu, ⏳ plus tard.

| # | Correction | Commit |
|---|---|---|
| 1.1 ✅ | `C_LevelLoadEnd` remet `gbAlreadyDoingLoad` à faux. | `8cfb5f79d` |
| 1.3, 1.8 | Le tas du renderer est libéré à chaque map (`h_low` en TAG_HUNKALLOC). Le hook `R_ClearStuffToStopGhoul2CrashingThings` remplace le stub et garde un état valide entre `Hunk_Clear` et `R_Init`. | `2b78e37aa`, `fa6d14dda` |
| 1.4 ✅ | Le marqueur `srfSprites_t` est statique. Il passe l'index de cubemap 0, ce qui corrige aussi un crash sur yavin_swamp et yavin_canyon. | `bbf4b5c38`, `9467a5e35` |
| 2.1 ✅ | `RB_SurfaceOrientedQuad` dérive ses axes de la normale, comme vanilla. | `046c59df4` |
| 1.6 ✅ | `COutside::Cache` écrit et relit `maps/<map>.vkweather` (nom distinct de celui de vanilla). L'en-tête porte un magic, le checksum de la map et la taille de cellule ; chaque zone porte ses dimensions et ses extents. Un fichier de vanilla, d'une autre grille, tronqué ou abîmé est ignoré et la grille est recalculée. | `65d146543` |
| 1.7 ✅ (partiel) | `devmapall` et `devmapmdl` vident `s_animationCFGs`. Non fait : `bAllowScreenDissolve` reste ignoré (pas de dissolve) et les lightmaps ne sont pas supprimés (déjà détruits à chaque map). `ri.CM_DeleteCachedMap` sur `devmapbsp` et `devmapall` reste absent. | `314e5df99` |
| 1.10 ✅ | `r_fogDistance`, `r_fogColor`, `r_reloadfonts`, `imagecacheinfo` et `modelcacheinfo` sont enregistrées. Le brouillard met aussi à jour `fog->color[]`, que l'uniforme et la couleur d'effacement lisent. `imagecacheinfo` liste `tr.images` sans niveau d'usage (rd-vulkan n'en garde pas). `modelcacheinfo` liste `CModelCache`. | `6044072e2` |
| 2.2 ✅ | `RB_SurfaceCylinder` lit le rayon de fin dans `e->backlerp`, replie vers `RB_SurfaceCone` quand un bout est presque fermé, et divise la distance par 2048 (40 segments), comme vanilla. | `0a508ece9` |
| 2.3 ✅ | `RE_AddPolyToScene` préfère un volume qui contient tout le poly, puis un volume partiel identique à celui de la caméra (`R_FogParmsMatch`). Ajoute `tr.refdef.fogIndex` et `R_SetViewFogIndex`, appelé par `R_RenderView`. Les trois fonctions `*FogNum` du point 3.1 ne l’utilisent pas encore. | `3017c3abe` |
| 2.4 ✅ | `R_AddMD3Surfaces` plafonne `frame` et `oldframe` pour `RF_CAP_FRAMES`. | `0b8d6eb8e` |
| 2.5 ✅ | `R_MarkFragments` pose toujours les marques sur `SF_TRIANGLES` et rejette les triangles de dos par leur normale de face, comme vanilla. La cvar `r_marksOnTriangleMeshes` n’est plus lue. | `7c923a957` |
| 2.6 ✅ | `RB_CalcDisintegrateColors` reprend le paramètre `rgbGen` ; avec `CGEN_LIGHTING_DIFFUSE_ENTITY`, le bord de désintégration est teinté par `shaderRGBA`. | `ada4eefef` |
| 2.8 ✅ | `RB_SurfaceBeam` choisit rouge, vert ou bleu selon `skinNum`. | `9a562e50e` |
| 2.11 ✅ | `RE_StretchPic`, `RE_RotatePic` et `RE_RotatePic2` testent `tr.registered`. | `d489de349` |
| 3.1 ✅ | `R_FogNumForSphere` choisit le volume de brouillard des sprites, de ghoul2 et des MD3 comme vanilla. | `1db6c6b7a` |
| 3.2 ✅ | Brouillard linéaire : `linFogStart` lu dans `R_LoadEntities`, `SetRangedFog` restaure l'ancienne valeur, `vk_update_fog_constants` envoie `linearStart`/`linearEnd` au global et au goggles, et `CalcFog` des shaders de brouillard fait le linéaire quand `linearEnd > 0`. En jeu : le brouillard s'allège avec les jumelles et revient au dézoom. | `ca113d91f` |
| 3.3 ✅ | `distanceCull` par défaut à 12000. | `f7ff752d4` |
| 3.4 ✅, 3.5 ✅, 3.6 ✅ | Météo : lot vidé à `SHADER_MAX_VERTEXES`, orientation par vitesse, filtre NEAREST, coupure pendant les cinématiques, 50 zones météo, 12 zones de vent, cellule de 32 (magic du cache `WVK2`). | `9fd167c0e` |
| 3.7 ✅ | UBO de brouillard porté à 64 volumes (`VK_MAX_FOGS`, 3088 octets) ; le GLSL borne l'index avec `fogs.length()` et le C++ borne `num_fogs`. En jeu : le brouillard global de yavin_swamp (volume 20) s'affiche ; HEAD ne l'affiche pas. | `ca113d91f` |
| 3.8 ✅ | Plan lointain du frustum à `distanceCull*1.02`. | `f3b7191a6` |
| 4.1 ✅ | `R_SetupEntityLighting` ajoute +96 d'ambiant avec `RF_MORELIGHT` (sinon +32) ; bloc `RF_MINLIGHT` mort retiré. | `76705f647` |
| 4.2 ✅ | `r_dlightScale` par défaut à 1.0. | `140bca05d` |
| 4.3 ✅ | `RE_AddLinearLightToScene` lit `r_dlightSaturation`. | `046c4cd46` |
| 4.4 ❌ | `r_debugStyle` non ajoutée : seul l'éclairage des entités réagissait, pas les lightmaps du monde. | — |
| 5.1 ✅ | `G2_TransformBone` rétablit l'étape unsquash et `BONE_ANIM_NO_LERP`, `r_ghoul2nolerp`, `r_ghoul2noblend`. Crée `r_ghoul2unsquash` et `r_ghoul2timebase`. | `f8ad3049a` |
| 5.2 ✅ | `G2_TransformGhoulBones` lisse aussi les appels côté jeu, comme vanilla. `r_ghoul2animsmooth` vaut 0.25. | `0e8f695cd` |
| 5.3 ✅ | `R_AddGhoulSurfaces` force `lodBias` à 10 avec `RF_G2MINLOD`. | `a31f4a72c` |
| 5.4 ✅ | `r_lodscale` vaut 10 par défaut. La cvar est archivée : une valeur déjà sauvée (5) reste en place tant que l'utilisateur ne la remet pas à 10. | `ce28ddf05` |
| 5.5 ✅ | Le test `Cvar_VariableIntegerValue("dedicated")` est supprimé de `G2_TransformGhoulBones` : vanilla n'en a pas, et ce renderer ne tourne jamais en dédié. | `a3ba70b27` |
| 5.6 ✅ | `r_noghoul2` (`CVAR_CHEAT`) existe et coupe `R_AddGhoulSurfaces`, `RB_TransformBones` et la passe RTX. `r_noserverghoul2` reste active. | `2cf4c4df0` |
| 6.1 ✅ | `ParseShader` saute `hitLocation` et `hitMaterial` avec leur argument, comme vanilla. | `40d96110c` |
| 6.2 ✅ | Les étages de surface sprites en `depthFunc equal` sont créés et dessinés avec leur état, comme vanilla. Le nom du shader d'étage inclut les `stateBits`. | `79be86c4f` |
| Revue M2 ✅ | `CullPoints` teste les quatre plans latéraux seulement : les coins de la boîte de ciel dépassent le plan lointain. | `f86a766ec` |
| Revue m1 ✅ | `RE_RenderWorldEffects` et `RE_RenderAutoMap` testent `tr.registered`, comme vanilla. | `21c2438b4` |
| Revue m2 ✅ | Le quad orienté RTX dérive ses axes de la normale (`axis[0]`), comme le raster. | `03a6cea81` |
| Feuilles noires yavin_swamp (G-buffer) ✅ | `RB_RenderGBufferSurfList` saute les stages surface sprite et s'arrête sur un stage opaque qui écrit la profondeur ; `FindLightingStage` saute aussi les stages `ss`. | `c7e279598`, `d495b7f4e` |
| Normales faces arrière G-buffer ✅ | Les cinq `gbuffer*.frag` qui écrivent la normale l'inversent quand `!gl_FrontFacing` (le SSAO lit donc une normale tournée vers la caméra sur les surfaces à double face). En jeu : les rectangles noirs du SSAO sur les palmiers vus de dos disparaissent. | `ca113d91f` |

Les points 2.2 à 2.11 de cette liste sont validés à la compilation (Debug et Release) et par lecture comparée avec `rd-vanilla`. Ils n’ont pas été testés en jeu.

Mesure de la fuite 1.3 : la zone totale reste entre 329 et 352 Mo sur 8 allers-retours entre deux maps, contre +50 Mo par map avant. Non testés : un second `vid_restart` et le retour du double renderer vers le simple.

Ordre de gravité dans chaque tableau : crash / bloquant, puis visuel, puis perf, puis mineur.

---

## 1. Cycle de vie, mémoire, chargement de map, refexport

| # | Gravité | Fichier:fonction (rd-vulkan) | Équivalent vanilla | Manquement | Confiance | Raison (non fait / reporté) |
|---|---|---|---|---|---|---|
| 1.1 ✅ | **bloquant** | `tr_init.cpp:C_LevelLoadEnd` (`re.RegisterMedia_LevelLoadEnd`) | `tr_model.cpp:RE_RegisterMedia_LevelLoadEnd` | Vanilla remet `*(ri.gbAlreadyDoingLoad()) = qfalse` en fin de chargement. rd-vulkan ne le fait pas. Après le premier chargement de sauvegarde réussi, `SV_LoadGame_f` refuse toute nouvelle commande `load` ("Already loading") jusqu'à un ERR_DROP. Seul `SG_Shutdown` (sur ERR_DROP) remet le drapeau. rd-rend2 le remet aussi. En mode double renderer, le second renderer peut masquer le bug. | confirmé | — |
| 1.2 | **crash** (hérité) | `tr_skin.cpp:RE_RegisterSkin` l.250 | `tr_skin.cpp:RE_RegisterSkin` l.417/422 | `Hunk_Alloc( sizeof(skin->surfaces[0]) )` alloue la taille d'un pointeur (8 octets). La ligne suivante écrit `->shader` à l'offset 64 de `skinSurface_t`. Débordement de tas quand un nom de skin n'est pas un `.skin`. Le bug existe aussi dans vanilla. `R_InitSkins` (l.425) utilise la bonne taille. | confirmé | — |
| 1.3 ✅ | perf (fuite, risque OOM) | `tr_subs.cpp:Hunk_Alloc` + `tr_model.cpp:RE_BeginRegistration` | `tr_subs.cpp:R_Hunk_Alloc` (TAG_HUNKALLOC) + `Hunk_Clear` | `h_low` est maintenant TAG_GENERAL, que rien ne libère. Or `RE_BeginRegistration` appelle `R_Init` à chaque map (après `CL_FlushMemory` → `re.Shutdown(qfalse)`). Chaque chargement réalloue sans libérer : données BSP (25 sites dans `tr_bsp.cpp`), MD3 et GLM (`tr_model.cpp`), `s_shaderText` et la table de hachage des shaders, shaders, skins, VBO, tangentes MikkTSpace, `backEndData`. Vanilla met tout en TAG_HUNKALLOC, que `Hunk_Clear` libère à chaque map. Fuite de plusieurs dizaines de Mo par map ou par chargement de sauvegarde. | confirmé | — |
| 1.4 ✅ | perf (fuite) | `tr_main.cpp:R_GenerateDrawSurfs` l.1664 | — (pas d'équivalent) | `Hunk_Alloc( sizeof(srfSprites_t), h_low )` à chaque vue et à chaque frame, dès qu'un groupe de surface sprites est visible. Le bloc TAG_GENERAL n'est jamais libéré. Fuite continue et un `Z_Malloc` par vue. | confirmé | — |
| 1.5 ❌ | perf | `tr_init.cpp:RE_Shutdown` + `tr_cache.cpp:C_Images_LevelLoadEnd` | `tr_init.cpp:RE_Shutdown`, `tr_image.cpp:RE_RegisterImages_LevelLoadEnd` | Vanilla garde les textures entre deux maps ("only do this for vid_restart now, not during things like map load") et purge celles que la nouvelle map n'utilise pas. rd-vulkan appelle `vk_delete_textures()` à chaque `RE_Shutdown`. `C_Images_LevelLoadEnd` retourne `qfalse` sans rien faire. Toutes les textures se rechargent à chaque map. | confirmé | Décision de l'utilisateur : non prévu pour l'instant. Garder les textures entre deux maps touche le pool d'images, déjà source de bugs (détruit, jamais reconstruit). |
| 1.6 ✅ | perf | `tr_WorldEffects.cpp:COutside::Cache` | `COutside::Cache` + `ReadCachedWeatherFile` | Le cache disque `maps/<map>.weather` (rd-vulkan : `.vkweather`) n'existe plus. La grille intérieur/extérieur se recalcule à chaque chargement de map. | confirmé | — |
| 1.7 ✅ (partiel) | mineur | `tr_init.cpp:RE_LevelLoadBegin` | `RE_RegisterMedia_LevelLoadBegin` | `bAllowScreenDissolve` est ignoré. `eForceReload_BSP` ne supprime pas les lightmaps. Le cache `s_animationCFGs` n'est jamais vidé (vanilla appelle `RE_AnimationCFGs_DeleteAll` au rechargement forcé). Traité : le vidage de `s_animationCFGs`. Non traité : `bAllowScreenDissolve` (le dissolve n'existe pas dans rd-vulkan) et la suppression des lightmaps (inutile : `RE_Shutdown` détruit déjà toutes les images à chaque map). | confirmé | Partiel. Le dissolve n'existe pas dans rd-vulkan (voir 2.7). Les lightmaps sont déjà détruits par `RE_Shutdown`. `CM_DeleteCachedMap` n'est pas ajouté (utile en développement seulement). |
| 1.8 ✅ | mineur | `tr_init.cpp:stub_R_ClearStuffToStopGhoul2CrashingThings` | `R_ClearStuffToStopGhoul2CrashingThings` (`memset(&tr)`) | No-op. `Hunk_Clear` l'appelle. `R_Init` remet `tr` à zéro ensuite, donc l'effet est faible. | confirmé | — |
| 1.9 ❌ | mineur | `tr_init.cpp:stub_Scissor`, `stub_GetScreenShot`, `stub_GetModelBounds` | `RE_Scissor`, `RE_GetScreenShot`, `RE_GetModelBounds` | Stubs vides. En JKA, `CG_Scissor` n'a pas d'appelant. `GetScreenShot` ne sert qu'au code `JK2_MODE`. `GetModelBounds` n'a pas d'appelant. | confirmé | Faux positif : aucun appelant en JKA (`CG_Scissor`, `GetModelBounds`). `GetScreenShot` ne sert qu'au code `JK2_MODE`. |
| 1.10 ✅ | mineur | `tr_init.cpp:commands[]` | `tr_init.cpp:commands[]` | Absentes : `r_fogDistance`, `r_fogColor`, `r_reloadfonts`, `imagecacheinfo`, `modelcacheinfo`. `screenshot_png` et `screenshot_tga` appellent `R_ScreenShot_f`. Les cinq commandes sont portées. Pas un défaut : `R_ScreenShot_f` choisit le format d'après `Cmd_Argv(0)`, donc `screenshot_png` écrit bien un `.png` et `screenshot_tga` un `.tga`. | confirmé | — |

## 2. Scène, refEntity, effets FX

| # | Gravité | Fichier:fonction (rd-vulkan) | Équivalent vanilla | Manquement | Confiance | Raison (non fait / reporté) |
|---|---|---|---|---|---|---|
| 2.1 ✅ | **visuel (majeur)** | `tr_surface.cpp:RB_SurfaceOrientedQuad` | `RB_SurfaceOrientedQuad` | rd-vulkan lit `left`/`up` dans `e.axis[1]`/`e.axis[2]` (convention MP). Vanilla les dérive de `axis[0]` par `MakeNormalVectors`. Le cgame SP (`COrientedParticle::Draw`, `FxPrimitives.cpp`) ne remplit que `axis[0]`, et `CEffect` met `mRefEnt` à zéro. Tous les `OrientedParticle` du système FX (flashs et anneaux d'impact sur les murs, etc.) sont des quads de surface nulle, donc invisibles en raster. Même classe de bug que l'électricité. | confirmé | — |
| 2.2 ✅ | **visuel** | `tr_surface.cpp:RB_SurfaceCylinder` | `RB_SurfaceCylinder` + `RB_SurfaceCone` | Le rayon de fin vient de `e->rotation` (MP). Le cgame SP l'écrit dans `e->backlerp` (`CCylinder::UpdateSize2`). Les cylindres FX deviennent des cônes ou s'effondrent. Le repli vers `RB_SurfaceCone` (bout effilé) manque. Le niveau de détail divise par 1024 au lieu de 2048. | confirmé | — |
| 2.3 ✅ | visuel | `tr_scene.cpp:RE_AddPolyToScene` | `RE_AddPolyToScene` | Choix du volume de brouillard par simple chevauchement (Q3). Vanilla préfère un volume qui contient tout le poly, puis un volume partiel identique à celui du joueur (`R_FogParmsMatch`). Voir 3.1. | confirmé | — |
| 2.4 ✅ | visuel | `tr_mesh.cpp:R_AddMD3Surfaces` | `R_AddMD3Surfaces` | `RF_CAP_FRAMES` est ignoré. Une animation MD3 « one shot » qui dépasse la dernière frame passe par le contrôle « no such frame » et revient à la frame 0 au lieu de rester bloquée sur la dernière. | confirmé | — |
| 2.5 ✅ | visuel | `tr_marks.cpp:R_MarkFragments` | `R_MarkFragments` | Les marques sur `SF_TRIANGLES` (misc_model statiques) n'existent que si `r_marksOnTriangleMeshes` vaut 1 (défaut 0). Vanilla les pose toujours et rejette les triangles de dos par leur normale de face. Sans la cvar, pas d'impact de tir sur ces modèles. | confirmé | — |
| 2.6 ✅ | visuel | `tr_shade_calc.cpp:RB_CalcDisintegrateColors` | `RB_CalcDisintegrateColors( colors, rgbGen )` | Le paramètre `rgbGen` a disparu. Avec `CGEN_LIGHTING_DIFFUSE_ENTITY`, vanilla teinte le bord de désintégration par `shaderRGBA`. rd-vulkan ne le fait pas. | confirmé | — |
| 2.7 | visuel | `tr_init.cpp:stub_InitDissolve`, `stub_ProcessDissolve` | `RE_InitDissolve`, `RE_ProcessDissolve` | Pas de fondu d'écran en fin de cinématique (`cl_cin.cpp`, `CL_EndScreenDissolve_f`) ni en fin de chargement. | confirmé | Non fait : gros chantier (capture d'écran, passes de fondu, appelants de `cl_cin.cpp`, lien avec `bAllowScreenDissolve`). Effet purement cosmétique. |
| 2.8 ✅ | mineur | `tr_surface.cpp:RB_SurfaceBeam` | `RB_SurfaceBeam` | Couleur fixe rouge. Vanilla choisit rouge, vert ou bleu selon `skinNum`. | confirmé | — |
| 2.9 ❌ | mineur | `tr_main.cpp:R_AddDrawSurf` / `tr_scene.cpp:RE_ClearScene` | idem | `RDF_doLAGoggles`/`RDF_doFullbright` ne sont plus posés ni effacés. Seul le booléen `doLAGoggles` sert. Comme `RE_RenderScene` écrase `rdflags`, l'effet en vanilla est déjà nul. Rien à porter : aucun code de rd-vulkan ne lit ces bits. | confirmé | Faux positif : `RE_RenderScene` écrase `rdflags`, donc l'effet est déjà nul en vanilla. Aucun code de rd-vulkan ne lit ces bits. |
| 2.10 ❌ | mineur | `tr_init.cpp` (`tr_distortionPrePost`) | `RB_RenderDrawSurfList` (`tr_stencilled && tr_distortionPrePost`) | La variante « pré/post » de la distorsion n'existe pas. Le cgame SP envoie toujours `qfalse`. | confirmé | Inutile : le cgame SP envoie toujours `qfalse`. |
| 2.11 ✅ | mineur | `tr_cmds.cpp:RE_StretchPic`, `RE_RotatePic`, `RE_RotatePic2` | idem | Pas de test `tr.registered` avant d'écrire la commande. | confirmé | — |

## 3. Brouillard, distance, ciel, météo

| # | Gravité | Fichier:fonction (rd-vulkan) | Équivalent vanilla | Manquement | Confiance | Raison (non fait / reporté) |
|---|---|---|---|---|---|---|
| 3.1 ✅ | **visuel** | `tr_main.cpp:R_SpriteFogNum`, `tr_ghoul2.cpp:R_GComputeFogNum`, `tr_mesh.cpp:R_ComputeFogNum`, `tr_main.cpp:R_RenderView` | `R_SetViewFogIndex`, `R_FogParmsMatch`, versions SP des `*FogNum` | `tr.refdef.fogIndex` (volume du joueur), `R_SetViewFogIndex` et `R_FogParmsMatch` n'existent pas. Les trois fonctions gardent le test Q3 « premier volume qui chevauche ». Vanilla prend d'abord un volume qui contient toute l'entité, puis un volume partiel identique à celui de la caméra, puis le premier volume partiel. Les entités, sprites et modèles à cheval sur deux brouillards prennent le mauvais. **Traité : R_FogNumForSphere (tr_main.cpp) reprend la logique de vanilla (volume qui contient tout, puis volume partiel du point de vue, puis premier partiel, goggles) pour sprites, ghoul2 et MD3.** | confirmé | — |
| 3.2 ✅ | **visuel** | `tr_bsp.cpp:R_LoadEntities`, `tr_init.cpp:SetRangedFog` | `R_LoadEntities` (`linFogStart`), `RE_SetRangedFog`, `tr_shade.cpp` (GL_LINEAR) | `tr.rangedFog` est écrit mais aucun code de rendu ne le lit. Le brouillard linéaire de la lunette (sniper/disruptor) et l'override `linFogStart` du worldspawn n'existent pas. `SetRangedFog(0)` ne restaure pas `g_oldRangedFog`. **Traité : `linFogStart` est lu dans `R_LoadEntities` (et `rangedFog` remis à 0 à chaque map) ; `SetRangedFog` restaure `g_oldRangedFog` ; `vk_update_fog_constants` calcule début/fin comme `tr_shade.cpp` de vanilla et les met dans `linearStart`/`linearEnd` du brouillard global et du brouillard goggles ; `CalcFog` (fog_frag, gen_frag, surface_sprite_frag) fait un brouillard linéaire quand `linearEnd > 0`. Distance radiale, comme le chemin exponentiel existant.** | confirmé | Traité. Validé par compilation (Debug et Release) et comparaison avec rd-vanilla, non testé en jeu. |
| 3.3 ✅ | **visuel** | `tr_bsp.cpp:R_LoadEntities` l.2394 | `R_LoadEntities` l.1238 | `tr.distanceCull` vaut 6000 par défaut au lieu de 12000. `R_SetFarClip` plafonne `zFar` à `distanceCull*1.732` : environ 10 400 unités au lieu de 20 800 sur les maps sans clé `distanceCull`. La géométrie lointaine et le ciel sont coupés sur les grandes maps extérieures. `r_distanceCull` ne s'applique que si la map a la clé. **Traité : la valeur par défaut vaut 12000 comme vanilla. Le plan lointain du 3.8 évite que la distance de rendu dépasse celle de vanilla.** | confirmé | — |
| 3.4 ✅ | **visuel** | `tr_WorldEffects.cpp:CWeatherParticleCloud::Render` | `CParticleCloud::Render` (glBegin) | Le rendu s'arrête dès que `tess.numVertexes > SHADER_MAX_VERTEXES - 4` (1200), sans vidage intermédiaire. Au plus 300 quads ou 400 triangles par nuage. `snow`, `rain`, `heavyrain` et `acidrain` créent 1000 particules : une grande partie n'est jamais dessinée. **Traité : le lot est dessiné et vidé quand il atteint SHADER_MAX_VERTEXES, puis le rendu continue.** | confirmé | — |
| 3.5 ✅ | visuel | `tr_WorldEffects.cpp` (constantes) | idem | `MAX_WEATHER_ZONES` vaut 10 au lieu de 50, `MAX_WIND_ZONES` 10 au lieu de 12, `POINTCACHE_CELL_SIZE` 96 au lieu de 32 (valeurs MP). Les zones météo au-delà de 10 sont perdues. La détection intérieur/extérieur est 3 fois plus grossière : pluie ou neige à l'intérieur près des ouvertures. **Traité : 50 zones météo, 12 zones de vent, cellule de 32 comme vanilla ; le magic du cache .vkweather passe à WVK2, donc les anciens fichiers (cellule de 96) sont refusés. Le premier chargement d'une grande map recalcule la grille (environ 20 s sur valley).** | confirmé | — |
| 3.6 ✅ | visuel | `tr_WorldEffects.cpp:Update`/`Render`, `RB_RenderWorldEffects` | idem | L'orientation selon la vitesse de chaque particule (`mOrientWithVelocity`) devient une orientation fixe sur la normale du plan d'apparition : les gouttes ne suivent plus le vent. Le test `ri.CL_IsRunningInGameCinematic()` a disparu : la météo s'affiche pendant les cinématiques in-game. `mFilterMode` est ignoré. **Traité : l'orientation suit la vitesse de chaque particule, la météo est coupée pendant les cinématiques in-game, le filtre NEAREST utilise un descripteur dédié. Non touché : Wrap de SVecRange (version MP) et ordre des texcoords des quads.** | confirmé | — |
| 3.7 ✅ | visuel | `tr_backend.cpp:vk_update_fog_constants` | — | L'UBO de brouillard copie au plus 16 volumes (`MIN(num_fogs, 16)`). Au-delà, l'index de brouillard pointe vers des données vides. **Traité : `VK_MAX_FOGS` = 64 (UBO de 3088 octets, minimum Vulkan 16384). Le tri des surfaces n'a que 5 bits d'index de brouillard. `fogs[64]` dans fog_frag, gen_frag et surface_sprite_frag, index borné par `fogs.length()`, `num_fogs` borné côté C++. 32 variantes SPIR-V régénérées une par une.** | probable | Traité. Validé par compilation (Debug et Release) et comparaison avec rd-vanilla, non testé en jeu. |
| 3.8 ✅ | perf | `tr_main.cpp:R_SetupFrustum` | `R_SetupFrustum` (plan 4 = plan lointain à `distanceCull*1.02`) | Le plan 4 de rd-vulkan est le plan proche. Aucun culling lointain à `distanceCull`. Tout ce qui est avant `zFar` est soumis. **Traité : le plan 4 devient le plan lointain à distanceCull*1.02, testé par R_CullLocalBox, R_CullPointAndRadius et R_RecursiveWorldNode (planBits 31), sauf pour les vues sans monde. Impact RTX : visBounds (ombres, god rays) est borné par la distance.** | confirmé | — |

## 4. Éclairage

| # | Gravité | Fichier:fonction (rd-vulkan) | Équivalent vanilla | Manquement | Confiance | Raison (non fait / reporté) |
|---|---|---|---|---|---|---|
| 4.1 ✅ | visuel | `tr_light.cpp:R_SetupEntityLighting` | `R_SetupEntityLighting` | `RF_MORELIGHT` (SP : +96 de lumière ambiante au lieu de +32) est ignoré. Le cgame SP le pose sur l'arme en vue et sur certains effets joueur (`cg_players.cpp` l.4653, 8283, 9523) : ils sont plus sombres qu'en vanilla. À la place, le code garde la logique MP de `RF_MINLIGHT` (teinte jaune ou bleue des items « holo »). Le cgame SP ne pose plus ce drapeau, donc ce code est mort. **Traité : +96 avec `RF_MORELIGHT`, sinon +32, et le bloc `RF_MINLIGHT` mort est retiré. Correction de l'audit : le cgame ne pose pas le drapeau sur l'arme en vue, seulement sur la coque de Force Sight (4653) et sur les PNJ avec `SCF_MORELIGHT` (8283, 9523). Impact RTX : `R_SetupEntityLighting` y est aussi appelé, ces entités s'éclairent plus.** | confirmé | — |
| 4.2 ✅ | visuel | `tr_init.cpp:R_Register` | — | `r_dlightScale` vaut 0.8 par défaut : le rayon des lumières dynamiques est 20 % plus petit qu'en vanilla. **Traité : défaut à 1.0 (vanilla n'a pas cette cvar, le rayon n'y est pas mis à l'échelle). La cvar est `CVAR_ARCHIVE_ND` : une valeur différente du défaut déjà sauvée dans la config reste en place. Impact RTX : le tracer divise par cette valeur, donc il reste cohérent.** | confirmé | — |
| 4.3 ✅ | mineur | `tr_scene.cpp:RE_AddLinearLightToScene` | — | La saturation utilise `r_mapGreyScale` au lieu de `r_dlightSaturation` (copier-coller). Aucun appelant SP. **Traité : la saturation lit `r_dlightSaturation`. Sans appelant SP, donc sans effet visible.** | confirmé | — |
| 4.4 ❌ | mineur | `tr_main.cpp:R_RenderView` | `R_RenderView` (`r_debugStyle`) | La cvar de debug `r_debugStyle` n'existe pas. **Non prévu : cvar `CVAR_CHEAT` de debug. Un essai (styles noirs dans `R_RenderView`) n'a touché que l'éclairage des entités ; les lightmaps du monde restent inchangées, car vanilla saute les stages lightmap dans `RB_IterateStagesGeneric`, structure absente ici. Retiré.** | confirmé | Non faisable proprement : rd-vulkan n'a pas le saut des stages lightmap par style de `RB_IterateStagesGeneric`. Cvar de debug. |

## 5. Ghoul2 et modèles

| # | Gravité | Fichier:fonction (rd-vulkan) | Équivalent vanilla | Manquement | Confiance | Raison (non fait / reporté) |
|---|---|---|---|---|---|---|
| 5.1 ✅ | visuel | `tr_ghoul2.cpp:G2_TransformBone` | `G2_TransformBone` | Le `tr_ghoul2.cpp` vient de MP. L'étape « unsquash » (`r_Ghoul2UnSqash`, défaut 1 en SP) est commentée : les os gardent l'écrasement dû à l'interpolation. `BONE_ANIM_NO_LERP`, `r_Ghoul2NoLerp` et `r_Ghoul2NoBlend` sont commentés. Ces trois cvars et `r_ghoul2timebase` ne sont pas créées. Effet visible faible, mais c'est un écart SP. | confirmé (effet probable) | — |
| 5.2 ✅ | visuel | `tr_ghoul2.cpp:G2_TransformGhoulBones` | `G2_TransformGhoulBones` | Le lissage d'animation ne s'active que si `HackadelicOnClient` est vrai (MP). Vanilla lisse aussi les appels côté jeu (bolts, collisions). Le jeu et le rendu peuvent voir des positions d'os différentes (sabre, armes). `r_ghoul2animsmooth` vaut 0.3 au lieu de 0.25. | probable | — |
| 5.3 ✅ | visuel / perf | `tr_ghoul2.cpp:R_AddGhoulSurfaces` | `R_AddGhoulSurfaces` | `RF_G2MINLOD` est ignoré. Vanilla force alors `lodBias` à 10 (LOD le plus bas). | confirmé | — |
| 5.4 ✅ | visuel | `tr_init.cpp:R_Register` (`r_lodscale`) | `r_lodscale` = 10 | `r_lodscale` vaut 5 : les LOD grossiers arrivent plus près de la caméra qu'en vanilla. | confirmé | — |
| 5.5 ✅ | perf | `tr_ghoul2.cpp:G2_TransformGhoulBones` | — | `ri.Cvar_VariableIntegerValue("dedicated")` (recherche par chaîne) à chaque transformation de squelette. | confirmé | — |
| 5.6 ✅ | mineur | `tr_ghoul2.cpp:R_AddGhoulSurfaces` | `r_noghoul2` | Le test lit `r_noServerGhoul2` (MP). `r_noghoul2` n'existe pas. | confirmé | — |

## 6. Shaders, BSP, divers

| # | Gravité | Fichier:fonction (rd-vulkan) | Équivalent vanilla | Manquement | Confiance | Raison (non fait / reporté) |
|---|---|---|---|---|---|---|
| 6.1 ✅ | visuel | `tr_shader.cpp:ParseShader` | `ParseShader` (`hitLocation`, `hitMaterial`) | Les deux mots-clés ne sont pas parsés. Le parseur rend `qfalse` et le shader entier devient le shader par défaut. Vanilla saute le mot-clé et son argument. L'impact dépend des `.shader` livrés. | probable | — |
| 6.2 ✅ | visuel | `vk_vbo_surfacesprites.cpp` (création des groupes) | `tr_surfacesprites.cpp:RB_DrawSurfaceSprites` | Les étages de surface sprites avec `depthFunc equal` (hors étage 0) sont ignorés, avec un avertissement. Vanilla les dessine. Herbe absente sur ces shaders. | probable | — |
| 6.3 ❌ | mineur | `tr_bsp.cpp:R_LoadEntities` | bloc commenté dans vanilla | `R_RemapShader(value, s, "0")` passe le mot-clé (`s`) au lieu du shader cible (`vs`). `remapshader` et `vertexremapshader` du worldspawn ne font rien. Vanilla SP ne les gère pas non plus. | confirmé | Vanilla SP a désactivé ces remaps ; l'appel actuel est sans effet et le corriger activerait un comportement absent de vanilla. |
| 6.4 ❌ | mineur | `tr_terrain.cpp` | — | Fichier MP non compilé (absent de `CMakeLists.txt`). Il contient des `tess.texCoords[numVertexes][0]` transposés. Code mort à supprimer ou à corriger avant réactivation. | confirmé | Fichier non compilé et sans équivalent dans rd-vanilla : le SP n'en a pas besoin. |

---

## Points vérifiés sans écart

- `refexport_t` : tous les champs de vanilla sont assignés. Seuls six champs pointent vers des stubs (voir 1.8, 1.9, 2.7).
- Mots-clés de shader : seuls `hitLocation` et `hitMaterial` manquent (6.1). Toutes les valeurs `CGEN_`, `AGEN_`, `TCGEN_`, `TMOD_`, `DEFORM_` et `GF_` de vanilla sont gérées.
- Commandes météo : la liste est identique, avec `die` en plus.
- `G2_bones.cpp`, `G2_bolts.cpp`, `G2_surfaces.cpp`, `G2_API.cpp` et `G2_misc.cpp` ne diffèrent de vanilla que par la substitution mécanique `mdxm`/`mdxa` vers `data.glm`/`data.gla` et par des optimisations (cache de `G2_SetupModelPointers`, skinning de collision).
- `RB_SurfaceSprite`, `RB_SurfaceLine`, `RB_SurfaceSaberGlow`, `RB_SurfaceLathe`, `RB_SurfaceClouds`, `R_inPVS`, `RE_GetLighting`, les styles de lumière et `RT_ENT_CHAIN` correspondent à vanilla.
- Aucun nouveau local `static` fautif ni nouveau `Z_Free(NULL)` trouvé hors `rtx/`.

## Synthèse : top 10

1. ✅ **1.1** `gbAlreadyDoingLoad` jamais remis à zéro : après une sauvegarde chargée, les chargements suivants sont ignorés.
2. ✅ **1.3** Toutes les allocations `h_low` partent en TAG_GENERAL et `R_Init` tourne à chaque map : fuite de toutes les données de map (BSP, modèles, texte des shaders) à chaque chargement.
3. ✅ **2.1** `RB_SurfaceOrientedQuad` lit `axis[1]`/`axis[2]` : tous les `OrientedParticle` FX du SP sont invisibles.
4. **2.2** `RB_SurfaceCylinder` lit `rotation` au lieu de `backlerp` : les cylindres FX deviennent des cônes.
5. **3.4** Le rendu météo s'arrête à 1200 sommets par nuage : pluie et neige affichent moins de la moitié des particules.
6. **3.3** `distanceCull` vaut 6000 au lieu de 12000 : `zFar` coupe les grandes maps extérieures à environ 10 400 unités.
7. **3.1 / 3.2** Pas de brouillard du point de vue (`fogIndex`, `R_FogParmsMatch`) et pas de brouillard linéaire (lunette, `linFogStart`).
8. ✅ **1.4** Fuite d'un `srfSprites_t` à chaque vue et à chaque frame quand des surface sprites sont visibles.
9. **3.5 / 3.6** Météo réglée sur les valeurs MP : 10 zones, cellules de 96 unités, orientation fixe, météo pendant les cinématiques.
10. **4.1 / 1.2** `RF_MORELIGHT` ignoré (arme en vue plus sombre) ; débordement de tas hérité dans `RE_RegisterSkin`.
