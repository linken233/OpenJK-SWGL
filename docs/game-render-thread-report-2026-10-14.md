# Rapport : thread de rendu et pool de jobs (nuit du 13 au 14/10/2026)

Branche `feature/game-render-thread`, 12 commits au-dessus de `0438185be` (outils perflog). Rien n'est fusionné dans `develop`.

## Conditions de mesure
- Save `stress180` : 184 PNJ en combat, map korriban_academy.
- Release, RTX actif.
- CPU plafonné à 50 % (2 350 MHz), affinité 12 cœurs, `com_jobThreads 10`.
- Chaque partie dure ~1 500 frames. Les chiffres sont des moyennes par frame, chargement exclu.

## Résultat
Fps moyens et écart entre parties :

| Configuration | Parties | Fps moyen | Écart |
|---|---|---|---|
| Tout coupé (`r_renderThread 0`, `com_jobThreads 0`) | 2 | **17,6** | 16,5 à 18,7 |
| Jobs seuls (`r_renderThread 0`) | 7 | **20,9** | 18,1 à 24,2 |
| Jobs + thread de rendu (`r_renderThread 1`) | 7 | **26,0** | 19,0 à 28,7 |

Le gain total est de **+48 %** par rapport à tout coupé. Le handoff mesurait 14 fps à 2,5 GHz avant la branche, mais à un plafond un peu différent (53 % contre 50 %).

## Ce qui limite maintenant : le GPU
Le temps GPU est mesuré par des timestamps Vulkan, ajoutés cette nuit. Il vaut ~31 à 34 ms par frame :
- path tracing et débruitage (`gpurt`) : ~28 ms ;
- BLAS/TLAS (`gpuaccel`) : ~3 ms ;
- raster : 0,3 ms.

Le plafond GPU est donc d'environ 30 fps. Avec le thread de rendu, le jeu atteint 27 à 28,6 fps, soit ~90 % de ce plafond. L'attente de fence (6 à 12 ms) est le GPU qui travaille, pas un bug.

**Les écarts entre parties viennent du GPU.** Une partie sans thread a mesuré 47,8 ms de GPU par frame contre 31 ms pour les autres. La partie lente avec thread (19 fps, fence 33 ms) avait sans doute la même cause. Je n'ai pas cherché ce qui rend certaines parties plus lourdes (position de la caméra, nombre de lumières dynamiques, état du combat).

Présentation : FIFO (vsync) à 144 Hz, 3 images. Ça ne coûte rien à ces fps.

## Corrigé cette nuit
1. **Synchro du gore** : le destructeur `~R2GoreTextureCoordinates` attendait le thread de rendu jusqu'à 5 ms par frame. La mémoire est maintenant libérée en différé, deux frames plus tard (`67082b831`). Il ne reste aucune synchro avec attente pendant le jeu (`rtsync` 0).
2. **Trop de workers** : le pool lançait 14 workers sur 12 cœurs autorisés, ce qui volait du temps au thread de rendu. Le pool compte maintenant les CPU de l'affinité du processus (`1a5624bf3`).
3. **Nouveaux outils** :
   - `r_renderThreadSyncLog 1` : liste les synchros par endroit du code ;
   - perflog : temps GPU, temps CPU réel du backend, mode de présentation, état de la connexion.

## Problèmes ouverts
1. **Chargement bloqué, 1 fois sur 29 parties avec le thread.**
   - Après le `vid_restart` du chargement, le client reste dans le menu : 0 frame serveur, pas de cgame. Pas de crash, pas d'erreur.
   - Pas reproduit en 24 chargements suivants. La lecture du code ne trouve aucun lien avec le thread de rendu.
   - Des traces de connexion (`connection: client …`) sont en place pour la prochaine fois (`25c74ffcc`). Avec 0 cas sur 6 sans le thread, on ne peut pas dire que le thread en est la cause.
2. **Plantage GPU sans RTX (`VK_ERROR_DEVICE_LOST`), indépendant de la branche.**
   - Avec `r_rtx 0`, chaque partie de `stress180` perd le device Vulkan 1 à 2 s après l'entrée en jeu.
   - Le build de départ `0438185be` le fait aussi : ce n'est pas une régression.
   - Les mesures sans RTX de cette nuit ne valent donc rien : le GPU ne dessinait plus.
   - Une tâche séparée est proposée pour le corriger.
3. **Un compteur faux** : `rtsyncleft` affiche une valeur aberrante quand le thread est coupé. C'est cosmétique.
4. **Revue de code** : les points mineurs restants (zone lock partiel, cvars RTX différées d'une frame, file météo) sont dans la revue de l'agent. Ils n'ont pas d'effet mesuré.

## Prochaines étapes possibles
- Corriger le plantage sans RTX, puis mesurer le gain CPU en raster. Là, le GPU ne limite pas, et le thread devrait donner plus que +25 %.
- Réduire le coût GPU du path tracing (~28 ms) : c'est maintenant le plafond avec RTX.
- Étapes 6 et 7 du plan (traces de sabre en parallèle, conception A) : elles n'aident qu'en raster ou sur un GPU plus rapide, tant que le RTX plafonne à ~30 fps.
- Décider des valeurs par défaut. Je propose `com_jobThreads -1` (auto) tout de suite, et `r_renderThread 1` après plus de parties sans blocage de chargement.

## Fichiers
- Script de mesure : `bench.ps1` dans le scratchpad de la session. Il plafonne le CPU puis le restaure ; la restauration a été vérifiée (PROCTHROTTLEMAX = 0x64).
- CSV et journaux : `results/` dans ce même scratchpad.
