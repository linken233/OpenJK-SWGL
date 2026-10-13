# Handoff : jeu et rendu sur des threads séparés (2026-10-13)

Branche de départ `develop` (aucune branche créée). Rien n'est commité. Suggestion : `feature/game-render-thread`. Les builds Debug et Release du moteur et de `rdsp-vulkan` passent avec les changements ci-dessous. Le jeu a été lancé en Release (instance isolée) pour les mesures.

## Objectif
Savoir si un PC « moyen » (CPU 2,5 GHz, 4 threads ou plus) peut faire tourner le jeu avec 190 PNJ, et doubler les fps si possible. Le moteur est entièrement mono-thread : aucune création de thread dans `code/` hors génération de PVS et outil de shaders.

## Mesures (save `stress180`, 184 PNJ en combat, RTX, Release)

| | Sans plafond | Plafonné 2,5 GHz, 12 threads |
|---|---|---|
| Fps | ~37 | ~14 |
| Frame | 27 ms | 70 ms (p95 86, pire 88) |
| Jeu, par frame | 6,5 ms (tourne 16 frames sur 30) | 31 ms (30 sur 30) |
| Client | 20 ms | 38 ms |
| dont backend du rendu | | 24,6 ms |
| dont scène (frontend) | | 3,8 ms |
| dont cgame | | ~8 à 12 ms |
| Attente GPU (fence, present) | | 0 ms : 100 % CPU |
| Charge | | 2 360 refEntities, 7 200 draw surfaces |

Le jeu tourne à fréquence fixe (20 Hz) : à 14 fps il tourne à chaque frame et rattrape parfois plusieurs pas. Charge du jeu par seconde ≈ 20 pas × 24 ms ≈ 480 ms de CPU. Restent ~520 ms pour le client : 38 ms × 14 fps. Pour 30 fps sur un seul thread, il faudrait un client à ~17 ms.

Profil du thread principal (sampler, plafonné), parts du thread : jeu 62 % (traces `SV_Trace` 47 %, dont monde `CM_BoxTrace` 35 %, sabres `WP_SaberDamageTrace` 23 %, `Pmove` 15 %, IA Jedi 11 %, cadavres `DeadThink` 7 %), client 38 % (backend 24 % dont `vk_rtx_begin_scene` 19 % et `prepare_entities` 11,5 %, cgame 13 %, scène 4,6 %). L'échantillonneur gonfle le jeu : `com_perflog` donne 43 % pour le jeu, le profil 62 %. Utiliser `com_perflog` pour les totaux.

Un cache de traces monde ne vaut rien : sur ~1 300 à 2 900 traces par frame, 0 à 10 % ont les mêmes arguments qu'une trace plus tôt dans la frame, 0 à 17 % les mêmes que la frame précédente.

## Fait (non commité)
1. **`com_perflog N`** : résumé toutes les `com_perflogFrames` frames (30 par défaut). `com_perflog 2` écrit aussi `perflog.csv` dans le homepath. Contient fps, moyenne, p95 et pire frame, travail CPU séparé en serveur (dont jeu), client, nombre de PNJ, et des statistiques du renderer. Fichiers :
   - `code/server/sv_perflog.cpp` (nouveau), appelé par `Com_Frame` (`code/qcommon/common.cpp`) et `SV_Frame` (`code/server/sv_main.cpp`, structure `svPerfGameScope`) ;
   - déclarations dans `code/qcommon/qcommon.h`, source ajoutée à `code/CMakeLists.txt`.
2. **API renderer** `code/rd-common/perf_stats_api.h` : extension `rdPerf_v1` obtenue par `re.GetExtension`. Implémentée seulement par `rdsp-vulkan` (`code/rd-vulkan/vk_perf.cpp`, `vk_perf.h`, accroches dans `tr_scene.cpp`, `tr_backend.cpp`, `tr_cmds.cpp`, `vk_frame.cpp`, `tr_init.cpp`, `tr_local.h`). Temps : scène, backend, attente de fence, acquire, submit, idle, present, enregistrement (`record` = backend moins les attentes). Compteurs : refEntities, dlights, draw surfaces, polys, remplissage du buffer de géométrie.
3. **Compteurs de traces** : `PerfLog_NoteTrace`, appelé au début de `SV_Trace` (`code/server/sv_world.cpp`) quand `com_perflog` est actif.
4. **`cpu_throttle.ps1`** (racine, non suivi) : plafonne la fréquence par `PROCTHROTTLEMAX` (2,5 GHz = 53 % de 4,7 GHz) et l'affinité, lance le jeu, restaure ensuite dans un `finally`. Usage : `.\cpu_throttle.ps1 -Mhz 2500 -Cores 12 -Exe <exe> -ExeArgs "<args>"`. Le réglage `PERFBOOSTMODE` est caché sur cette machine : le script ne le touche pas. Toujours vérifier après coup que `powercfg /query SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX` rend 0x64.

## Ce que le code impose

**Boucle principale** (`Com_Frame`) : `SV_Frame` (jeu) puis `CL_Frame` (cgame, `RE_RenderScene`, `RE_EndFrame`). Le backend du rendu tourne en ligne dans `RE_EndFrame` : `R_IssueRenderCommands` appelle `RB_ExecuteRenderCommands` directement. Il y a un seul `backEndData` (`tr_init.cpp:1403`) : pas de double tampon, malgré les commentaires hérités de Quake 3 sur le SMP.

