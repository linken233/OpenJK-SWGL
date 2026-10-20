# Audit des options graphiques par écran de menu (SP)

Audit en lecture seule, branche `feature/vulkan-raster`, 2026-10-10. Remplace la version du 2026-10-09 (organisée par famille de cvars).

Sources code : `code/rd-vanilla` (OpenGL, `rdsp_swgl`), `code/rd-rend2` + `shared/rd-rend2` (Rend2), `code/rd-vulkan` (Vulkan raster, et chemin RTX `USE_RTX` : `rtx/`, `shaders/glsl/rtx/global_ubo.h`), `shared/sdl/sdl_window.cpp` et `shared/sys/sys_main.cpp` (cvars fenêtre partagées), `code/cgame`, `code/ui/ui_main.cpp`.
Sources menu : `C:\jka_menus\ui\Setup.menu` et `IngameSetup.menu` (synchronisés avec le dernier `SWGL_Menu.pk3`), chaînes `strings/English/SWGL.str`, `SWGL_OPTIONS.str`, `strings/French/SWGL.str`. Aucun autre `.menu` SP ne lie de cvar graphique (`ui/jamp/setup.menu` est le menu MP, hors périmètre).

Conventions : "déf" = valeur par défaut ; "latch" = pris en compte au `vid_restart` ; "=" = identique à la colonne OpenGL ; "=vk" = identique au raster Vulkan ; n/a = cvar non enregistrée par ce renderer ; "Appliquer" = le bouton APPLY CHANGES (`ui_r_modified 1`, puis `uiScript updatevideosetup` qui copie les `ui_r_*` vers les `r_*` et lance `vid_restart`). Lignes marquées **[KO]** : option présente mais fausse, morte ou sans effet sur au moins un renderer.

## État après la refonte (2026-10-10)

Les sections suivantes décrivent le menu d'avant la refonte. Menu actuel (Setup.menu et IngameSetup.menu, section VIDEO identique) :

| Onglet | Options |
|---|---|
| DISPLAY | Video Mode (`ui_r_mode`), Width X Height, Window Mode (`ui_r_windowmode` : fenêtré / sans bordure / plein écran, écrit `r_fullscreen` + `r_noborder` à Appliquer), Video Sync (Appliquer), Frame Rate Limit (`com_maxfps`), Field of View (`cg_fov` 60..130), Renderer, Brightness. Préréglage "Video Quality" retiré. |
| QUALITY | Geometric Detail, Texture Detail, Texture Quality, Texture Filter, Anisotropic Filter (Appliquer ; à Appliquer, `r_ext_max_anisotropy` reçoit la valeur), Detailed Shaders, Dynamic Lights, Light Flares, Wall Marks. |
| OPENGL | Shadows, Color Depth, Compressed Textures, Dynamic Glow, MSAA. |
| REND2 1/3 | Shadows, HDR, Tonemapping, Autoexposure, HDR Lighting, MSAA, SSAO, Dynamic Glow, Glow Bloom, Depth Prepass, Color Depth. |
| REND2 2/3 | Deluxemapping, Deluxemap Specular (Appliquer), Cubemapping, Cubemapping bounces, Parallax, Generate Normalmaps, Compressed Textures. |
| REND2 3/3 | Sun Shadows, Shadow Filtering (0..2), Dynamic Light Shadows (`r_dlightMode` 1/2), SMAA (Off / 1x ; 2 = T2x plante rd-rend2 au chargement de carte), SMAA Quality (visible si SMAA), Volumetric Fog, God Rays (`r_drawSunRays`). |
| VULKAN > GENERAL | Ray Tracing (`r_rtx`), Shadows (0..3), Normal Mapping, Generate Normalmaps, Specular Mapping, Deluxemapping, Anisotropy Level (même cvar que QUALITY). |
| VULKAN > ADVANCED (raster, bouton visible si `r_rtx` 0) | Bloom, Bloom Intensity, Ambient Occlusion (Off/SSAO/GTAO, met `r_depthPrepass 1`), Contact Shadows (actif si GTAO), Dynamic Light Mode, Supersampling, Cubemapping, Dynamic Glow, MSAA, Compressed Textures. Les items qui exigent le FBO mettent aussi `r_fbo 1`. |
| VULKAN > ADVANCED (RTX, bouton visible si `r_rtx` 1) | Temporal Anti-aliasing (`flt_taa`), Bloom (`pt_bloom`), Bloom Intensity (`pt_bloom_intensity`). Ces trois cvars sont archivées. |