**Pas de simulation en tranches du jeu** : `sv_frameSlices` répartit un pas de jeu sur plusieurs frames client (`G_RunFrameBegin`, `G_RunFrameSlice`). Cela lisse les saccades mais ne réduit pas le travail total.

**Couplage cgame ↔ jeu** : le cgame solo lit l'état du jeu directement. Environ 1 500 accès `->gent` (974 dans `cg_players.cpp`, 201 dans `cg_ents.cpp`) et 391 références à `g_entities` ou `ge->gentities`. Un thread de jeu séparé impose donc de copier cet état à la fin de chaque pas, ou de verrouiller.

**G2 partagé** : le jeu (traces de sabre, matrices de bolts) et le renderer (`CBoneCache::EvalRender`) utilisent les mêmes `CBoneCache`, cachés par compteurs de « touch ». Ce cache n'est pas thread-safe.

**Autres états globaux à vérifier** : `Z_Malloc` / `Z_Free`, `Com_Printf`, cvars, système de fichiers, enregistrement de shaders et de modèles (touche `tr.*` pendant que le backend tourne), upload du gore (`vk_flush_gore_uploads`), captures d'écran, `vid_restart`, chargement de map.

## Deux conceptions

| | A. Thread de jeu | B. Thread de rendu (backend) |
|---|---|---|
| Principe | Le pas de jeu tourne sur son propre thread, à 20 Hz. Le cgame lit une copie de l'état. | Le backend de la frame N tourne pendant le jeu et le cgame de la frame N+1. |
| Frame estimée (plafonné) | max(client 38 ms ; jeu ~24 ms sur son thread) ≈ 38 ms, ~26 fps | max(jeu + cgame + scène ≈ 44 ms ; backend 24,6 ms) ≈ 44 ms, ~23 fps |
| Les deux ensemble | cgame + scène ≈ 16 ms sur le thread principal, backend 24,6 ms, jeu 24 ms sur le sien : ~40 fps | |
| Point dur | Copie de l'état du jeu pour ~1 500 accès du cgame ; G2 partagé | `backEndData` à doubler ; G2 et gore partagés ; points de synchronisation (chargement, `vid_restart`, captures) |
| Latence | Une frame d'état en retard côté affichage | Une frame de plus |

Estimations arithmétiques sur les mesures ci-dessus, pas vérifiées en jeu.

## Pistes complémentaires (indépendantes des threads)
- Traces de sabre en parallèle : 5 à 8 ms, risque moyen. Les tests monde sont en lecture seule.
- `prepare_entities` et os G2 en parallèle : 5 à 10 ms.
- Traces d'inclinaison des pieds (`PM_FootSlopeTrace`, `pitch_roll_for_slope`) une frame sur N : ~3 ms.
- Cadavres : leur `ClientThink` pilote le ragdoll (`G_RagDoll` appelle `G2API_SetRagDoll`). Ne pas sauter leur think sauf une fois le ragdoll terminé ; je n'ai pas vérifié que G2 expose cet état.
- Cache de traces : abandonné (voir plus haut).

## Recommandation
1. **Auditer d'abord la sûreté des threads** : lister les accès de `RB_ExecuteRenderCommands` et de `vk_rtx_begin_scene` à des données que le jeu ou le cgame modifie pendant la frame suivante (G2, gore, `tr.*`, `ri.*`).
2. **Prototyper B** derrière un cvar (par exemple `r_renderThread`, désactivé par défaut) dans `rdsp-vulkan` seulement : double `backEndData`, un thread, une barrière à chaque point de synchronisation (map, `vid_restart`, shutdown, captures).
3. **Mesurer avec `com_perflog`** sur `stress180` plafonné, et comparer à 14 fps.
4. **Décider de A** avec ce résultat.

## Rappels de méthode (préférences de l'utilisateur)
- Ne jamais lancer le jeu sans demander ; tests en Release (asserts coupés), Debug seulement pour compiler ; builder Debug ET Release.
- Instance de test : `bin` du scratchpad avec l'exe et les DLL Release, les DLL d'exécution copiées de `SWGL_DEV/GameData`, jonctions `SWGL` et `base` vers `SWGL_DEV/GameData`, homepath isolé avec `SWGLSPconfig.cfg` de l'utilisateur et la save copiée dans `saves/`. Lancer avec `+set fs_basepath <bin> +set fs_homepath <home> +set cl_renderer rdsp-vulkan +set com_perflog 2 +load stress180`. Ne tuer que le PID lancé, jamais par nom.
- Le script de plafond doit être lancé directement (avec `run_in_background`), pas dans un `Start-Job` : le job meurt avec l'appel d'outil.
- Fichiers CRLF : pas de `sed -i` ni `perl -pi` sur des fichiers existants, utiliser l'outil Edit.
- Commentaires : une ou deux lignes, Simplified Technical English.
- Une fonctionnalité nouvelle : proposer un plan puis attendre un « go » explicite.
- Un agent neuf par itération.