Corrections du code liées :
- Constat 3 corrigé autrement : `Cvar_Set` force la valeur, donc le menu écrivait les cvars latch tout de suite (affichage juste, mais le renderer en cours voyait une valeur qu'il n'avait pas initialisée). Le menu passe maintenant par `Cvar_SetLatched` (valeur en attente jusqu'au `vid_restart`) et affiche la valeur en attente (`Cvar_PendingStringBuffer`). Un drapeau `CVAR_LATCH` posé par un renderer reste sur la cvar après un changement de renderer : les items à cvar partagée (glow, anisotropie, `r_dlightMode`) demandent donc toujours Appliquer.
- rd-vulkan : `r_fbo` et `r_vbo` valent 1 par défaut ; `pt_bloom*` est enregistré aussi hors RTX.
- Fenêtre SDL : une fenêtre sans bordure est centrée sur son écran (elle couvre l'écran en résolution bureau).
- `@SWGL_SETUP_RENDERER_VULKAN` ajouté.

## Structure du menu vidéo

Onglet **VIDEO** (Setup.menu et IngameSetup.menu, contenu identique) :

| Sous-onglet (libellé EN, pas de traduction FR) | Groupe | Visibilité |
|---|---|---|
| DISPLAY | `pg_video_display` | toujours |
| QUALITY | `pg_video_quality` | toujours |
| OPENGL | `pg_video_gl` | caché si `cl_renderer` = rdsp-rend2 ou rdsp-vulkan |
| REND2 (pages 1/2 et 2/2) | `pg_video_r2a`, `pg_video_r2b` | visible si `cl_renderer` = rdsp-rend2 |
| VULKAN | `pg_video_vk` | visible si `cl_renderer` = rdsp-vulkan ; **aucun item** |

Autres écrans qui lient une cvar graphique : IngameSetup > **ANGLES** (`cg_fov`), GAME > **DISPLAY** (`cg_g2Marks`, `cg_drawFPS`), GAME > **GENERAL** (`ui_screenshotType`).

Les sous-onglets et `SWGL_OPTIONS.str` n'existent qu'en anglais. `French/SWGL.str` n'a pas non plus `SETUP_RENDERER*` ni `SETUP_REND2_*`.

## Constats principaux

1. **Le préréglage "Video Quality" écrase la résolution et le plein écran.** `UI_Update("ui_r_glCustom")` met `ui_r_mode` à 4 (800x600) ou 3 (640x480) et `ui_r_fullscreen` à 1 pour tous les niveaux. Un joueur en "Desktop Resolution" qui choisit "High" repasse en 800x600 après Appliquer.
2. **Le choix "Vulkan" du sélecteur de renderer n'a pas de libellé.** `@SWGL_SETUP_RENDERER_VULKAN` n'existe dans aucun fichier `.str`, et `SE_GetString` renvoie "" : la 3e entrée est vide.
3. **Les cvars latch liées directement s'affichent avec leur ancienne valeur.** L'UI lit `var->string` (`Cvar_VariableStringBuffer`), pas `latchedString`. Après un choix, `cl_renderer`, `r_hdr`, `r_toneMap`, `r_floatLightmap`, `r_ext_multisample`, `r_ssao`, `r_deluxeMapping`, `r_cubeMapping`, `r_cubeMappingBounces`, `r_genNormalMaps` (et sur Vulkan `r_DynamicGlow` et `r_ext_texture_filter_anisotropic`) affichent toujours l'ancienne valeur jusqu'au `vid_restart`. Trois items latch ne demandent même pas Appliquer : "Deluxemap Specular" (Rend2), "Dynamic Glow" et "Anisotropic Filter" (Vulkan). Ces choix ne sont pris en compte qu'au prochain `vid_restart` venu d'ailleurs.
4. **Sur Vulkan, trois options du menu sont sans effet ou fausses.** "Dynamic Glow" exige `r_fbo 1` (déf 0, absent du menu). "Anisotropic Filter" n'est qu'un interrupteur : le niveau vient de `r_ext_max_anisotropy` (déf 2), donc "16x" donne 2x. "Video Sync" est lu à la création du swapchain seulement, et le menu ne demande pas Appliquer.
5. **L'onglet VULKAN est vide**, et les options Vulkan restent inaccessibles : ombres (`cg_shadows` n'est que dans les onglets OpenGL/Rend2), MSAA, post-traitement (`r_fbo`, bloom, SSAO/GTAO), PBR, `r_rtx` et tout le traceur. Le MSAA marche aussi sur OpenGL (`r_ext_multisample_default_fb 1`) mais n'est que dans l'onglet Rend2.
6. Correctif écran large : `r_ratioFix` (rd-vanilla, lu aussi par cgame/ui et `cl_cin`) et `cl_ratioFix` (rd-vulkan). Sous Rend2 et Vulkan, cgame/ui enregistrent `r_ratioFix` avec la déf "" (= 0). Une entrée de menu doit écrire les deux cvars.
7. Les cvars UBO du traceur (`pt_*`, `flt_*`, `tm_*` de `global_ubo.h`) et `gr_*` sont enregistrées sans `CVAR_ARCHIVE` : une entrée de menu ne survit pas au redémarrage tant que leur drapeau ne change pas.

---

## 1. VIDEO > DISPLAY (Affichage)

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| **[KO]** Video Quality / Qualité vidéo (`ui_r_glCustom`) | préréglage 0..4 (Very High, High, Normal, Low, Custom ; déf 4) | = | = | = | Utile user | Oui (Appliquer) | Point d'entrée simple pour régler la qualité en un clic. Bug : chaque niveau force `ui_r_mode` 3 ou 4 et `ui_r_fullscreen` 1 (`ui_main.cpp` 5210-5276), donc il casse la résolution choisie. Il fixe aussi `ui_r_depthbits`, `ui_r_fastSky`, `ui_r_inGameVideo` qui n'ont pas d'item. À corriger : ne plus toucher mode/plein écran. |
| Video Mode / Mode vidéo (`ui_r_mode` -> `r_mode`) | partagé SDL : -2 bureau, -1 custom, 3..12 (liste 4:3 + 2400x600) ; latch | = | = | = | Utile user | Oui (Appliquer) | Résolution, réglage de base. Fonctionne partout (fenêtre SDL commune). Seules des résolutions 4:3 sont listées ; le 16:9 passe par "Desktop" ou "Custom". |
| Width X Height / Largeur X Hauteur (`r_customWidth`, `r_customHeight`) | partagé : déf 1600x1024, latch ; visible si mode -1 | = | = | = | Utile user | Oui (Appliquer) | Résolution libre, nécessaire pour le 16:9/21:9. Fonctionne partout. |
| Full Screen / Plein écran (`ui_r_fullscreen` -> `r_fullscreen`) | 0/1 (déf 0) | = | = | = | Utile user | Oui (Appliquer) | Plein écran ou fenêtré. Fonctionne partout. Avec `g_FastRendererSwitch` il n'y a pas de vrai plein écran (fenêtre SDL partagée). |
| **[KO]** Color Depth / Profondeur de couleur (`ui_r_colorbits` -> `r_colorbits`) | 0/16/32 (déf 0 = 24), latch | = | 0/16/32 ; sans effet sur le format de présentation (c'est `r_presentBits`) ; 16 réduit seulement l'overbright à 1 bit | sans effet (`overbrightBits` forcé à 0 sous RTX) | Utile user (héritage) | Oui (Appliquer) | Format du tampon GL. Sur Vulkan, la seule conséquence de 16 est une lumière plus terne. Option à garder pour OpenGL/Rend2, à masquer sur Vulkan. |
| **[KO]** Video Sync / Sync vidéo (`r_swapInterval`) | 0/1 (déf 0), appliqué à la prochaine image | = | 0 immédiat/mailbox, 1 FIFO, 2 FIFO relaxed, 3 mailbox ; lu à la création du swapchain seulement | =vk | Utile user | Oui (pas d'Appliquer) | Supprime le déchirement d'image. OpenGL/Rend2 : appliqué tout de suite (`SDL_GL_SetSwapInterval` sur `modified`). Vulkan : aucun effet avant un `vid_restart`, et l'item ne demande pas Appliquer. Ajouter `setcvar ui_r_modified 1` à l'action. |
| **[KO]** Renderer / Moteur de rendu (`cl_renderer`) | `rdsp_swgl` | `rdsp-rend2` | `rdsp-vulkan` | `rdsp-vulkan` + `r_rtx 1` | Utile user | Oui (Appliquer) | Choix du renderer, il conditionne les sous-onglets. Le libellé de "Vulkan" est vide (chaîne absente). La cvar est latch : la liste montre l'ancien renderer jusqu'au redémarrage. |
| Brightness / Luminosité (`r_gamma`) | curseur 0.5..3 (déf 1) ; gamma matériel | = (gamma shader en HDR) | = ; gamma matériel, ou passe gamma si `r_fbo 1` ; immédiat | =vk (appliqué après le tone mapper) | Utile user | Oui (immédiat) | Réglage indispensable pour l'écran du joueur. Fonctionne sur tous les renderers. |

### Options à ajouter

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Fenêtré sans bordure (`r_noborder`) | partagé : 0/1 (déf 0), latch | = | = | = | Utile user | Non | Mode le plus demandé sur les écrans modernes (alt-tab sans changement de mode). Widget : multi Off/On, Appliquer. Fonctionne partout (drapeau SDL). |
| Fréquence d'affichage (`r_displayRefresh`) | partagé : Hz, 0 = auto, latch, **non archivé** | = | = (lu par vk_info) | = | Utile user | Non | Permet 120/144 Hz en plein écran. Widget : multi Auto/60/120/144/165/240, Appliquer. Prérequis : ajouter `CVAR_ARCHIVE` dans `sdl_window.cpp`, sinon la valeur est perdue au lancement suivant. |
| Limite d'images/s (`com_maxfps`) | partagé : déf 125 | = | = | = | Utile user | Non | La limite de 125 bride les écrans 144/240 Hz et la physique de saut en dépend. Widget : multi 60/85/125/144/240/Illimité (0). Immédiat. |
| Correctif écran large (`r_ratioFix` + `cl_ratioFix`) | `r_ratioFix` 0/1 (déf 1) | n/a (cgame/ui lisent `r_ratioFix`, déf "") | `cl_ratioFix` 0/1 (déf 1) ; cgame/ui lisent `r_ratioFix` | =vk | Utile user | Non | Évite le HUD et le menu étirés en 16:9. Widget : multi Off/On dont l'action écrit les deux cvars (`setcvar r_ratioFix` et `setcvar cl_ratioFix`). Immédiat. |
| Polices au bon ratio (`r_aspectCorrectFonts`) | n/a | 0/1 (déf 0) | 0/1 (déf 0) | =vk | Utile user | Non | Corrige le texte étiré en écran large. Widget : multi Off/On, caché si `cl_renderer` = rdsp_swgl. |
| Champ de vision (`cg_fov`) | cgame : déf 80, borné 1..160 | = | = | = | Utile user | Partiel (IngameSetup > ANGLES seulement) | Le FOV est un réglage d'affichage de base, il doit être accessible depuis le menu principal. Widget : curseur 60..130. |

---

## 2. VIDEO > QUALITY (Qualité)

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Geometric Detail / Détails géométriques (`ui_r_lodbias` -> `r_lodbias` + `r_subdivisions`) | lodbias 0..2 (déf 0) ; subdiv 4/12/20 (déf 4, latch) | = | = | = ; `r_lodbias` lu aussi par le traceur (effets) | Utile user | Oui (Appliquer) | Gain de performance sur les modèles et courbes. Fonctionne partout. |
| Texture Detail / Détails des textures (`ui_r_picmip` -> `r_picmip`) | 0..3 dans le menu (déf 0), latch | = | = | = (mêmes images) | Utile user | Oui (Appliquer) | Économie de VRAM sur petites cartes. Fonctionne partout. |
| Texture Quality / Qualité des textures (`ui_r_texturebits` -> `r_texturebits`) | 0/16/32 (déf 0), latch | = | = (16 = formats 4444/1555) | = | Utile user (héritage) | Oui (Appliquer) | Utile seulement sur très vieux GPU. Fonctionne partout. Candidat au retrait plus tard. |
| Texture Filter / Filtre des textures (`ui_r_texturemode` -> `r_textureMode`) | bilinéaire / trilinéaire (déf trilinéaire) | = (déf `GL_LINEAR_MIPMAP_NEAREST`) | = (déf trilinéaire, appliqué en direct) | =vk | Utile user | Oui (Appliquer) | Qualité du filtrage. Fonctionne partout. |
| **[KO]** Anisotropic Filter / Filtre anisotropique (`r_ext_texture_filter_anisotropic`) | curseur 0..16 (déf 16) = niveau réel, borné au max GPU | = | déf 16, **latch**, 0 = off, sinon **simple interrupteur** : le niveau est `r_ext_max_anisotropy` (déf 2) | =vk | Utile user | Oui (pas d'Appliquer) | Netteté des sols en angle rasant. Sur Vulkan, "16x" donne 2x, et la valeur n'est prise qu'au prochain `vid_restart`. `r_ext_texture_filter_anisotropic_avail` n'est jamais écrit par Vulkan. Correctif recommandé : rd-vulkan prend le curseur comme niveau (au lieu d'ajouter `r_ext_max_anisotropy` au menu), et l'item demande Appliquer. |
| **[KO]** Compressed Textures / Textures compressées (`ui_r_ext_compress_textures` -> `r_ext_compress_textures`) | déf 1, latch (S3TC) | déf 0, latch | déf 0, latch (BC3) | **sans effet** : compression jamais activée sous RTX | Utile user | Oui (Appliquer) | Économie de VRAM. Le traceur l'ignore volontairement (`vk_init.cpp` 504-509). À indiquer dans la description. |
| Detailed Shaders / Shaders détaillés (`ui_r_detailtextures` -> `r_detailtextures`) | déf 1, latch | = | = | = | Utile user | Oui (Appliquer) | Couches de détail des textures. Fonctionne partout. |
| Dynamic Lights / Lumières dynamiques (`r_dynamiclight`) | déf 1, immédiat | = | = (vide aussi la liste des dlights du traceur) | = | Utile user | Oui (immédiat) | Lumières des tirs et des sabres ; gros impact sur les perfs. Fonctionne partout. |
| **[KO]** Dynamic Glow / Brillance dynamique (`r_DynamicGlow`) | déf 0, immédiat (forcé à 0 si le GPU n'a pas les extensions) | déf 0 | déf 0, **latch**, **exige `r_fbo 1`** | sans effet : le halo RTX vient de `pt_bloom` | Utile user | Oui (pas d'Appliquer) | Halo des sabres et lumières, signature visuelle de JKA. Sur Vulkan : sans effet tant que `r_fbo` vaut 0, et pas pris en compte sans `vid_restart`. Correctif : l'action met `r_fbo 1` et `ui_r_modified 1` quand `cl_renderer` = rdsp-vulkan (ou rd-vulkan active le FBO quand le glow est demandé). |
| Light Flares / Éclats lumineux (`r_flares`) | déf 1 | déf 0 | déf 1 | lu par le traceur (chemin transparence) | Utile user | Oui (immédiat) | Halos des sources lumineuses. Fonctionne partout. |
| Wall Marks / Marques murales (`cg_marks`) | cgame 0/1 (déf 1) | = | = | = (non vérifié en jeu sous RTX) | Utile user | Oui (immédiat) | Impacts et brûlures sur les murs ; coût CPU faible. Fonctionne partout. |

### Options à ajouter

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Anti-crénelage MSAA (`r_ext_multisample`) | 0/2/4/8, latch (tampon par défaut, `r_ext_multisample_default_fb 1`) | 0/2/4/8, latch (FBO) | 0..64 (déf 0), latch | forcé off (le traceur fait son TAA) | Utile user | Partiel (onglet REND2 seulement) | L'option la plus demandée après la résolution, et elle fonctionne sur les 3 renderers raster. Déplacer l'item de REND2 page 1 vers QUALITY. Widget : multi Off/2x/4x/8x, Appliquer. |
| Herbe et végétation (`r_surfaceSprites`) | déf 1 | déf 1 | déf 1, latch | lu par le traceur | Utile user | Non | L'herbe dense de certaines cartes coûte cher ; l'option permet de gagner des FPS. Widget : multi Off/On, Appliquer. |
| Ciel simplifié (`r_fastsky`) | déf 0 | = | = (immédiat) | =vk | Utile user | Indirect (préréglage) | Gain sur cartes à skybox lourde. Déjà réglé par le préréglage sans item visible, donc le joueur ne peut pas le remettre. Widget : multi Off/On. |
| Intensité du glow (`r_DynamicGlowIntensity`) | déf 1.13 | déf 1.13 | déf 1.13 (exige `r_fbo 1`) | sans effet | Utile user | Non | Le glow par défaut est jugé trop fort ou trop faible selon les cartes ; une intensité réglable suffit (les autres `r_DynamicGlow*` restent debug). Widget : curseur 0.5..2, désactivé si `r_DynamicGlow` 0. |
| Éclairage par sommet (`r_vertexLight`) | déf 0, latch | = | = | chargement vertex forcé sous RTX | Utile user (bas de gamme) | Non | Seule solution pour les GPU très faibles (pas de lightmaps). Widget : multi Off/On, Appliquer. Priorité basse. |

---

## 3. VIDEO > OPENGL

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Shadows / Ombres (`cg_shadows`) | 0 aucune, 1 simple (blob), 2 volumétriques (stencil), 3 projetées | (onglet caché) | (onglet caché) | (onglet caché) | Utile user | Oui (immédiat) | Les 4 valeurs correspondent au code rd-vanilla. Correct ici. |

### Options à ajouter

Aucune propre à OpenGL. Le MSAA va dans QUALITY. L'onglet reste utile pour les ombres 0..3, qui diffèrent de Rend2 (0..4).

---

## 4. VIDEO > REND2, page 1/2

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Shadows / Ombres (`cg_shadows`) | (onglet caché) | 0..3 + 4 Shadow Map | (onglet caché) | (onglet caché) | Utile user | Oui (immédiat) | Les ombres par shadow map (4) sont l'atout de Rend2. Attention : la cvar est commune. Avec 4 puis un passage à Vulkan, les modèles n'ont plus d'ombre (Vulkan gère 0..3 seulement). |
| HDR (`r_hdr`) | n/a | déf 1, latch | (même nom, autre sens : précision 16 bits du FBO) | =vk | Utile user | Oui (Appliquer ; affichage latch) | Active le rendu HDR, il conditionne les trois items suivants. Cvar partagée : 0 ici passe aussi le tampon Vulkan en 8 bits. |
| Tonemapping (`r_toneMap`) | n/a | déf 1, latch ; désactivé si `r_hdr` 0 | n/a | n/a | Utile user | Oui (Appliquer ; affichage latch) | Sans tone mapping, le HDR sature. Fonctionne. |
| Autoexposure / Exposition auto (`r_autoExposure`) | n/a | déf 1 ; désactivé si `r_hdr` 0 | n/a | n/a | Utile user | Oui (Appliquer) | Adapte l'exposition entre intérieur et extérieur. Lu à chaque image avec le tone mapper. Fonctionne. |
| HDR Lighting support (`r_floatLightmap`) | n/a | déf 0, latch ; effectif seulement si `r_hdr` 1 | n/a | n/a | Utile user | Oui (Appliquer ; affichage latch) | Lightmaps flottantes ; ne sert que sur des cartes compilées en HDR. Fonctionne. |
| Anti-aliasing (MSAA) (`r_ext_multisample`) | (marche aussi) | 0/2/4/8, latch | (marche aussi) | forcé off | Utile user | Oui (Appliquer ; affichage latch) | Fonctionne ici, mais l'item est au mauvais endroit : le déplacer vers QUALITY (voir §2). |
| SSAO (`r_ssao`) | n/a | 0/1 (déf 0), latch ; 2 = vue debug | (même nom : 0..2, 2 = GTAO) | n/a | Utile user | Oui (Appliquer ; affichage latch) | Occlusion ambiante, gain de relief net. 0/1 correspond au code Rend2. |
| Glow Bloom (`r_dynamicGlowBloom`) | n/a | 0..2 (déf 0), immédiat ; désactivé si `r_dynamicGlow` 0 | n/a | n/a | Utile user | Oui (immédiat) | Étend le glow en bloom. Fonctionne. |
| Depth Prepass (`r_depthPrepass`) | n/a | déf 1, non latch | (même nom : déf 0, latch, exige `r_fbo`) | n/a | Debug (optimisation interne) | Oui (Appliquer) | Évite le surdessin. Utile au joueur seulement pour tester un gain de perf ; à laisser, mais il pourrait passer en exclusion. |

## 5. VIDEO > REND2, page 2/2

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Deluxemapping (`r_deluxeMapping`) | n/a | déf 1, latch | (même nom, PBR Vulkan) | =vk | Utile user | Oui (Appliquer ; affichage latch) | Relief des surfaces statiques avec les cartes deluxe. Fonctionne. |
| **[KO]** Deluxemap Specular (`r_deluxeSpecular`) | n/a | curseur 0..1 (déf 1), **latch** ; désactivé si deluxe 0 | (même nom) | =vk | Utile user | Oui (**pas d'Appliquer**) | Dose le spéculaire des deluxe maps. L'action ne met pas `ui_r_modified` : la valeur reste en attente jusqu'à un `vid_restart` venu d'ailleurs, et le curseur revient à l'ancienne valeur. Ajouter `setcvar ui_r_modified 1`. |
| Cubemapping (`r_cubeMapping`) | n/a | déf 0, latch | (même nom : exige PBR + `r_fbo`, off sous RTX) | forcé off | Utile user | Oui (Appliquer ; affichage latch) | Reflets des sondes de la carte. Fonctionne. |
| Cubemapping bounces (`r_cubeMappingBounces`) | n/a | 0..2 (déf 0), latch ; désactivé si cubemap 0 | n/a | n/a | Utile user | Oui (Appliquer ; affichage latch) | Reflets dans les reflets ; coût de chargement seulement. Fonctionne. |
| Parallax Occlusion Mapping (`r_parallaxMapping`) | n/a | déf 0 | n/a | n/a | Utile user | Oui (Appliquer) | Relief par parallaxe ; utile seulement avec des matériaux Rend2. Fonctionne. |
| Generate Normalmaps (`r_genNormalMaps`) | n/a | déf 0, latch | (même nom : déf 0, latch, utilisé aussi par le traceur) | =vk | Utile user | Oui (Appliquer ; affichage latch) | Relief approché sur les textures d'origine sans carte de normales. Fonctionne. |

### Options à ajouter (nouvelle page REND2 3/3 "Ombres et post-traitement")

La page 2 a encore 5 rangées libres (296 -> 391), mais les ombres du soleil forment un groupe ; une page 3 garde chaque page lisible.

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Ombres du soleil (`r_sunShadows`) | n/a | déf 1, latch ; actif avec `cg_shadows 4` et un ciel `q3gl2_sun` | n/a | n/a | Utile user | Non | Ombres du soleil en cascades, le gain visuel majeur de Rend2 en extérieur. Widget : multi Off/On, visible si `cg_shadows` 4 (cvarTest), Appliquer. |
| Taille des shadow maps (`r_shadowMapSize`) | n/a | déf 1024, latch | n/a | n/a | Utile user | Non | Compromis netteté/VRAM des ombres. Widget : multi 512/1024/2048/4096, Appliquer. |
| Filtrage des ombres (`r_shadowFilter`) | n/a | 0..2 (déf 1), latch | n/a | n/a | Utile user | Non | Bords d'ombre doux ou nets. Widget : multi Off/Normal/Doux, Appliquer. |
| Ombres des lumières dynamiques (`r_dlightMode`) | n/a | déf 1, latch ; >= 2 = shadow maps des dlights | (même nom, autre échelle 0..2) | n/a | Utile user | Non | Ombres des tirs et sabres ; coûteux, donc réglable. Widget : multi Off (1)/On (2), Appliquer. |
| SMAA (`r_smaa`) | n/a | 0/1 (déf 0), latch ; 2 = vue debug | n/a | n/a | Utile user | Non | Anti-crénelage post-traitement, moins cher que le MSAA. Widget : multi Off/On, Appliquer. |
| Qualité SMAA (`r_smaa_quality`) | n/a | 0..3 (déf 2), latch | n/a | n/a | Utile user | Non | Dose le coût du SMAA. Widget : multi Basse..Ultra, désactivé si `r_smaa` 0. |
| Brouillard volumétrique (`r_volumetricFog`) | n/a | déf 0, latch | n/a | n/a | Utile user | Non | Brouillard éclairé par la grille de lumière ; effet d'ambiance fort. Widget : multi Off/On, Appliquer. |
| Rayons du soleil (`r_drawSunRays`) | n/a | déf 0, latch | n/a | n/a | Utile user | Non | God rays en espace écran. Widget : multi Off/On, Appliquer. |
| Normal mapping (`r_normalMapping`) | n/a | déf 1, latch | (même nom : interrupteur PBR Vulkan) | =vk | Utile user | Non | Le joueur doit pouvoir couper le relief sur GPU faible. Widget : multi Off/On, Appliquer. |
| Specular mapping (`r_specularMapping`) | n/a | déf 1, latch | (même nom : PBR Vulkan) | =vk | Utile user | Non | Idem pour le spéculaire. Widget : multi Off/On, Appliquer. |

---

## 6. VIDEO > VULKAN (vide aujourd'hui)

### Options présentes

Aucune. Le sous-onglet s'affiche quand `cl_renderer` = rdsp-vulkan, mais `pg_video_vk` ne contient aucun item. Le joueur Vulkan n'a donc ni ombres, ni MSAA, ni post-traitement, ni PBR, ni ray tracing dans le menu.

### Options à ajouter : page VULKAN 1/2 "Rendu"

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Ombres (`cg_shadows`) | (voir OpenGL) | (voir Rend2) | 0 aucune, 1 simple, 2 volumétriques (stencil z-fail), 3 projetées | 1 dessiné par cgame ; 2/3 sans objet (ombres tracées, non vérifié en jeu) | Utile user | Partiel (onglets OpenGL/Rend2, cachés sous Vulkan) | Le joueur Vulkan ne peut pas régler les ombres alors que rd-vulkan gère 0..3. Widget : multi 0..3 (pas de 4). Immédiat. |
| Post-traitement (`r_fbo`) | n/a | n/a | déf 0, latch : conditionne glow, bloom, SSAO, prepass, supersample, renderScale, dither, `r_presentBits` | =vk | Utile user | Non | Interrupteur maître : sans lui, "Dynamic Glow" et tout ce qui suit sont sans effet. Widget : multi Off/On, Appliquer. Les items qui en dépendent sont désactivés (`disableCvar`) quand il vaut 0. Mieux : le passer à 1 par défaut. |
| Bloom (`r_bloom`) | n/a | n/a | déf 0, latch, exige `r_fbo` | sans objet (RTX : `pt_bloom`) | Utile user | Non | Lueur des zones claires, en plus du glow. Widget : multi Off/On, Appliquer. |
| Intensité du bloom (`r_bloom_intensity`) | n/a | n/a | 0.01..2 (déf 0.15), latch | n/a | Utile user | Non | Le seul réglage du bloom qui parle au joueur (le seuil reste debug). Widget : curseur, désactivé si `r_bloom` 0, Appliquer. |
| Occlusion ambiante (`r_ssao`) | n/a | (0/1) | 0 off, 1 SSAO, 2 GTAO ; latch ; exige `r_depthPrepass 1` + `r_fbo 1` | sans objet | Utile user | Non (Rend2 seulement, 0/1) | Relief des coins et des contacts ; GTAO est la meilleure valeur. Widget : multi Off/SSAO/GTAO, dont l'action met aussi `r_depthPrepass 1`, ce qui garde le prepass hors menu. Appliquer. |
| Ombres de contact (`r_contactShadows`) | n/a | n/a | 0/1 (déf 0), exige `r_ssao 2` | sans objet | Utile user | Non | Ombres fines sous les pieds et les objets au soleil. Widget : multi, visible si `r_ssao` 2. |
| Lumières dynamiques (`r_dlightMode`) | n/a | (autre échelle) | 0 legacy, 1 par pixel, 2 par pixel + ombres stencil (déf 2) | sans objet | Utile user | Non | Le mode 2 coûte cher avec beaucoup de tirs ; 1 est un bon compromis. Widget : multi 3 valeurs. |
| Suréchantillonnage (`r_ext_supersample`) | n/a | n/a | 0/1 (déf 0), latch, exige `r_fbo` | sans objet | Utile user | Non | Anti-crénelage de qualité maximale pour les GPU puissants. Widget : multi Off/On, Appliquer. |
| Échelle de rendu (`r_renderScale`) | n/a | n/a | 0 off, 1..4 (plus proche/linéaire, étiré/bandes) ; latch ; exige `r_fbo` | ignoré (le traceur rend à 100 %) | Utile user | Non | Rendu à résolution réduite pour gagner des FPS. Widget : multi 5 valeurs, Appliquer. |
| Résolution de rendu (`r_renderWidth`, `r_renderHeight`) | n/a | n/a | déf 800x600, latch | ignoré | Utile user | Non | Va avec l'échelle de rendu. Widget : deux champs, visibles si `r_renderScale` != 0. |

### Options à ajouter : page VULKAN 2/2 "Matériaux et GPU"

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Normal mapping PBR (`r_normalMapping`) | n/a | (déf 1) | déf 0, latch : active le PBR | =vk (le traceur lit les normales) | Utile user | Non | Le PBR Vulkan est entièrement porté mais le joueur ne peut pas l'activer. Widget : multi Off/On, Appliquer. |
| Specular mapping PBR (`r_specularMapping`) | n/a | (déf 1) | déf 0, latch : active le PBR | =vk | Utile user | Non | Idem pour le spéculaire. Widget : multi Off/On, Appliquer. |
| Cartes de normales générées (`r_genNormalMaps`) | n/a | (onglet Rend2) | déf 0, latch | =vk (relief aussi pour le traceur) | Utile user | Partiel (onglet Rend2 seulement) | Relief sur les textures d'origine. Widget : multi Off/On, Appliquer. |
| Deluxe mapping (`r_deluxeMapping`) | n/a | (onglet Rend2) | déf 1, latch, effectif avec le PBR | =vk | Utile user | Partiel (onglet Rend2 seulement) | Direction de la lumière des lightmaps. Widget : multi, désactivé si PBR off. |
| Cubemaps (`r_cubeMapping`) | n/a | (onglet Rend2) | déf 0, latch, exige PBR + `r_fbo` | forcé off | Utile user | Partiel (onglet Rend2 seulement) | Reflets raster. Widget : multi, caché si `r_rtx` 1. |
| Niveau d'anisotropie (`r_ext_max_anisotropy`) | n/a | n/a | 1..16 (déf 2), latch | =vk | Utile user | Non | Nécessaire tant que rd-vulkan ignore le niveau du curseur QUALITY. Widget : multi 2x/4x/8x/16x, Appliquer. À retirer si le code est corrigé (préférable). |
| Sortie 10 bits (`r_presentBits`) | n/a | n/a | 16..30 (déf 24), latch ; 30 = 10 bits, exige `r_fbo` | =vk | Utile user | Non | Moins de bandes de couleur sur les écrans 10 bits. Widget : multi 24/30, Appliquer. |
| Tramage (`r_dither`) | n/a | n/a | 0/1 (déf 0), exige `r_fbo` | =vk | Utile user (mineur) | Non | Atténue les bandes en 8 bits. Widget : multi Off/On. Immédiat. |
| Cache GPU du monde (`r_vbo`) | n/a | n/a | déf 0, latch | =vk | Utile user (perf) | Non | Gain CPU net sur les grandes cartes. Widget : multi Off/On, Appliquer. |
| Carte graphique (`r_device`) | n/a | n/a | -1 GPU dédié (déf), -2 intégré, 0..8 index ; latch | =vk | Utile user | Non | Indispensable sur les portables à deux GPU. Widget : multi Dédié/Intégré/0/1, Appliquer. |
| Ray tracing (`r_rtx`) | n/a | n/a | 0/1 (déf 0), latch ; `r_rtxActive` (ROM) donne le résultat | bascule | Utile user | Non | Seul moyen pour un joueur d'activer le traceur. Widget : multi Off/On, Appliquer. Afficher `r_rtxActive` (texte "Actif / Non supporté") pour expliquer un échec. |

---

## 7. VIDEO > VULKAN > RAY TRACING (pages proposées)

Pourquoi des pages dans l'onglet VULKAN et pas un 4e sous-onglet : un item n'a qu'un `cvarTest`. Un onglet testé sur `r_rtx` apparaîtrait aussi sous OpenGL quand `r_rtx` reste archivé à 1. Placer les pages RTX après VULKAN 2/2 les garde sous le test `cl_renderer` = rdsp-vulkan ; le bouton "page suivante" teste `r_rtx` (showCvar 1). La barre de sous-onglets n'a de toute façon pas la place d'un 4e bouton de 130 unités.

Prérequis commun : les cvars UBO (`pt_*`, `flt_*`, `tm_*` de `global_ubo.h`, enregistrées `CVAR_NONE`) et `gr_*` (drapeau 0) ne sont pas archivées. Ajouter `CVAR_ARCHIVE` aux cvars exposées, sinon un choix fait dans le menu est perdu au lancement suivant (`pt_bloom*`, `tm_contrast`, `tm_per_channel` sont déjà `CVAR_ARCHIVE_ND`).

### Options à ajouter : page RTX 1/2 "Qualité du tracé"

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| *Groupe éclairage tracé (`pt_*` de qualité)* | | | | | | | Ces cvars règlent le compromis coût/qualité du tracé ; c'est l'équivalent RTX des préréglages. |
| Rebonds de lumière (`pt_num_bounce_rays`) | n/a | n/a | n/a | 0 off, 0.5 demi-rés., 1 (déf), 2 | Utile user | Non | Le réglage qui a le plus d'impact sur les perfs RTX. Widget : multi Off/Demi/1/2. Immédiat (UBO). |
| Reflets et réfractions (`pt_reflect_refract`) | n/a | n/a | n/a | 0, 1, 2 (déf 2) | Utile user | Non | Profondeur des reflets (eau, verre, sols brillants). Widget : multi 0/1/2. |
| Caustiques (`pt_caustics`) | n/a | n/a | n/a | 0/1 (déf 1) | Utile user | Non | Lumière focalisée par l'eau et le verre ; coûteux. Widget : multi Off/On. |
| Verre épais (`pt_thick_glass`) | n/a | n/a | n/a | 0 off (déf), 1 référence, 2 temps réel | Utile user | Non | Réfraction physique du verre. Proposer seulement 0/2 (1 = mode référence). |
| *Groupe anti-crénelage (`flt_taa`)* | | | | | | | Seule cvar `flt_*` utile au joueur ; les autres sont des réglages internes du débruiteur (voir exclusions). |
| Anti-crénelage temporel (`flt_taa`) | n/a | n/a | n/a | 0 off, 1 TAA (déf), 2 upscale (= TAA, rendu fixe à 100 %) | Utile user | Non | Remplace le MSAA, forcé off sous RTX. Proposer 0/1 seulement : 2 n'apporte rien tant que le rendu reste à 100 %. |
| *Groupe post-traitement RTX (`pt_bloom*`, `gr_*`)* | | | | | | | Effets d'image du traceur, équivalents RTX du glow/bloom raster. |
| Bloom RTX (`pt_bloom`) | n/a | n/a | n/a | 0/1 (déf 1) | Utile user | Non | Seule source de halo sous RTX ("Dynamic Glow" y est sans effet). Widget : multi Off/On. |
| Intensité du bloom RTX (`pt_bloom_intensity`) | n/a | n/a | n/a | déf 1.0 | Utile user | Non | Dose le halo des sabres et lumières. Widget : curseur 0..2, désactivé si `pt_bloom` 0. |
| Rayons divins (`gr_enable`) | n/a | n/a | n/a | 0/1 (déf 1) | Utile user | Non | Effet d'ambiance fort en extérieur, coût notable. Widget : multi Off/On. |
| Intensité des rayons (`gr_intensity`) | n/a | n/a | n/a | déf 2.0 | Utile user | Non | Certaines cartes saturent. Widget : curseur 0..4, désactivé si `gr_enable` 0. |
| *Groupe tone mapper (`tm_exposure_bias`, `tm_contrast`, `tm_per_channel`)* | | | | | | | Les trois seuls `tm_*` qui correspondent à une préférence du joueur ; le reste du tone mapper est de la calibration. |
| Exposition (`tm_exposure_bias`) | n/a | n/a | n/a | déf -1.0 (log2) | Utile user | Non | Équivalent RTX de la luminosité : le tone mapper compense `r_gamma`. Widget : curseur -3..+1. |
| Contraste (`tm_contrast`) | n/a | n/a | n/a | déf 1 (archivé) | Utile user (mineur) | Non | Rend l'image plus ou moins dure. Widget : curseur 0.5..1.5. |

### Options à ajouter : page RTX 2/2 "Caméra"

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Couleurs par canal (`tm_per_channel`) | n/a | n/a | n/a | 0/1 (déf 1, archivé) | Utile user (mineur) | Non | Choix de style : couleurs saturées (1) ou blanchies vers le blanc (0). Widget : multi. |
| Profondeur de champ (`pt_dof`) | n/a | n/a | n/a | 0 off (déf), 1 accumulation, 2 sans débruiteur, 3 toujours | Utile user (mineur) | Non | Effet photo. Proposer 0/3 seulement (1/2 sont des modes de référence). Les `pt_focus`/`pt_aperture*` restent hors menu. |
| Projection (`pt_projection`) | n/a | n/a | n/a | 0 rectiligne (déf), 1 Panini, 2 stéréo, 3 cylindrique, 4 équirect., 5 Mercator | Utile user (mineur) | Non | Panini réduit la déformation avec un grand FOV. Proposer 0/1 seulement ; les autres sont des curiosités. |

---

## 8. Ingame > ANGLES (IngameSetup.menu)

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| **[KO]** Fov (`cg_fov`) | cgame : déf 80, borné 1..160 ; curseur 0..150 | = | = | = (aussi `pt_projection`) | Utile user | Oui (IngameSetup seulement, immédiat) | Champ de vision, réglage de confort de base. Plage fausse : le curseur descend à 0 (borné à 1 par cgame, image inutilisable). Plage recommandée 60..130. Le libellé "Fov" n'est pas traduit. Les autres items de l'écran (`cg_thirdperson*`, `fixedtime`) sont de la caméra et du jeu, pas du graphisme. |

### Options à ajouter

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| FOV adapté à l'écran large (`cg_fovAspectAdjust`) | cgame : 0/1 (déf 0) | = | = | = | Utile user | Non | Avec 1, `cg_fov` est un FOV 4:3 élargi en Hor+ : même angle vertical en 16:9. Sans lui, le 16:9 rogne le haut et le bas. Widget : multi Off/On. |
| FOV de l'arme (`cg_fovViewmodel`) | cgame : 0 = suit `cg_fov` (déf) | = | = | = | Utile user | Non | Évite l'arme déformée avec un grand FOV. Widget : curseur 0 (auto), 60..110. |

---

## 9. GAME > DISPLAY (Jeu > Affichage)

Seuls les items graphiques sont listés ; réticule, HUD, souffle et barres de vie sont des options d'interface.

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Model Marks / Marques sur les modèles (`cg_g2Marks`) | cgame 0/1 (déf 1) ; gore ghoul2 | = | = (`G2_gore_r2`) | = (non vérifié en jeu sous RTX) | Utile user | Oui (immédiat) | Brûlures et impacts sur les personnages ; coût notable avec beaucoup de PNJ. Fonctionne sur les renderers raster. Sa place logique serait VIDEO > QUALITY, à côté de "Wall Marks". |
| Enable FPS Counter / Compteur de FPS (`cg_drawFPS`) | cgame 0/1 (déf 0) | = | = | = | Utile user | Oui (immédiat) | Outil indispensable pour régler les options vidéo. Fonctionne partout. |

### Options à ajouter

Aucune.

---

## 10. GAME > GENERAL (Jeu > Général)

### Options présentes

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| **[KO]** Screenshot Format / Format des captures (`ui_screenshotType`) | jpg/tga/png | = | = (`screenshot_png`/`_tga` présents) | = | Utile user | Oui (immédiat) | Le joueur pense régler le format de la touche de capture. En fait, la valeur ne sert qu'à F12 dans les menus et seulement avec `developer 1` (`ui_shared.cpp` 13686) ; la commande `screenshot` liée à une touche reste en JPEG. À relier à un alias (`bind` vers `screenshot_png`), ou à retirer. |

### Options à ajouter

| Option | Valeurs OpenGl | Valeurs Rend2 | Valeurs Vulkan | Valeurs Vulkan RTX | Utile user / debug | Présente dans le menu | Justification |
|---|---|---|---|---|---|---|---|
| Qualité JPEG (`r_screenshotJpegQuality`) | déf 95 | déf 90 | déf 100 | =vk | Utile user (mineur) | Non | Seulement si le format de capture devient réel. Widget : multi 75/90/100. Priorité basse. |

---

## Options volontairement exclues

Le comptage de 443 cvars de la version précédente reste valable. Les familles ci-dessous restent hors menu. Pour chacune : la raison en une ligne.

| Famille (exemples) | Renderers | Raison de l'exclusion |
|---|---|---|
| Ghoul2 / ragdoll (`broadsword*`, `r_ghoul2*`, `r_noGhoul2`, `r_g2_shadow*`) | OpenGL, Rend2, Vulkan | Réglages d'animation et de débogage du moteur ; aucun bénéfice visuel pour le joueur. |
| Vent / météo (`r_wind*`, `r_surfaceWeather`, `r_debugWeather`) | OpenGL, Vulkan (Rend2 partiel) | Pilotés par les cartes et les scripts ; un réglage joueur casserait les effets prévus. |
| Vues de debug (`r_show*`, `r_debug*`, `r_speeds`, `r_lightmap`, `r_fullbright`, `r_showGBuffer`, `pt_debug_*`, `pt_nrd_validation`, `flt_show_gradients`, `flt_fixed_albedo`, `tm_debug`) | tous | Visualisations pour développeur, beaucoup sont `CVAR_CHEAT`. |
| Désactivation du pipeline (`r_no*`, `r_draw*`, `r_lockpvs`, `r_portalOnly`, `r_skipBackEnd`, `r_offset*`, `r_terrain*`, `r_finish`) | tous | Coupent des parties du rendu pour diagnostiquer ; image incorrecte par nature. |
| Extensions GL / formats internes (`r_allowExtensions`, `r_ext_*` hors menu, `r_arb_*`, `r_ignoreGLErrors`, `r_stencilbits`, `r_depthbits`, `r_defaultImage`, `r_ignorehwgamma`) | tous | Compatibilité matérielle ; les valeurs par défaut sont les bonnes, et plusieurs sont mortes (`r_gammaShaders`, `r_ext_compress_lightmaps`, cvars Rend2 marquées "Unused"). |
| Images / mips (`r_simpleMipMaps`, `r_roundImagesDown`, `r_colorMipLevels`, `r_imageUpsample*`, `r_nomip`, `r_base*`, `r_greyscale`) | tous | Débogage du chargement des textures ; `r_picmip` et `r_texturebits` suffisent au joueur. |
| Éclairage legacy (`r_overBrightBits`, `r_mapOverBrightBits`, `r_intensity`, `r_ambientScale`, `r_directedScale`, `r_dlightScale/Intensity/Saturation/Style`) | tous | Calibration de l'éclairage d'origine ; les changer fausse le rendu des cartes. `r_gamma` couvre le besoin du joueur. |
| Réglages fins du glow (`r_DynamicGlowPasses/Delta/Soft/Width/Height/Scale/AllStages`) | tous | Seule l'intensité est proposée ; le reste est de la calibration du flou. |
| Réglages fins SSAO / G-buffer (`r_ssaoRadius/Intensity/Slices/Steps`, `r_contactShadow*` sauf l'interrupteur, `r_velocityBuffer`, `r_depthPrepass` Vulkan) | Vulkan | Calibration interne ; le prepass est mis automatiquement par l'item SSAO proposé. |
| Réglages fins flares / brouillard / distorsion / ombres (`r_flareSize/Fade/Coeff`, `r_drawfog`, `r_distortionStyle`, `r_distanceCull`, `r_pshadowDist`, `r_shadowCascadeZ*`, `r_shadowOffset*`, `r_sunlightMode`) | tous | Réglages par carte ou techniques ; les valeurs par défaut correspondent au jeu d'origine. |
| LOD / géométrie (`r_lodscale`, `r_lodCurveError`, `r_znear`, `r_maxpolys`, `r_maxpolyverts`, `r_markcount`, `r_modelpoolmegs`, `r_patchStitching`) | tous | Limites mémoire et précision ; `r_lodbias` couvre le besoin du joueur. |
| Forçages Rend2 (`r_force*`, `r_cameraExposure`, `r_saveFontData`) | Rend2 | Surchargent le tone mapper pour tester ; contredisent "Tonemapping" et "Autoexposure". |
| Divers fenêtre / système (`r_stereo`, `r_anaglyphMode`, `r_allowSoftwareGL`, `r_centerWindow`, `r_inGameVideo`, `mapname`, `se_language`) | partagés | Usage marginal ou interne ; `r_inGameVideo` est déjà géré par le préréglage. |
| Expérimental Vulkan (`r_vbo_models`, `r_hdr` Vulkan, `r_ext_alpha_to_coverage`, `r_bloom_threshold`, `r_bloom_modulate`, `r_bloom_threshold_mode`) | Vulkan | `r_vbo_models` regroupe le skinning GPU et le VBO MD3 (surfaces manquantes signalées, à scinder d'abord). `r_hdr` n'est qu'une précision de tampon. Les autres sont de la calibration. |
| Capture vidéo (`r_aviMotionJpegQuality`) | tous | Outil d'enregistrement pour développeur. |
| **RTX : débruiteur A-SVGF / TAA** (`flt_*` sauf `flt_taa`, 28 cvars) | RTX | Paramètres internes du filtre (anti-lag, a-trous, historique) ; une valeur fausse produit du grain ou des traînées, sans gain pour le joueur. |
| **RTX : choix d'algorithme** (`pt_denoiser`, `pt_restir`, `pt_restir_gi`, `pt_rc_enable`) | RTX | Expérimental : NRD, ReSTIR GI et le cache de radiance ne battent pas A-SVGF + ReSTIR (mesure du 2026-10-01). Les défauts actuels sont les bons. |
| **RTX : NRD** (`pt_nrd_*`, 6) et **cache de radiance** (`pt_rc_*`, 8) | RTX | Ne servent que pour les algorithmes expérimentaux ci-dessus. |
| **RTX : ReSTIR** (`pt_restir_m_clamp`, `pt_restir_spatial*`, `pt_restir_gi_*`, 7) | RTX | Calibration de l'échantillonnage ; affecte le bruit et le biais, pas une préférence du joueur. |
| **RTX : calibration des lumières** (`pt_light_scale_edit`, `pt_light_scale_dlight`, `pt_light_scale_saber`, `pt_lightgen_scale`, `pt_dlight_*`, `pt_glow_scale`, `pt_beam_*`, `pt_particle_*`, `pt_explosion_brightness`, `pt_weapon_fx*`, 22+) | RTX | Calibration par le développeur pour accorder le tracé au raster ; un réglage joueur casse l'équilibre des cartes. |
| **RTX : échantillonnage / matériaux** (`pt_direct_*`, `pt_indirect_*`, `pt_specular_*`, `pt_sun_*`, `pt_toksvig`, `pt_bump_scale`, `pt_*_override`, `pt_texture_lod_bias`, `pt_water_density`, 22) | RTX | Interrupteurs de diagnostic et réglages physiques ; les surcharges de rugosité et de métal sont des outils de test. |
| **RTX : tone mapper avancé** (`tm_*` hors `tm_exposure_bias`, `tm_contrast`, `tm_per_channel` ; `tm_hdr_*`, `ui_hdr_nits`) | RTX | Calibration de l'adaptation et de la courbe. `tm_hdr_*` est sans sortie HDR10 dans ce port, donc trompeur dans un menu. |
| **RTX : ciel et soleil** (`physical_sky*`, `sun_*`, `sky_*`, `sun_preset`, `pt_sky_per_map`, `pt_accumulation_rendering*`) | RTX | Réglés par carte (`pt_sky_per_map` écrase `physical_sky`) ; l'accumulation est un mode de référence photo, pas un mode de jeu. |

## Comptes

- Écrans de menu passés en revue : **10** (VIDEO > DISPLAY, QUALITY, OPENGL, REND2 1/2, REND2 2/2, VULKAN, Ingame > ANGLES, GAME > DISPLAY, GAME > GENERAL, et l'écran proposé VULKAN > RAY TRACING). Pages nouvelles proposées : REND2 3/3, VULKAN 1/2 et 2/2 (dans l'onglet vide), RTX 1/2 et 2/2.
- Options présentes passées en revue : **39** (DISPLAY 8, QUALITY 11, OPENGL 1, REND2 15, ANGLES 1, GAME DISPLAY 2, GAME GENERAL 1). Marquées **[KO]** : **10** (préréglage, renderer, profondeur de couleur, sync vidéo, anisotropie, textures compressées, glow dynamique, deluxemap specular, FOV, format de capture). En plus, 10 items latch affichent leur ancienne valeur jusqu'au redémarrage (constat 3).
- Ajouts proposés : **58** (DISPLAY 6, QUALITY 5, REND2 10, VULKAN 21, RTX 14, ANGLES 2) ; plus `r_screenshotJpegQuality` en option conditionnelle.
