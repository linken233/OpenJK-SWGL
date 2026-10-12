# Mode « light edit » (RTX, solo)

Le mode light edit permet de placer, déplacer, orienter, régler et supprimer en jeu les lumières que le path tracer RTX de `rdsp-vulkan` utilise. Les changements sont visibles à l'image suivante. Ils sont enregistrés dans `maps/<map>.lgt`, qui est relu à chaque chargement de la map, en mode normal comme en mode édition.

Ce document décrit le MVP et les itérations 2, 3 et 4 : les neuf outils, les spots, les lumières rectangulaires, les styles de lumière, la sélection multiple, la grille, les contraintes d'axe et l'accumulation à vue fixe, les raccourcis clavier, le menu, les étiquettes et les filtres d'affichage, le multiplicateur des shaders émissifs, l'édition des surfaces émissives une par une et le ciel et le soleil de chaque map.

## 1. Prérequis

- **Renderer** : `cl_renderer rdsp-vulkan` avec `r_rtx 1`. Le raster (sans RTX), `rdsp_swgl` (rd-vanilla) et `rdsp-rend2` n'ont pas l'extension. La commande répond alors `light edit: needs the RTX renderer (cl_renderer rdsp-vulkan, r_rtx 1)` et rien ne change.
- **Cheats** : le mode exige les cheats, comme `noclip`. Lancez la map avec `devmap <map>`, ou mettez `helpusobi 1`.
- **Double renderer** (`g_FastRendererSwitch 1`) : l'extension est trouvée dans celui des deux renderers qui la fournit.

## 2. Entrée et sortie

| Commande | Effet |
|---|---|
| `lightedit` | Bascule le mode. |
| `lightedit 1` | Entre dans le mode (sans effet s'il est déjà actif). |
| `lightedit 0` | Sort du mode. |
| `lightedit 0 discard` | Sort du mode et abandonne les changements non sauvegardés (relit le `.lgt`). |

À l'entrée :
- le corps du joueur reste où il est (immobile, angles figés), en god, notarget, et les pouvoirs de la Force sont coupés ;
- une caméra libre vole à la place : elle part de la vue actuelle, traverse les murs, et le serveur envoie les entités visibles depuis elle (le corps du joueur est dessiné) ;
- le tir et les pouvoirs ne partent plus (le jeu retire les boutons de tir et de Force) ;
- l'arme en vue subjective n'est plus dessinée ;
- le HUD normal est remplacé par l'overlay du mode ;
- la sauvegarde de la partie est refusée tant que le mode est actif.

À la sortie, le solo et les lumières muettes sont rétablis, et `pt_accumulation_rendering` reprend sa valeur d'avant si `ledit_still_accum` l'avait changée.

À la sortie, la caméra disparaît, la vue du joueur revient à son angle d'entrée, et l'état d'origine du joueur (god, notarget, Force) est restauré. La mort du joueur sort aussi du mode. S'il reste des changements non sauvegardés, un message l'indique. Ils restent actifs en mémoire jusqu'au prochain chargement de map, sauf avec `discard`.

Le mode n'est jamais sauvegardé dans une partie. Un changement de map ou le chargement d'une partie le désactive.

La cvar `ledit_active` (lecture seule) vaut 1 pendant le mode.

## 2 bis. Caméra libre (`tfc`)

La commande `tfc` lance la caméra libre du light edit, sans ses outils. Elle n'exige pas le renderer RTX : elle marche avec tous les renderers. Elle exige les cheats, comme le light edit.

| Commande | Effet |
|---|---|
| `tfc` | Bascule le mode. |
| `tfc 1` / `tfc 0` | Entre dans le mode ou en sort. |

Le mode se comporte comme le light edit pour la caméra et le joueur : la caméra part de la vue actuelle et traverse les murs, le corps reste immobile et est dessiné, et le joueur est en god, notarget et sans Force. Le tir et la Force sont bloqués, et la sauvegarde est refusée.

L'écran n'affiche que la carte et les entités. Le HUD, le viseur, l'arme en vue subjective, les messages au centre et l'overlay du light edit ne sont pas dessinés.

La vitesse de la caméra est la cvar `ledit_cam_speed`, partagée avec le light edit.

`tfc` et `lightedit` s'excluent : entrer dans l'un sort de l'autre. La mort, un changement de map, le chargement d'une partie et `vid_restart` sortent du mode. Une cinématique (`in_camera`) a priorité sur la caméra.

## 3. Contrôles communs

Le mode réutilise les touches des armes :

| Entrée | Commande d'origine | Action |
|---|---|---|
| Touches 1 à 9 | `weapon N` | Choisit l'outil (1 à 9). |
| Tir principal | `+attack` | Action principale de l'outil. |
| Tir alternatif | `+altattack` | Action secondaire de l'outil. |
| Molette | `weapnext` / `weapprev` | Valeur de l'outil. |
| `]` / `[` | `invnext` / `invprev` | Propriété active de l'outil 5. |
| Marche (maintenue) | walk | Pas fin pour la molette. |
| Saut / accroupi | `+moveup` / `+movedown` | Monter / descendre (caméra libre). |
| Utiliser | `+use` | Place la caméra devant la sélection, tournée vers elle (`ledit_goto`). |

### Raccourcis clavier

Ces touches fonctionnent sans bind, seulement en mode light edit, et seulement quand ni la console ni un menu ne sont ouverts. Toutes les autres touches gardent leur bind.

| Touche | Action |
|---|---|
| Ctrl+Z | Annuler. |
| Ctrl+Y ou Ctrl+Shift+Z | Rétablir. |
| Ctrl+S | Sauvegarder (`ledit_save`). |
| Ctrl+D | Vider la sélection. |
| Ctrl+M | Ouvrir le menu light edit (§ 5 bis). |
| Suppr | Supprimer ou désactiver la sélection. |
| Entrée (ou Entrée du pavé) | Saisie numérique de la propriété active (voir ci-dessous). |

**Saisie numérique.** Entrée ouvre un champ pour la propriété active de l'outil 5 (l'intensité avec les autres outils), rempli avec la valeur de la lumière principale. Un premier caractère remplace la valeur ; chiffres, `.`, `-` et espace s'ajoutent, Retour arrière efface. Entrée applique la valeur à toute la sélection (une entrée d'undo), Échap annule. Pendant la saisie, les chiffres ne changent pas d'outil. Les valeurs : intensité ; teinte (0-360, ou `teinte sat valeur`) ; saturation (0-1) ; température (K) ; rayon ; cône (`ext [int]`). Le champ se ferme à la sortie du mode, au changement de map, quand la sélection est vide ou après 30 s sans touche.

Échap n'est consommé que pendant une saisie ; sinon il ouvre le menu du jeu, comme d'habitude. La touche de la console et Shift+Échap ne sont jamais interceptés.

Ctrl est souvent lié à l'accroupissement : un raccourci Ctrl fait descendre un peu la caméra libre.

### Cibles d'une action

- La **sélection** est une liste de lumières. La dernière lumière cliquée est la lumière **principale** : elle porte la sphère ou le cône filaire, et le panneau la décrit.
- Une action vise la lumière **sous le viseur**. Si cette lumière fait partie de la sélection, l'action s'applique à **toute la sélection**. Si rien n'est visé, l'action s'applique à la sélection (outils 3, 4, 5, 7, 8).
- Les actions de groupe gardent les écarts entre lumières (position, intensité relative).
- Une action de groupe compte pour **une seule** entrée d'undo.

## 4. Outils

### 1. Sélection
- **Tir** : sélectionne la lumière ou la surface émissive visée (voir § 8 ter) et remplace la sélection. Dans le vide, vide la sélection et le shader émissif choisi. Les surfaces émissives ne sont visées qu'avec `ledit_show` ≥ 2, et après les lumières : si une lumière et une surface sont sous le viseur, la molette passe de l'une à l'autre.
- **Alt** : ajoute la lumière visée à la sélection, ou l'en retire.
- **Molette** : passe d'une lumière à l'autre quand plusieurs sont alignées sous le viseur (triées par distance).

### 2. Création
- Un « fantôme » suit le point d'impact du viseur, décalé le long de la normale de la surface. Avec `ledit_snap 1`, il est accroché à la grille.
- **Tir** : ajoute une lumière au fantôme avec le préréglage (`ledit_preset_*`, ou le presse-papier de la pipette), puis la sélectionne.
- **Alt** : change le type des nouvelles lumières, sphère → spot → rectangle → sphère. Un spot posé à plus de `ledit_spot_near` unités de la surface vise l'opposé de la normale (dans la surface) ; plus près, il vise le long de la normale (loin de la surface : sous un plafond, il éclaire la pièce). Le fantôme montre cette direction. Cônes 35° / 25°, ou ceux du presse-papier.
- **Rectangle** : un rectangle posé prend la normale de la surface (il éclaire à l'opposé de la surface), ou `0 0 -1` sans impact. Sa taille vient du presse-papier de la pipette, sinon 32 × 32. Le fantôme est un rectangle filaire avec la flèche de la normale.
- **Molette** : décalage par rapport à la surface, ×2 ou ÷2 (±1 avec la marche), de 2 à 256 unités.
- **Maj + molette** : règle la propriété active de la lumière que l'outil va poser, avec les mêmes pas que l'outil 5. `]` / `[` choisit la propriété (le cycle saute celles qui ne s'appliquent pas au type posé). Le pas est toujours le pas normal, car Maj sert souvent de touche de marche. Le réglage est conservé pour les poses suivantes : il vit dans les cvars `ledit_preset_*` (archivées), ou dans le presse-papier de la pipette quand il contient une lumière. L'aide en haut à gauche montre la propriété et sa valeur. `ledit_preset_step <n>` fait la même chose en commande, pour un bind sans Maj.
- La distance prise en compte est celle du fantôme au plan de la surface, après l'accrochage à la grille. Sans impact, la direction reste `0 0 -1`.
- Sans impact (ciel), le fantôme est à 256 unités devant la caméra. Dans un solide, il est rouge et la pose est refusée. Un solo actif (outil 9) est terminé avant la pose.

### 3. Déplacement (« physgun »)
- **Tir maintenu** sur une lumière : la saisit (ou toute la sélection si elle en fait partie). Elle suit le viseur à distance constante. Une surface émissive fait partie de la map : elle n'est pas saisie (message) ; les autres lumières de la sélection le sont.
- **Molette pendant la saisie** : distance ×1,1 ou ÷1,1 (×1,01 avec la marche). Sans effet en mode contraint.
- **Alt pendant la saisie** : contrainte libre → axe X → axe Y → axe Z → plan de la surface → libre.
  - **Axe** : la lumière glisse sur l'axe du monde qui passe par sa position au choix de la contrainte (ligne en pointillés rouge, verte ou bleue).
  - **Plan de la surface** : la lumière suit la surface visée, à la distance qu'elle avait de la surface au début de la saisie (cercle jaune au point visé).
- **Relâcher** : valide le déplacement (une entrée d'undo).
- **Alt sans saisie** : remet les cibles à leur position d'origine.
- Une position dans un solide n'est pas appliquée : la lumière garde sa dernière position valide.
- Avec `ledit_snap 1`, la lumière principale est accrochée à la grille (seule la composante de l'axe en mode contraint).

### 4. Orientation (spots et rectangles)
- **Tir** : oriente les spots ciblés vers le point visé. Avec `ledit_angle_snap` > 0, le lacet et le tangage de la direction sont arrondis à ce pas. Sur un rectangle, le tir tourne sa normale vers le point visé (même arrondi).
- **Alt sur un spot** : choisit l'angle que la molette règle (extérieur ou intérieur).
- **Alt sur un rectangle** : choisit ce que la molette règle : roulis → largeur → hauteur.
- **Alt sur une sphère** : convertit les sphères ciblées en spots, dirigés vers le point visé (ou vers le bas).
- **Molette** : angle choisi ± `ledit_angle_snap` (1° avec la marche). Extérieur 1-89°, intérieur 0 à l'extérieur. Réduire l'extérieur sous l'intérieur abaisse l'intérieur.
- **Molette sur un rectangle** : roulis ± `ledit_angle_snap` (1° avec la marche) ; largeur ou hauteur ×1,1 (×1,01 avec la marche), de 1 à 2048. Avec `ledit_snap 1`, la taille est arrondie à la grille et bouge d'au moins un pas de grille ; la marche coupe cet arrondi.
- Affichage : cône filaire du spot principal (axe jusqu'au monde, cercle du bord, 8 génératrices, cercle intérieur, disque éclairé au point d'impact) ; axe et bord seulement pour 3 autres spots sélectionnés au plus.
- Une surface émissive n'a ni direction ni forme à régler : cet outil l'ignore.

### 5. Propriétés
- `]` / `[` choisit la propriété active : intensité, teinte, saturation, température, rayon d'émission, cône extérieur, cône intérieur, largeur, hauteur, roulis, deux faces, style. Elle est en jaune dans le panneau, avec une jauge. Le cycle saute les propriétés qui ne s'appliquent pas à la lumière principale : cônes pour les spots seulement ; largeur, hauteur, roulis et deux faces pour les rectangles seulement ; rayon d'émission pas pour les rectangles.
- **Molette** sur les cibles :
  - intensité ×1,1 (×1,01 avec la marche) ;
  - teinte ±10° (±1°) ; saturation ±0,05 (±0,01) ;
  - température ±250 K (±50 K) : la couleur devient celle d'un corps noir de 1000 à 12000 K, normalisée ;
  - rayon d'émission ±1 (±0,1), minimum 0,5 ;
  - cônes comme l'outil 4 ;
  - largeur et hauteur ×1,1 (×1,01), arrondies à la grille avec `ledit_snap 1` ; roulis ± `ledit_angle_snap` (1°) ;
  - deux faces : chaque cran bascule la valeur ;
  - style ±1 (de 0 à 63, avec bouclage) ; il s'applique à tous les types (voir § 8 bis).
- **Tir** : copie la propriété active de la lumière visée sur toute la sélection.
- **Alt** : remet la propriété active à sa valeur d'origine (pour la température, toute la couleur d'origine).
- Les crans de molette successifs sur les mêmes lumières et la même propriété, à moins de 500 ms d'écart, forment **une** entrée d'undo.
- La teinte n'agit pas sur une couleur grise (saturation 0) : augmentez d'abord la saturation.
- **Surface émissive** (§ 8 ter) : seules les propriétés intensité (le facteur de la surface), teinte, saturation, température et style s'appliquent ; le cycle `[` / `]` saute les autres. La molette change le facteur ×1,1 (×1,01 avec la marche), de 0 à 100. La teinte multiplie la couleur de la lumière émise. Une sélection qui mêle des lumières et des surfaces : l'intensité et la saisie numérique de l'intensité ne touchent que le type de la lumière principale (les unités diffèrent) ; la molette, qui multiplie, change les deux.
- **Multiplicateur du shader.** Quand un shader émissif est choisi (menu, page « Emissive », ou `ledit_emissive_select`) et qu'aucune lumière n'est visée ni sélectionnée, l'outil 5 règle le multiplicateur du shader (voir § 8 ter) :
  - **Molette** : multiplicateur ×1,1 (×1,01 avec la marche), de 0 à 100 ; il s'arrête à 1 quand un cran le franchit ;
  - **Alt** : remet le multiplicateur à 1 ;
  - jusqu'à 300 surfaces du shader choisi reçoivent un cadre jaune ;
  - une lumière visée ou sélectionnée garde la priorité.

### 6. Pipette
- **Tir** : copie la lumière visée dans le presse-papier (type, couleur, intensité, rayon, direction, cônes, largeur, hauteur, roulis, deux faces, style). Le presse-papier devient le préréglage de l'outil 2 jusqu'au changement de map.
- **Alt** : colle sur la lumière visée (ou la sélection si elle en fait partie), selon le filtre.
- **Surfaces émissives** (`ledit_show` ≥ 2) : le tir sur une surface copie son facteur, sa teinte et son style. Cette copie ne devient pas le préréglage de l'outil 2. Alt colle sur une surface visée (ou la sélection) : les filtres « tout » et « couleur » collent la teinte, « tout » et « intensité » collent le facteur (d'une autre surface seulement), « tout » colle le style, « forme » ne colle rien. Une surface et une lumière ne partagent que la couleur et le style : l'intensité d'une lumière ne se colle pas sur une surface, ni l'inverse (message).
- **Molette** : filtre du collage : tout / couleur / intensité / forme. « Forme » colle le type, le rayon, les cônes (spot) ou la largeur, la hauteur, le roulis et les deux faces (rectangle). « Tout » colle aussi le style. Coller une forme d'un autre type convertit la cible ; la direction du presse-papier n'est copiée que lors d'une conversion.

### 7. Clonage
- **Tir** : duplique la lumière visée (ou la sélection si elle en fait partie). Les copies deviennent la sélection, l'outil 3 s'active et les copies sont saisies tout de suite. Une surface émissive ne se clone pas (message).
- **Alt** : crée N copies à 1, 2, … N pas de grille (`ledit_grid`) le long de l'axe X ou Y du monde le plus proche de la droite de la vue. Les copies hors du monde sont sautées.
- **Molette** : N, de 1 à 32.
- Une copie est toujours une lumière **ajoutée** ; son intensité est convertie pour garder la même luminosité que la source (voir § 8).

### 8. Suppression et restauration
- **Tir** : supprime la lumière visée (ou la sélection). Une lumière ajoutée est effacée. Une lumière d'origine, ou une surface émissive, est **désactivée** : icône rouge barrée, translucide. La surface désactivée garde ses slots et reste visible jusqu'à `ledit_compact`.
- **Alt** : restaure la lumière désactivée visée. Après `ledit_compact`, une lumière ou une surface compactée n'est visée et restaurée que dans le mode restauration (§ 8 quinquies).

### 9. Solo et muet
- **Tir** : met la lumière visée en **solo** : toutes les autres lumières éditables sont éteintes, les surfaces émissives comprises (une surface visée peut être mise en solo elle aussi). Un second tir termine le solo.
- **Alt** : coupe ou rétablit la lumière ou la surface visée (**muet**).
- **Molette** : filtre des icônes : toutes / ajoutées / modifiées / d'origine / désactivées.
- Le solo et le muet ne sont ni des éditions ni sauvegardés, et ne passent pas par l'undo. Ils sont annulés à la sortie du mode et au changement de map.

## 5. Affichage

- **Icônes** :
  - bleu : entité `light` du BSP ;
  - jaune : lumière reconstruite (`.lgt`, `pt_lightgen`) ;
  - vert : lumière ajoutée ;
  - orange : lumière d'origine modifiée ;
  - rouge barré, translucide : lumière désactivée ; une lumière compactée (§ 8 quinquies) a la même icône, mais seulement dans le mode restauration ;
  - rouge plein : lumière dans un solide (elle n'émet rien) ;
  - contour gris : lumière muette ou éteinte par le solo ;
  - petit point gris : surface émissive (avec `ledit_show` ≥ 2) ; orange si elle est modifiée, rouge barré si elle est désactivée ou compactée. Les 500 plus proches, à moins de 2048 unités, sont dessinées, après les lumières.
- **Cadre d'une surface émissive** : le contour de ses triangles (les arêtes qu'un seul triangle utilise, ou les 12 arêtes de la boîte si le contour dépasse 32 arêtes), en jaune pour les surfaces sélectionnées (la principale et 3 autres au plus) et en gris clair pour la surface visée avec les outils 1, 5, 6, 8 et 9. Il est dessiné même sans `ledit_show` 2, et ne prend jamais les 1000 derniers points du budget.
- Cadre blanc : lumière sous le viseur. Cadre de chaque lumière sélectionnée ; sphère filaire (sphère) ou cône filaire (spot) sur la lumière principale.
- **Rectangles** : les rectangles sélectionnés et celui visé (outils 4 et 5) sont dessinés en vert menthe : 4 bords, flèche de la normale et, pour le rectangle principal, un trait court le long de +U (le roulis). Un rectangle à deux faces ajoute les diagonales et une flèche arrière. Trois autres rectangles sélectionnés au plus ont les bords et la flèche.
- **Soleil** : avec le ciel physique ou hybride, un repère (cadre et point jaunes) marque la direction du soleil projetée à l'écran.
- Sans `ledit_xray`, les lumières cachées par un mur, et celles à plus de 3000 unités, ne sont pas affichées.
- **Lumière compactée** : hors du mode restauration, elle disparaît complètement : ni icône, ni étiquette, ni cadre, et aucun outil ne peut la viser ni la sélectionner (`ledit_select` la refuse). Dans le mode restauration, elle a l'icône rouge barrée et l'étiquette « compacted », et seul l'outil 8 la vise.
- **En haut à gauche** : outil actif et aide (Tir / Alt / Molette), avec la valeur courante (décalage, angle, propriété, filtre, nombre de copies, contrainte).
- **À droite** : panneau de la sélection. Pour une lumière : id, source, type, état, origine, couleur, teinte/saturation/température, intensité, rayon, direction et cônes (spot), nom ; valeur d'origine en gris si elle a changé. Un rectangle s'affiche « rect 32x16 », avec « normal » à la place de « direction », puis largeur, hauteur, roulis et deux faces ; le style s'affiche s'il n'est pas 0. Pour plusieurs : « N lights (primary id) », puis la valeur commune ou « - ».
  - Pour une surface émissive, le panneau donne « emissive surface N » (N est l'id), la source (« emissive face rank R »), le type « surface », l'état, un point de la surface, la teinte (couleur), l'intensité (« x » suivi du facteur), le style s'il n'est pas 0, le nom (la fin du nom du shader), « shader: <nom> », « triangles: T (face rank R) » et « factor: surface F x shader S (index) ». Le facteur et la teinte d'origine s'affichent en gris quand ils ont changé.
  - Sans lumière sélectionnée ni shader émissif choisi, le panneau donne le ciel : « sky: <mode> (map) * » et « sun az A el E xB » ou « no sun ». Avec un shader émissif choisi (menu ou `ledit_emissive_select`), il donne « emissive shader N », le nom, le nombre de polygones et le multiplicateur. Une lumière visée ou sélectionnée a la priorité.
- **En bas** : map, compteurs (dont `emissive` (surfaces émissives), `disabled` et `compacted`), slots utilisés / limite, profondeur d'undo et de redo, `*` si des changements ne sont pas sauvegardés, grille et snap, `RESTORE MODE`, `SOLO <id>`, filtre d'icônes, avertissements. Les slots utilisés sont ceux des lumières et des polygones des surfaces émissives (un slot par triangle) ; les slots libres (après un compactage ou en réserve) ne comptent pas.
- **Étiquettes** (`ledit_label`) : nom (ou `#id`), type et intensité à côté de l'icône. Un rectangle donne « rect LxH ». Le style apparaît après l'intensité s'il n'est pas 0 (« I style N »).
- **Filtres d'affichage** (`ledit_show`) : 2 ajoute les surfaces émissives (points gris, sélectionnables), 3 ajoute aussi les lumières dynamiques de l'image précédente (points magenta : dlights, sabres), qui ne sont pas sélectionnables. Le bas de l'écran affiche « show N ».
- Tout le dessin de l'overlay respecte un budget fixe par image (points et caractères) : un cône, une grande sélection ou beaucoup d'étiquettes ne peuvent pas saturer le buffer de commandes du renderer. Les formes de la sélection et des outils passent en premier, puis les icônes (les surfaces émissives après les lumières), puis les points dynamiques.

## 5 bis. Menu light edit

`ledit_menu` (ou Ctrl+M) ouvre un menu sur la moitié droite de l'écran ; la scène reste visible à gauche. Échap ou « Close » le ferme et rend la main au jeu.

- **En-tête** : map, nombre de lumières, de surfaces émissives, compteurs par source, désactivées, compactées, slots utilisés / limite, `* unsaved`. Le sélecteur « Page » (cvar `ui_ledit_page`) choisit entre les pages Lights, Sky and sun et Emissive ; les sections suivantes décrivent chaque page. « Close » est sur toutes les pages.

**Page Lights**
- **Filtre** (cvar `ui_ledit_filter` : all, added, entity, lgt, emissive, modified, disabled, spots ; un clic passe à la valeur suivante) et **recherche** (`ui_ledit_search`, sous-chaîne du nom, sans casse). Les surfaces émissives sont nombreuses : le filtre `all` les cache ; `emissive`, `modified` et `disabled` les listent. Le nom d'une surface est la fin du nom de son shader.
- **Liste** : id, source (`added`, `ent N`, `lgt N`, `emi R` pour une surface émissive, R étant le rang de la face parmi celles de son shader), type (« rect LxH » pour un rectangle, « surface » pour une surface émissive, suivi de « sN » si le style N n'est pas 0), intensité, état (M modifiée, D désactivée, C compactée, S dans un solide, m muette), nom. Un clic sur une ligne la choisit et la sélectionne (`ledit_select`). Le filtre `disabled` liste les lumières supprimées, compactées comprises (état C) ; les autres filtres cachent les lumières compactées. Sur une ligne compactée, seul « Restore » agit : les autres boutons répondent « compacted, restore it first ».
- **Boutons** (sur la ligne choisie) : Select, Add to sel., Go to, Delete, Restore, Solo, End solo, Revert, Close.
- **Couleur** : curseurs teinte (0-360), saturation, valeur, température (1000-12000 K) avec un aperçu. « Load from light » lit la couleur, l'intensité et le rayon de la ligne choisie ; « Apply colour » applique la couleur HSV ; « Use temperature » met la couleur du corps noir dans les curseurs ; « Apply temperature » l'applique.
- **Valeurs** : champs intensité et rayon, chacun avec « Apply ».
- **Nom et cônes** : un champ « Name » (31 caractères au plus ; `"`, `;` et `\` sont remplacés par `_`) avec « Apply » (`ledit_set name`), et deux champs « Cone ext » et « int » (degrés) avec « Apply » (`ledit_set cone`, pour un spot seulement). « Load from light » remplit aussi le nom et, pour un spot, les cônes.
- **Suivi de la sélection du jeu** : le cgame écrit la lumière principale dans la cvar `ledit_sel` et le shader émissif choisi dans `ledit_emi_sel` (-1 pour aucun). Quand l'une change, le menu choisit la ligne de la liste correspondante (elle défile jusqu'à elle), remplit les champs de la page et les boutons agissent sur cette ligne. Un filtre ou une recherche qui cache la lumière laisse la liste sans ligne choisie ; les boutons visent quand même la lumière. À l'ouverture du menu, la sélection du jeu est reprise.

**Page Sky and sun**
- Deux lignes donnent le ciel en vigueur : mode (`global`, `skybox`, `physical`, `hybrid`, « (map) » s'il vient de la map), le nuage (`cloud`) et le plancher (`floor`), puis le soleil (azimut, élévation, couleur, luminosité, angle).
- Quatre boutons (Global, Skybox, Physical, Hybrid) envoient `ledit_sky <mode>`.
- Champs : « Azimuth », « Elevation », « Angle (1-10) », « Brightness » et « Colour r g b », puis « Cloud (0-1) », « Floor colour r g b » et « Floor intensity ». « Apply » envoie seulement les commandes dont la valeur diffère du ciel en vigueur (`ledit_sun`, `ledit_sun_color`, `ledit_sun_brightness`, `ledit_sun_angle`, `ledit_sky_diffuse`, `ledit_ambient`) : chacune est une entrée d'undo. « Sun here » envoie `ledit_sun_here`, « Reset sky » envoie `ledit_sky_reset`, « No floor » envoie `ledit_ambient_reset`, « Load from sky » remplit les champs.
- Les champs se remplissent seuls à l'ouverture du menu et à chaque changement du ciel (commande, undo, Ctrl+Z).

**Page Emissive**
- Liste des shaders émissifs : index, polygones, multiplicateur (`*` s'il n'est pas 1), nom. Le champ « Search » filtre sur une sous-chaîne du nom.
- Un clic sur une ligne la choisit, remplit le champ « Multiplier » et sélectionne le shader dans le jeu (`ledit_emissive_select` : cadres jaunes sur ses surfaces avec les outils 1, 5 et 6, panneau).
- « Set » envoie `ledit_emissive <index> <valeur>`, « Reset » envoie `ledit_emissive_reset <index>`, « Reset all » envoie `ledit_emissive_reset all`.

Le menu envoie des commandes du mode (`ledit_select`, `ledit_set`, `ledit_goto`, `ledit_delete`, `ledit_revert`, `pt_ledit_restore`, `pt_ledit_solo`) : ses changements passent par l'undo (sauf Restore et Solo). Le texte du menu est intégré à l'exe ; un fichier `ui/lightedit.menu` dans les données du jeu le remplace s'il existe (pour retoucher la mise en page sans recompiler). Ce fichier doit alors contenir tout le menu : dans le texte intégré, chaque page est cachée par un test sur `ui_ledit_page` (`cvarTest "ui_ledit_page" showCvar { "lights" }`).

Le **rayon d'émission** n'est pas une portée. Une sphère du tracer éclaire en 1/d², sans coupure. Le rayon d'émission règle seulement la douceur des ombres.

Une sphère posée contre un mur éclaire la partie du mur que la sphère traverse : un point du mur à l'intérieur de la sphère voit l'émetteur sur un hémisphère qui passe sans saut de la direction du centre (à la surface de la sphère) à la normale du mur (au centre) (shaders `light_lists.h`, `sphere_light_contains`). Le poids de la sphère a un champ proche doux (`sphere_light_area`, exact à partir de 5 rayons) et la pondération de l'émission part de 0 à la surface de la sphère, comme dehors : aucun cercle ne se voit, même avec un grand rayon. Une sphère un peu derrière une surface est reflétée devant elle (64 unités au plus).

## 6. Commandes

Toutes, sauf `lightedit`, demandent le mode actif.

| Commande | Effet |
|---|---|
| `ledit_save` | Écrit `maps/<map>.lgt` (v2) dans le homepath, après une copie de l'ancien fichier en `maps/<map>.lgt.bak`. |
| `ledit_reload` | Abandonne les changements en mémoire et relit le fichier. Vide l'undo et la sélection. |
| `ledit_undo` / `ledit_redo` | Annule / rétablit la dernière action (ou le dernier groupe). |
| `ledit_history` | Affiche les piles d'undo et de redo (une ligne par groupe, avec sa taille). |
| `ledit_set origin x y z` | Place la lumière principale ; les autres lumières sélectionnées gardent leur écart. |
| `ledit_set color r g b` | Couleur (0 à 1) de la sélection ; la teinte d'une surface émissive. |
| `ledit_set intensity v` | Intensité (voir § 8) ; le facteur (0 à 100) d'une surface émissive. |
| `ledit_set radius v` | Rayon d'émission. |
| `ledit_set name texte` | Nom. |
| `ledit_set type sphere\|spot\|rect` | Convertit la sélection. |
| `ledit_set dir x y z` | Direction (spot) ou normale (rectangle). |
| `ledit_set cone ext [int]` | Demi-angles des cônes, en degrés. |
| `ledit_set width w` / `height h` | Taille d'un rectangle (1 à 2048). |
| `ledit_set roll deg` | Roulis d'un rectangle, en degrés. |
| `ledit_set twosided 0\|1` | Rectangle à deux faces. |
| `ledit_set style N` | Style de lumière (0 à 63), tous les types. |
| `ledit_get` | Affiche les valeurs de la sélection (et les valeurs d'origine si elle est modifiée). |
| `ledit_delete` | Supprime ou désactive la sélection. |
| `ledit_deselect` | Vide la sélection. |
| `ledit_revert` | Remet toutes les valeurs d'origine de la sélection. |
| `ledit_grid_next` | Pas de grille suivant (1, 2, 4, 8, 16, 32, 64, 128). |
| `ledit_snap_toggle` | Active ou coupe le snap. |
| `ledit_xray_toggle` | Active ou coupe le mode « à travers les murs ». |
| `ledit_select <id> [add]` / `ledit_select none` | Sélectionne une lumière par son id (ou l'ajoute à la sélection), ou vide la sélection. |
| `ledit_compact` | Libère les slots de toutes les lumières et surfaces émissives supprimées et tasse les slots utilisés (§ 8 quinquies). Affiche le nombre de lumières (surfaces comprises) et de slots libérés, et les slots utilisés / limite. |
| `ledit_restore [0\|1]` | Bascule le mode restauration, ou le règle avec 0 ou 1 : les lumières compactées s'affichent et l'outil 8 (Alt) peut les restaurer. |
| `ledit_preset_step <n>` | Outil 2 : n crans de la propriété active du préréglage (comme Maj + molette ; n de -8 à 8). |
| `ledit_goto` | Place la caméra devant la sélection, tournée vers elle, hors des murs (aussi : Utiliser). |
| `ledit_menu` | Ouvre le menu light edit (aussi : Ctrl+M). |
| `ledit_writebinds [force]` | Écrit `lightedit_binds.cfg` dans le homepath (refuse d'écraser un fichier existant sans `force`), puis indique `exec lightedit_binds.cfg`. |
| `ledit_emissive_list [filtre]` | Liste les shaders émissifs : index, nom, surfaces, polygones, multiplicateur (`*` s'il n'est pas 1). Le filtre est une sous-chaîne du nom. |
| `ledit_emissive <index\|nom> <échelle>` | Multiplicateur d'un shader (0 à 100). Le nom est le nom exact ou une partie unique du nom. |
| `ledit_emissive_reset <index\|nom\|all>` | Remet un shader, ou tous, à 1. |
| `ledit_emissive_select <index\|nom\|none\|sel>` | Choisit un shader émissif (vide la sélection de lumières), ou le désélectionne. `sel` choisit le shader de la surface émissive sélectionnée. |
| `ledit_emissive_surfaces [index\|nom]` | Liste les surfaces émissives (id, shader, rang de la face, triangles, facteur, état, nom du shader) du shader d'index donné, des shaders dont le nom contient le texte, ou de tous (200 lignes au plus). |
| `ledit_sky [global\|skybox\|physical\|hybrid]` | Mode du ciel de la map. Sans argument, affiche les valeurs. |
| `ledit_sun <azimut> <élévation>` | Direction du soleil, en degrés. |
| `ledit_sun_here` | Le soleil vient de la direction du regard. |
| `ledit_sun_color <r> <g> <b>` | Couleur du soleil (0 à 10 par composante, 1 = blanc). |
| `ledit_sun_brightness <x>` | Luminosité du soleil. |
| `ledit_sun_angle <deg>` | Diamètre angulaire du soleil (1 à 10°). |
| `ledit_sky_reset` | La map revient au ciel global (le nuage revient à 0). Le plancher ambiant reste. |
| `ledit_sky_diffuse <0-1>` | Nuage (§ 8 quater) : part du soleil direct que le nuage retire et rend en lumière de ciel. Sans argument, affiche la valeur. En mode `global`, passe d'abord en `physical` ou `hybrid` (comme une commande de soleil). En mode `skybox` (pas de soleil), la valeur est gardée mais ignorée, et la commande l'indique. |
| `ledit_ambient <r> <g> <b> [intensité]` | Plancher ambiant de la map (§ 8 quater) : couleur (0 à 1 par composante) et intensité (0 à 100, 0 = coupé). Sans intensité, garde l'intensité en vigueur, ou 1 si elle valait 0. Sans argument, affiche la valeur. Marche dans tous les modes, `global` compris. |
| `ledit_ambient_reset` | Coupe le plancher ambiant de la map (intensité 0). |

Sur une surface émissive, seuls `ledit_set color`, `intensity` et `style` agissent. Les autres clés (`origin`, `radius`, `name`, `type`, `dir`, `cone`, `width`, `height`, `roll`, `twosided`) sautent les surfaces de la sélection, avec un message. `ledit_set intensity` ne touche que les cibles du type de la lumière principale (surface ou lumière). `ledit_delete`, `ledit_revert`, `ledit_select`, `ledit_goto`, `ledit_get` et `ledit_undo` marchent sur une surface comme sur une lumière ; `ledit_get` ajoute le shader, le nombre de triangles et les facteurs.

Commandes de débogage du renderer (utilisables même hors du mode) : `pt_ledit_list` (affiche aussi les rectangles, les surfaces émissives et les slots), `pt_ledit_add x y z [intensité] [r g b] [rect w h] [dir x y z] [roll deg] [twosided 0|1] [style n]`, `pt_ledit_set <id> <origin|color|intensity|radius> <valeurs>`, `pt_ledit_set <id> spot dx dy dz ext int`, `pt_ledit_set <id> sphere`, `pt_ledit_set <id> rect [w h]`, `pt_ledit_set <id> dir x y z`, `pt_ledit_set <id> roll deg`, `pt_ledit_set <id> twosided 0|1`, `pt_ledit_set <id> style n`, `pt_ledit_del <id>`, `pt_ledit_restore <id>`, `pt_ledit_compact`, `pt_ledit_mute <id> <0|1>`, `pt_ledit_solo <id|-1>`, `pt_ledit_stats`, `pt_ledit_emissive [n]` (les n premières surfaces émissives : face, rang, shader, triangles, slots, centre, facteur), `pt_ledit_dynamic`, `pt_ledit_emissive_shaders`, `pt_ledit_emissive_scale <index|nom> <échelle>`, `pt_sky_print`, `pt_sky_set <skybox|physical|hybrid|global>` ou `pt_sky_set <clé> <valeurs>` (`sun_azimuth`, `sun_elevation`, `sun_color`, `sun_brightness`, `sun_angle`, `sky_diffuse`, et, dans tous les modes, `ambient_color r g b` et `ambient_intensity v`), `pt_sky_reset`.

Déplacer ou tourner un rectangle reconstruit les listes de lumières par cluster (au plus toutes les 100 ms pendant une saisie), car ces listes écartent les clusters situés derrière le plan du polygone.

## 7. Cvars

| Cvar | Défaut | Rôle |
|---|---|---|
| `ledit_xray` | 0 | 1 : icônes visibles et sélection possible à travers les murs. |
| `ledit_show` | 1 | 0 : aucune icône ; 1 : lumières éditables ; 2 : + surfaces émissives (points gris, 500 au plus, à moins de 2048 unités ; sélectionnables) ; 3 : + lumières dynamiques de l'image précédente (points magenta, 64 au plus ; non sélectionnables). |
| `ledit_label` | 1 | 0 : aucune étiquette ; 1 : la lumière visée et les lumières sélectionnées ; 2 : + les lumières visibles à moins de 1024 unités de l'œil, les plus proches d'abord (24 étiquettes au plus). Une étiquette donne le nom (ou `#id`), puis le type et l'intensité, avec `muted` ou `disabled` si besoin. |
| `ledit_grid` | 16 | Pas de la grille (1, 2, 4, 8, 16, 32, 64, 128). |
| `ledit_snap` | 0 | 1 : accroche les positions à la grille (création, déplacement, clonage). |
| `ledit_angle_snap` | 15 | Pas d'angle (degrés) des cônes et des directions. 0 : libre. |
| `ledit_still_accum` | 0 | 1 : quand la vue ne bouge plus depuis 500 ms (et sans saisie ni bouton), met `pt_accumulation_rendering 1` pour une image de référence sans bruit ; le premier mouvement rétablit la valeur d'avant. La valeur est aussi rétablie à la sortie du mode. |
| `ledit_preset_intensity` | 2000 | Intensité d'une lumière créée. |
| `ledit_preset_radius` | 8 | Rayon d'émission d'une lumière créée. |
| `ledit_spot_near` | 64 | Distance (unités) sous laquelle un spot créé près d'une surface vise loin d'elle (le long de la normale). 0 : toujours dans la surface. Non archivé. |
| `ledit_cam_speed` | 500 | Vitesse de la caméra libre (unités par seconde, déplacement plein). Marche (maintenue) la divise par deux. |
| `ledit_preset_color` | `1 0.9 0.8` | Couleur d'une lumière créée. |
| `ledit_preset_cone_outer`, `ledit_preset_cone_inner` | 35, 25 | Cônes d'un spot créé (degrés). |
| `ledit_preset_width`, `ledit_preset_height`, `ledit_preset_roll`, `ledit_preset_twosided` | 32, 32, 0, 0 | Taille, roulis et deux faces d'un rectangle créé. |
| `ledit_preset_style` | 0 | Style d'une lumière créée. |
| `pt_light_scale_edit` | 0.1 | Échelle unique des lumières **ajoutées**. |
| `ledit_active` | — | Lecture seule, 1 pendant le mode. |
| `ledit_sel`, `ledit_emi_sel` | — | Écrites par le cgame pour le menu : id de la lumière principale et index du shader émissif choisi (-1 pour aucun). Ne pas les modifier. |
| `pt_light_styles` | 1 | 0 : coupe la modulation des styles de lumière. |
| `pt_sky_per_map` | 1 | 0 : ignore le ciel réglé pour la map (pour comparer avec le ciel global). |
| `pt_ambient_floor_scale` | 1 | Multiplie le plancher ambiant de la map (§ 8 quater), pour le régler sans toucher au fichier. 0 coupe le plancher. Archivée. |
| `pt_sky_q3map_sun` | 0 | 1 : une map sans bloc global mais avec `q3map_sun` reçoit le ciel hybride, avec la direction et la couleur de `q3map_sun`. |

Les cvars `ledit_*` sont archivées : elles gardent leur valeur d'une session à l'autre.

Le renderer redémarre l'accumulation (`pt_accumulation_rendering`) dès que la liste des lumières change.

## 8. Intensités et échelles

- Une lumière **ajoutée** a une intensité en unités de la clé `light` de q3map2, multipliée par `pt_light_scale_edit`. Cette échelle est la même partout : déplacer la lumière ne change pas sa luminosité.
- Une **entité** du BSP garde l'unité `light` de q3map2 et la classe d'échelle calculée au chargement à sa position d'origine (spot, ciel ou ambiante ; sans facteur : les cvars `pt_light_scale_ent_*`, `pt_light_scale_glow` et `pt_light_scale_surface` n'existent plus, l'édition à la main les remplace). La déplacer ou la modifier ne change pas sa classe.
- Une lumière **reconstruite** (`lgt`) garde son unité (unités de lightmap × `pt_lightgen_scale`).
- Le clonage, la pipette et la copie de l'intensité (outil 5) convertissent l'intensité d'une source à l'autre, pour garder la même luminosité : I2 = I1 × échelle1 / échelle2.
- Convertir une sphère en spot garde la luminosité sur l'axe du spot.
- **Rectangle** : l'intensité a la même unité que celle d'une sphère. La couleur est multipliée par 0,8π / (largeur × hauteur) : l'éclairement sur l'axe égale celui d'une sphère de même intensité, et le flux total reste le même quand la taille change.
- Un rectangle à une face posé sur un sol, normale vers le haut, éclaire le plafond et pas le sol. Une normale vers le bas éclaire le sol.
- Les axes d'un rectangle : U est horizontal pour une normale non verticale, et vaut l'axe X pour une normale verticale ; le roulis tourne U et V autour de la normale. La largeur suit U, la hauteur suit V.

## 8 bis. Styles de lumière

Chaque lumière a un style de 0 à 63. Le style 0 est fixe. Les autres modulent l'éclairage : l'échelle vaut (r+g+b) / 765 de la couleur que le jeu donne au style à chaque image.

| Style | Nom |
|---|---|
| 0 | fixe (steady) |
| 1 | scintillement (flicker) |
| 2 | pulsation lente et forte |
| 3 | bougie |
| 4 | stroboscope rapide |
| 5 | pulsation douce |
| 6 | scintillement 2 |
| 7 | bougie 2 |
| 8 | bougie 3 |
| 9 | stroboscope lent |
| 10 | néon qui scintille |
| 11 | pulsation lente |
| 12 | pulsation rapide |
| 13 | mélange de test |
| 14 à 31 | « style N » |
| 32 à 63 | lumières commutables (« switch N-32 ») |

- Une lumière d'entité du BSP lit la clé `style` de l'entité comme style d'origine.
- Un style qui scintille rend aussi la lumière plus sombre en moyenne (le niveau `m` de JKA vaut environ la moitié de `z`).
- La cvar `pt_light_styles` (1 par défaut) : 0 coupe la modulation.
- Réglage : outil 5 (propriété « style »), `ledit_set style N`, pipette (filtre « tout »).

## 8 ter. Multiplicateur émissif par shader

Un multiplicateur de 0 à 100 règle la lumière que les polygones d'un shader émissif projettent **et** l'émission de la surface que le tracer montre. Le renderer l'ajoute au facteur `pt_glow_scale` de la table des matériaux (code d'échelle sur 8 bits, pas de 4 %) : le facteur agit sur l'émission vue directement, sur les couches additives de la surface et sur l'émission vue par les rayons indirects. Un multiplicateur de 0 donne environ 0,004, pas zéro exact. Il n'agit pas sur l'image d'animation propre à un modèle de brosse (une porte qui montre son état).

- Choix du shader : le menu (page « Emissive », § 5 bis), `ledit_emissive_select <index|nom>` ou `ledit_emissive_select sel` (le shader de la surface sélectionnée). L'outil 5 règle ensuite le multiplicateur (§ 4). Le panneau donne « emissive shader N », le nom, le nombre de polygones et le multiplicateur.
- Le multiplicateur d'un shader vaut pour toutes ses surfaces. Il se **multiplie** avec le facteur de chaque surface (§ 8 ter bis).
- Commandes : `ledit_emissive_list [filtre]`, `ledit_emissive <index|nom> <échelle>`, `ledit_emissive_reset <index|nom|all>`, `ledit_emissive_select <index|nom|none|sel>`, `ledit_emissive_surfaces [index|nom]`.
- Les changements passent par l'undo. Des crans de molette consécutifs sur le même shader forment une seule entrée.
- La sauvegarde écrit un bloc `emissive` par shader dont le multiplicateur n'est pas 1 (§ 10).

## 8 ter bis. Surfaces émissives

Une **surface émissive** est l'ensemble des polygones émissifs qui viennent de la même face (surface) du BSP. Elle est un enregistrement comme une lumière : id dans la même suite que les autres, source `emissive`, type `surface`. Seule la **lumière émise** change : l'aspect de la surface (table des matériaux, lueur de la texture) ne change pas.

- **Regroupement.** Le renderer note la face d'origine de chaque polygone au moment où il les crée. Le groupe d'une face compte les polygones qui ont vraiment été créés : le tracer jette un triangle dont le centre est hors du monde (cluster -1) ou dégénéré, et ce triangle n'appartient à aucun groupe. Une face dont tous les triangles ne sont pas devenus des lumières est donc groupée par ceux qui le sont. L'origine est toujours connue (aucun repli par shader et adjacence n'est utilisé). Les polygones des modèles de brosse (portes, ascenseurs) ne sont pas dans le monde statique : ils sont instanciés à chaque image comme des lumières dynamiques et ne sont pas éditables un par un ; le multiplicateur du shader les touche toujours.
- **Clé.** Le shader, le rang de la face parmi toutes les faces du même shader dans l'ordre de chargement du BSP (`face` dans le fichier), et `check`, le centre de la boîte du groupe, pour détecter un BSP recompilé.
- **Valeurs.** Intensité : un facteur de 0 à 100 (1 = inchangé) sur la lumière des polygones. Couleur : une teinte (0 à 1 par canal) qui multiplie la couleur de la lumière. Style de lumière (§ 8 bis). Le facteur du shader (§ 8 ter) s'applique en plus.
- **Désactiver** (outil 8, `ledit_delete`) : les polygones deviennent inertes mais gardent leurs slots, et la surface reste visible, jusqu'à `ledit_compact` (§ 8 quinquies). **Restaurer** (Alt, `pt_ledit_restore <id>`, undo) les rallume. **Muet** et **solo** (outil 9) marchent par id d'enregistrement ; ils ne sont ni des éditions ni sauvegardés.
- **Revert** (`ledit_revert`) rend le facteur 1, la teinte blanche et le style d'origine ; il ne touche pas à l'état désactivé. L'Alt de l'outil 5 remet la propriété active seulement.
- Une surface n'est ni déplacée, ni clonée, ni orientée.
- Chaque action sur un groupe est une seule entrée d'undo, quel que soit son nombre de triangles.

## 8 quater. Ciel et soleil de la map

Chaque map peut avoir son propre réglage du ciel. Il n'écrit jamais les cvars archivées (`physical_sky`, `sun_*`).

| Mode | Effet |
|---|---|
| `global` | Pas de réglage pour la map : les cvars `physical_sky` et `sun_*` s'appliquent. |
| `skybox` | Skybox de la map, sans soleil. |
| `physical` | Ciel physique (terre), avec le soleil du réglage. |
| `hybrid` | La skybox de la map reste, un soleil analytique éclaire la scène. Corrige l'absence de soleil quand `physical_sky` vaut 0. |

- Convention : la direction vers le soleil vaut (cos az · cos él, sin az · cos él, sin él) dans les axes du monde, comme `sun_azimuth` et `sun_elevation`.
- Une commande de soleil en mode `global` passe d'abord en `physical` (si `physical_sky` est non nul) ou en `hybrid`. Sur `skybox`, elle passe en `hybrid`.
- `ledit_sun_color` : 0 à 10 par composante, 1 = blanc. `ledit_sun_angle` : 1 à 10°.
- Calibrage du mode `hybrid` : couleur du soleil × luminosité × `pt_env_scale` × 0,9 (estimation fixe de la transmittance).
- L'undo couvre les changements de ciel. `ledit_sky_reset` ou `ledit_sky global` retire le réglage de la map.
- Renderer : `pt_sky_print`, `pt_sky_set`, `pt_sky_reset` (§ 6) ; cvars `pt_sky_per_map`, `pt_sky_q3map_sun` et `pt_ambient_floor_scale` (§ 7).
- Le mode `global` ne désigne plus « pas de bloc » : un bloc `global` peut ne porter que le plancher ambiant (`sky global`, § 10). `ledit_sky global` et `ledit_sky_reset` retirent le ciel de la map et gardent le plancher.

**Nuage (`sky_diffuse`, 0 à 1).** Simule une couche de nuages qui empêche le soleil direct. 0 ne change rien ; 1 donne un ciel entièrement couvert. Modes `physical` et `hybrid` seulement (sans soleil, la valeur est ignorée).
- Soleil direct : la radiance de l'échantillonnage du soleil est multipliée par (1 − valeur). Le disque échantillonné s'élargit avec le carré de la valeur : la tangente de son demi-angle passe de celle de `sun_angle` à 0,6 (environ 31°). Les ombres s'adoucissent et, à 1, le soleil direct est nul (aucun rayon d'ombre n'est tiré). La brillance spéculaire du soleil suit (multipliée par (1 − valeur), lobe élargi).
- Énergie rendue : la scène ne s'assombrit pas. Le ciel utilisé comme lumière (polygones de ciel et rayons de rebond qui s'échappent vers le ciel) reçoit une radiance uniforme = couleur du soleil × valeur × sin(élévation) / π. Elle donne, sur une surface horizontale, le même éclairement que le soleil retiré (E = π·L) ; sin(élévation) est borné à 0,1 en bas. Choix : la fonction `env_map(…, sans soleil)` du tracer est le seul point que partagent l'échantillonnage des lumières de ciel, le ReSTIR, les rebonds, le cache de radiance et les sprites éclairés : un seul ajout les couvre tous, sans nouveau rayon et sans bruit supplémentaire. Conséquence : seul un ciel visible éclaire (ouvertures, polygones de ciel) ; une pièce fermée reste sombre, comme avec un vrai ciel couvert.
- Vue : le disque du soleil s'efface du ciel visible (mélange avec le ciel sans soleil) et les rayons de lumière (`god_rays`) disparaissent avec le soleil direct.
- Limites : la teinte de la lumière rendue est celle du soleil, pas celle de la voûte bleue ; la luminance plus forte au zénith d'un vrai ciel couvert n'est pas copiée.

**Plancher ambiant (`ambient_color`, `ambient_intensity`).** Par map, dans tous les modes. Intensité 0 (défaut) : aucun effet, la map est rendue comme avant (un seul test dans le shader de composition).
- C'est un **plancher**, pas un ajout : l'éclairement d'un point de surface, par canal, n'est jamais sous `couleur × intensité × pt_ambient_floor_scale`. Un point déjà plus lumineux ne change pas. Le plancher se multiplie ensuite par l'albédo, comme tout l'éclairement, puis par le débit (miroirs, verre).
- Où : dans la fonction de composition finale (`composite_color`, `brdf.glsl`), sur la somme éclairement indirect (LF) + éclairement direct (HF), **après** le débruiteur et **avant** la multiplication par l'albédo. Ce même point sert A-SVGF (`asvgf_atrous`), NRD (`nrd_composite`) et le rendu sans débruiteur (`compositing`) : le plancher est donc sans bruit, n'entre dans aucun historique temporel (pas de traînée, pas de démodulation faussée) et ne coûte aucun rayon. Les pixels de ciel (albédo 0) ne changent pas.
- Ce n'est pas une source de lumière : le plancher n'éclaire pas les autres surfaces (pas de rebond), n'agit pas sur le spéculaire, ni sur les pixels de ciel, ni sur les sprites ou les faisceaux. L'unité est celle de l'éclairement avant l'albédo (intensité 1 : une surface blanche prend au moins la couleur du plancher avant le mappage de tons). Régler à l'œil avec `pt_ambient_floor_scale`.
- Un bloc `global` du plancher seul donne `sky global` : le ciel reste celui des cvars.
- Corrections liées : `sun_color_r/g/b` rafraîchissent maintenant le ciel ; la radiance du sol atteint les shaders ; le préréglage `physical_sky` n'écrase plus le réglage d'une map.


## 8 quinquies. Compactage et mode restauration

Supprimer une lumière, ou désactiver une surface émissive, ne libère rien : elle garde ses slots du tracer (limite de 4096, § 11), et reste visible, visée et restaurable. `ledit_compact` libère ces slots sans recharger la map. Une surface émissive tient un slot par triangle : la désactiver puis compacter rend tous ces slots, ce qui redonne de la place sous la limite.

- **`ledit_compact`** : pour chaque lumière ou surface supprimée (désactivée ou effacée), le renderer rend ses slots, puis tasse les slots utilisés vers le début de la réserve et reconstruit les listes de clusters **une seule fois**. Les polygones émissifs n'ont plus de place réservée au début de la réserve : un polygone vivant, d'une lumière ou d'une surface, peut changer de slot. Il perd un instant son historique de ReSTIR. La commande affiche les lumières et les slots libérés, puis les slots utilisés / limite. Sans lumière supprimée, elle l'indique et ne fait rien. Elle ne quitte pas le mode, ne relit pas le `.lgt` et vide la prise en main.
- **État « compactée »** : la lumière reste en mémoire avec son id, ses valeurs et son drapeau désactivée ou effacée. Le `.lgt` ne change pas : une lumière d'origine garde son bloc désactivé, une lumière ajoutée effacée n'est toujours pas écrite. Les ids, les changements non sauvegardés et l'undo restent.
- **Disparition** : hors du mode restauration, une lumière compactée n'a plus d'icône, d'étiquette, de cadre, elle n'est plus visée ni sélectionnée, et le menu la cache (sauf avec le filtre `disabled`).
- **Mode restauration** (`ledit_restore`) : les lumières compactées s'affichent (icône rouge barrée, étiquette « compacted ») et seul l'outil 8 les vise. **Alt** les restaure. Le bouton « Restore » du menu et `pt_ledit_restore <id>` marchent dans tous les cas. Le bas de l'écran affiche `RESTORE MODE`. Le mode se coupe à la sortie du light edit et au changement de map.
- **Restauration d'une lumière compactée** : elle prend de nouveaux slots, avec les mêmes contrôles qu'un ajout (`no free light slot`, `cluster full`). En cas d'échec, la commande affiche l'erreur et la lumière reste compactée. Pour une surface émissive, le contrôle `cluster full` compte tous ses polygones dans chaque cluster que ceux-ci éclairent (la liste d'un cluster tient 1024 lumières au plus) : une surface qui ne tient pas reste compactée, et `no free light slot` apparaît si la réserve n'a pas autant de slots libres que de triangles.
- **Undo** : annuler la suppression d'une lumière compactée est une restauration. Elle réalloue les slots, et peut échouer comme ci-dessus (le message de groupe partiel s'affiche).
- **Recharger** (`ledit_reload`, `lightedit 0 discard`) : les lumières reprennent l'état du fichier ; celles qui redeviennent actives reprennent des slots.

## 9. Binds conseillés

Les touches des armes, le tir, la molette et `[` / `]` suffisent pour les outils. Le fichier `docs/lightedit_binds.cfg` propose des binds sur le pavé numérique ; `ledit_writebinds` l'écrit dans le homepath, `exec lightedit_binds.cfg` l'applique. Il lie aussi `KP_SLASH` à `ledit_sun_here` (le soleil vient de la direction du regard). Rien n'est exécuté automatiquement. Autre exemple (à adapter) :

```
bind F6 "lightedit"
bind F7 "ledit_save"
bind F8 "ledit_undo"
bind F9 "ledit_redo"
bind DEL "ledit_delete"
bind F10 "ledit_xray_toggle"
bind F11 "ledit_snap_toggle"
bind F12 "ledit_grid_next"
```

Vérifiez que la molette est liée à `weapnext` / `weapprev` (`bind MWHEELUP weapnext`, `bind MWHEELDOWN weapprev`) et que `[` / `]` sont liés à `invprev` / `invnext`.

## 10. Le fichier `.lgt` (format v2, API de version 7)

Emplacement : `maps/<map>.lgt` dans le homepath (par exemple `Documents\My Games\SWGL\SWGL\maps`). Une copie de sécurité `maps/<map>.lgt.bak` est faite avant chaque `ledit_save`.

Au chargement de la map, le fichier est **toujours** lu :
- les blocs **lgt** (lumières reconstruites) sont chargés seulement si la map n'a aucune entité `light`, comme avant. Sinon, ils sont conservés tels quels à la sauvegarde ;
- les surcharges d'entités, les désactivations et les lumières ajoutées sont **toujours** appliquées.

Exemple :

```
version 2
// light edit file of kejim_base; lgt blocks, entity overrides, added lights
lights 0
{                         // lumière reconstruite jamais modifiée (forme v1)
	origin 10 20 30
	color 1 0.9 0.8
	intensity 4200
	rays 6
	error 3.1
}
{                         // lumière reconstruite modifiée ou désactivée
	source lgt
	edited 1
	origin -1700 -27000 560
	color 0.5 0.5 1
	intensity -3000       // négative si désactivée
	disabled 1
	rays 4
	error 2
}
{                         // surcharge d'une entité light du BSP
	source entity
	ent 63                // rang parmi les entités "classname light" du BSP
	check 384 -1536 -8    // origine d'origine, pour détecter un BSP recompilé
	origin 384 -1536 -8
	color 1 0 0
	light 20
	radius 16
	intensity -1          // toujours -1
}
{                         // lumière ajoutée
	source added
	origin -2315.5 -1492.7 12.0
	color 0.2 1 0.2
	intensity 6000
	radius 8
}
{                         // spot ajouté
	source added
	origin -1312 -4512 4576
	color 1 0.9 0.8
	intensity 2000
	radius 8
	type spot             // absent pour une sphère
	dir 0 0 -1            // direction du spot
	cone_outer 35         // demi-angle extérieur, en degrés
	cone_inner 25         // demi-angle intérieur, en degrés
}
{                         // rectangle ajouté
	source added
	origin 100 200 300
	color 1 0.9 0.8
	intensity 2000
	type rect
	dir 0 0 -1            // normale : côté éclairé
	width 128
	height 48
	roll 45               // degrés
	two_sided 1           // 0 ou 1
	style 4               // absent si 0 (ou égal au style d'origine)
}
{                         // multiplicateur d'un shader émissif
	source emissive
	shader "textures/dossier/exemple"   // nom du shader
	scale 8
}
{                         // une surface émissive modifiée ou désactivée
	source emissive_surface
	shader "textures/dossier/exemple"   // nom du shader
	face 3                // rang de la face parmi les faces de ce shader dans le BSP
	check 120 -64 48      // centre de la boîte du groupe, pour détecter un BSP recompilé
	scale 2.5             // facteur de la lumière (absent si 1)
	color 1 0.6 0.4       // teinte (absente si blanche)
	style 4               // absent si 0
	disabled 1            // absent si la surface est active
}
global                    // ciel et soleil de la map
{
	source global
	sky hybrid            // skybox, physical ou hybrid
	sun_azimuth 120
	sun_elevation 35
	sun_color 1 0.9 0.8
	sun_brightness 1
	sun_angle 4
	sky_diffuse 0.4       // absent si 0
	ambient_color 0.6 0.7 1   // plancher ambiant, absent si ambient_intensity vaut 0
	ambient_intensity 0.5
}
```

Règles :
- `lights N` donne le nombre de blocs lgt (information seulement).
- Les blocs lgt sont écrits en premier, dans leur ordre : leur rang (`lgt N`) reste stable d'une sauvegarde à l'autre.
- Chaque clé a 1 ou 3 valeurs. Un ancien binaire (parseur v1) lit le fichier sans se décaler : il ignore les blocs d'intensité négative (surcharges, désactivations) et lit les lumières ajoutées comme des lumières reconstruites.
- Si `ent` et `check` ne correspondent plus au BSP (map recompilée), la surcharge est ignorée avec un avertissement, mais elle est conservée dans le fichier.
- Un bloc `emissive_surface` est écrit pour chaque surface émissive modifiée (facteur, teinte ou style) ou désactivée, y compris compactée (la compaction ne change pas le fichier). Il n'a ni clé `origin` ni clé `intensity` : un parseur v1 le saute, et un binaire v2 plus ancien le conserve comme un bloc de source inconnue. Le facteur est la clé `scale`, comme dans le bloc `emissive`. Si `shader` et `face` ne trouvent aucune surface, ou si `check` s'écarte de plus d'une unité du centre du groupe (BSP recompilé), le bloc est ignoré avec un avertissement et conservé dans le fichier.
- Les clés `type`, `dir`, `cone_outer` et `cone_inner` existent aussi pour les blocs lgt édités et les surcharges d'entités. Une surcharge qui change seulement la couleur d'un spot d'origine garde sa direction et ses cônes. `type sphere` note un spot d'origine converti en sphère.
- Le rectangle utilise les clés `type rect`, `dir`, `width`, `height`, `roll` et `two_sided`. La clé `style` est écrite seulement si elle n'est pas 0 ou si elle diffère du style d'origine.
- Un bloc `emissive` est écrit pour chaque shader dont le multiplicateur n'est pas 1. Un shader absent de la map garde son bloc dans le fichier (avertissement au chargement).
- Le bloc `global` est écrit si le mode n'est pas `global` ou si le plancher ambiant est actif. Un bloc qui ne porte que le plancher a `sky global` et aucune clé de soleil. Les clés `sky_diffuse`, `ambient_color` et `ambient_intensity` sont écrites seulement si elles sont non nulles : un fichier sans nuage ni plancher reste identique à celui d'avant. Un ancien binaire ignore ces clés (clés inconnues) et lit `sky global` comme « pas de ciel ». Seul le premier bloc `global` est lu. Une map qui a des entités `light` et pas de `.lgt` reçoit un fichier qui ne contient que ces blocs.
- Compatibilité : le parseur v1 saute le mot `global` et rejette les blocs sans `intensity`. Chaque clé a toujours une ou trois valeurs.
- `pt_lightgen` compte les blocs `emissive`, `emissive_surface` et `global` comme des éditions : il refuse d'écraser le fichier sans `force` ; avec `force`, il les garde.
- `ledit_save`, `ledit_reload` et `lightedit 0 discard` couvrent aussi ces blocs.
- Version d'API 7 (la version 7 ajoute à `rtxSkyDesc_t` les champs `skyDiffuse`, `ambientColor` et `ambientIntensity` ; `SetSky` avec le mode `RTX_SKY_GLOBAL` retire le ciel mais garde le plancher de la description). La version 6 ajoutait la source `RTX_LSRC_EMISSIVE`, le type `RTX_LTYPE_SURFACE`, `GetEmissiveSurface` et le champ `numEmissivePolys`, et retire `CountEmissive`, `GetEmissive` et `EmissiveShaderOf` : un polygone émissif n'a plus d'index, sa surface est un enregistrement ; la version 5 avait ajouté `Compact`, le drapeau `RTX_LFLAG_COMPACTED` et les compteurs `numCompacted` et `slotsUsed`). Le renderer, la DLL du jeu et l'exe doivent venir du même build. Le cgame et l'interface refusent une autre version.
- Les mutes et le solo (outil 9) ne sont jamais écrits.
- Les blocs de `source` inconnue sont conservés tels quels.
- `pt_lightgen <map>` refuse d'écraser un fichier qui contient des éditions. `pt_lightgen <map> force` l'écrase après une copie en `.lgt.bak`.


## 11. Limitations connues

- **RTX seulement.** Le raster utilise les lightmaps et la lightgrid, qui ne changent pas.
- **Multiplicateur émissif** : l'aspect de la surface suit le multiplicateur par pas de 4 % (code d'échelle sur 8 bits), et 0 donne environ 0,004. Les images d'animation propres à un modèle de brosse (porte) gardent le facteur 1. Une surface émissive qui n'est pas un shader « glow » reçoit aussi `pt_glow_scale` pour ses rayons indirects dès que son multiplicateur n'est pas 1.
- **Soleil hybride** : le calibrage est une estimation (transmittance fixe de 0,9).
- **Ciel** : en mode `global`, un changement de ciel affiche les anciennes valeurs pendant une image.
- **Nuage** : il n'agit qu'avec un soleil (`physical`, `hybrid`) et qu'à travers le ciel visible : sans ouverture de ciel, une pièce fermée ne reçoit pas la lumière rendue. La teinte de cette lumière est celle du soleil. La pondération de l'échantillonnage des lumières de ciel ne tient pas compte du supplément : un peu plus de bruit à valeur élevée.
- **Plancher ambiant** : ce n'est pas une lumière (pas de rebond, pas de spéculaire). Sur des surfaces déjà éclairées plus fort que le plancher, il ne fait rien. Il agit aussi sur les surfaces vues dans un miroir.
- **Shaders** : la construction ne régénère pas les blobs de `shader_data_rtx.c` ; après une retouche du GLSL, compiler avec `glslangValidator -DGLSL` et remplacer les blobs concernés.
- **Rectangles** : déplacer ou tourner un rectangle reconstruit les listes de clusters ; sur une très grande map, un à-coup est possible. Le choix d'un rectangle vise son centre seulement.
- **Clavier** : un raccourci ne marche pas si l'utilisateur a lié une combinaison de touches sur la même touche (le moteur la remplace avant le mode). Le nom d'une lumière se saisit par la console (`ledit_set name`).
- **Menu** : les largeurs de colonnes sont fixes ; le menu suit la lumière principale du jeu, pas toute la sélection multiple ; « Apply sun » envoie une commande, donc une entrée d'undo, par valeur changée.
- **Sélection** : pas de sélection par boîte, ni de « tout sélectionner de la même couleur ».
- **Limite de lumières** : 4096 lumières en tout dans le tracer (`MAX_LIGHT_POLYS`, constante des shaders), dont 128 gardées pour les lumières dynamiques. Les surfaces émissives comptent aussi, un slot par triangle : sur kejim_post, environ 90 places restent. Désactiver des surfaces puis `ledit_compact` rend leurs slots. Au-delà, l'ajout est refusé (`no free light slot`).
- **1024 lumières par cluster** : un ajout est refusé si un cluster visible est plein. `Restore` et la fin d'un muet font le même contrôle. La fin d'un solo ne peut pas être refusée : si un cluster déborde, un message le dit (« cluster full: N clusters truncated »).
- **Surfaces émissives** (néons, panneaux) : éditables une par une (facteur, teinte, style, désactivation, muet, solo), mais pas déplaçables, orientables ni clonables : la surface est de la géométrie de la map. Seule la lumière émise change, jamais l'aspect de la surface. Elles sont affichées en points gris avec `ledit_show 2` (500 au plus, à moins de 2048 unités, avec le test d'occlusion des lumières), et on ne les vise qu'avec `ledit_show` ≥ 2. Les polygones des modèles de brosse (portes, ascenseurs) ne sont pas éditables un par un : seul le multiplicateur de leur shader les touche. Un triangle de la face qui n'est pas devenu une lumière (centre hors du monde) n'appartient pas au groupe. Restaurer une surface demande un slot libre par triangle et la place dans les listes des clusters (1024 lumières au plus par cluster) : sinon elle reste compactée ou désactivée (`cluster full`).
- **Une lumière d'origine dans un solide** reste une sphère : on ne peut la convertir en spot qu'après l'avoir sortie du solide.
- **Pas d'icônes au-delà de 3000 unités** sans `ledit_xray`. Les modèles (caisses, portes) ne cachent pas les icônes et ne sont pas touchés par le fantôme ni par le point visé de l'outil 4.
- **Cônes larges** : le cercle du bord est limité à 256 unités de rayon ; il est alors dessiné plus près du sommet du cône.
- **Température** : la teinte d'un corps noir remplace la couleur entière ; une couleur loin de cette courbe saute au premier cran.
- **Undo** : si le renderer refuse une étape d'un groupe (par exemple un cluster plein), le groupe change de pile quand même et l'état peut être partiel ; un message l'indique.
- Un **vid_restart** recharge la map côté renderer : les changements non sauvegardés sont perdus.
- Chaque ajout, clone ou rechargement consomme un emplacement. Supprimer une lumière ne le libère pas : seul `ledit_compact` rend les emplacements des lumières supprimées (§ 8 quinquies), sans recharger la map. Restaurer une lumière compactée demande de nouveaux emplacements et peut échouer. L'agrandissement de la réserve (par blocs de 64) provoque un court arrêt du GPU.
- Les débruiteurs gardent une traînée de quelques images après un changement ; `ledit_still_accum 1` donne une image de référence quand la vue est fixe.
- La sauvegarde de la partie est refusée en silence pendant le mode.

## 12. Procédure de test pas à pas

1. Lancer le jeu avec `cl_renderer rdsp-vulkan` et `r_rtx 1`, puis `devmap t1_fatal`, et passer la cinématique (touche Utiliser).
2. `lightedit` : le HUD normal disparaît ; l'overlay affiche « LIGHT EDIT tool 1 Select (keys 1 … 9) ».
3. **Création** : touche **2**, **Alt** (« new lights are spots »), viser le sol, **Tir** : un spot éclaire le sol ; le cône filaire et le disque éclairé apparaissent.
4. **Orientation** : touche **4**, **molette** : le cône s'élargit par pas de 15° ; **Alt** puis **molette** : l'angle intérieur change ; viser un autre point, **Tir** : le spot s'oriente vers lui.
5. **Propriétés** : touche **5**, `]` jusqu'à « hue », **molette** : la couleur tourne ; `]` « temperature », **molette** : la couleur passe du bleu à l'orange. `ledit_history` montre une seule entrée par série de crans.
6. **Pipette** : touche **6**, viser une lumière, **Tir** (« copied light N ») ; touche **2**, **Tir** : la nouvelle lumière prend ses valeurs. Touche **6**, **molette** jusqu'à « colour », viser une autre lumière, **Alt** : seule la couleur est collée.
7. **Sélection multiple** : touche **1**, **Tir** sur une lumière, **Alt** sur une autre : le panneau affiche « 2 lights ». `ledit_set intensity 5000` change les deux.
8. **Clonage** : touche **7**, **molette** pour 4 copies, **Alt** : une rangée de 4 lumières apparaît ; **Tir** sur une lumière : la copie est saisie (outil 3).
9. **Contraintes** : touche **3**, **Tir maintenu** sur une lumière, **Alt** (« constraint X axis »), tourner : la lumière glisse sur X ; relâcher.
10. **Grille** : `ledit_snap_toggle`, `ledit_grid_next` : le bas de l'écran affiche le pas ; un nouvel ajout tombe sur la grille.
11. **Solo / muet** : touche **9**, **Tir** sur une lumière : les autres s'éteignent (`SOLO id` en bas) ; **Tir** à nouveau : retour normal ; **Alt** : la lumière visée s'éteint (icône grise), **Alt** à nouveau : elle revient.
12. **Accumulation** : `ledit_still_accum 1`, ne plus bouger : au bout d'une demi-seconde l'image se stabilise (`pt_accumulation_rendering` vaut 1) ; bouger : la valeur revient à 0.
13. **Durée** : tourner sur soi-même plusieurs fois et voler une minute avec une sélection et un cône affichés : l'image ne gèle pas et la console n'affiche pas « MAX_REFENTITIES ».
14. `ledit_save`, `lightedit 0`, `devmap t1_fatal` : la console affiche « light edit: maps/t1_fatal.lgt: … added » ; les spots sont relus avec leur direction et leurs cônes (`pt_ledit_list`).
15. `pt_lightgen t1_fatal` : refus (« holds light edits »).
16. Avec `cl_renderer rdsp_swgl` : `lightedit` répond « needs the RTX renderer » et le jeu continue normalement.
17. **Clavier** : sélectionner une lumière, **Entrée**, taper `5000`, **Entrée** : l'intensité change ; **Ctrl+Z** : elle revient ; **Ctrl+Y** : elle repart ; **Suppr** : la lumière est supprimée ; **Ctrl+Z** : elle revient. **Entrée**, `9`, **Échap** : rien ne change et le menu du jeu ne s'ouvre pas.
18. **Console** : ouvrir la console, appuyer sur **Ctrl+Z** : rien n'est annulé. Fermer la console.
19. **Aller à** : `ledit_select <id>`, puis **Utiliser** (ou `ledit_goto`) : la caméra se place devant la lumière, tournée vers elle.
20. **Menu** : **Ctrl+M** : le menu s'ouvre à droite ; filtre « Added », recherche `copy` : la liste se réduit ; cliquer une ligne, « Load from light », bouger la teinte, « Apply colour » : la lumière change de couleur, **Ctrl+Z** après fermeture l'annule. **Échap** ferme le menu.
21. **Étiquettes et filtres** : `ledit_label 2`, `ledit_show 3` : noms et valeurs à côté des icônes, points gris des émissives, points magenta des lumières dynamiques (tirer au blaster hors du mode, puis revenir).
22. **Sortie** : `lightedit 0`, puis **Ctrl+Z**, **Suppr**, **Entrée** : ces touches reprennent leur bind normal ; après `devmap`, idem.

Itération 4 (rectangles, styles, émissifs, ciel). Les étapes se font en mode `lightedit` (`devmap` puis `lightedit`).

23. **Rectangle** : `devmap t1_fatal`, `lightedit`, touche **2**, **Alt** deux fois (« new lights are rects »), viser un mur, **Tir** : un rectangle de 32 × 32 apparaît, éclairant à l'opposé du mur ; le rectangle filaire vert menthe montre les bords et la flèche de la normale.
24. **Taille et orientation** : touche **4**, **Tir** sur un autre point : la normale se tourne vers lui. **Molette** (cible par défaut : le roulis) : le trait +U tourne. **Alt** (« the wheel edits the width »), **molette** : la largeur change ; **Alt** à nouveau pour la hauteur.
25. **Propriétés** : touche **5**, `]` jusqu'à « width » (le cycle saute les cônes), `ledit_set width 128`, `ledit_set height 48`, `ledit_set roll 45`, puis `]` jusqu'à « two-sided », **molette** : le rectangle éclaire des deux côtés (diagonales et flèche arrière). Vérifier avec `ledit_get` et `pt_ledit_list`.
26. **Style** : `]` jusqu'à « style », **molette** jusqu'à 4 (« fast strobe ») : la lumière clignote. `pt_light_styles 0` : elle reste fixe ; remettre `pt_light_styles 1`.
27. **Sauvegarde** : `ledit_save`, `lightedit 0`, `devmap t1_fatal` : le message de chargement donne « 1 added » ; le rectangle garde sa taille, son roulis, ses deux faces et son style.
28. **Pipette** : touche **6**, **Tir** sur le rectangle, touche **2** : la création donne un rectangle de même taille ; **molette** jusqu'au filtre « shape », puis **Alt** sur une sphère : elle devient un rectangle.
29. **Ciel** (`devmap hoth2`, `lightedit`) : `ledit_sky` affiche le mode ; `ledit_sky skybox` (pas de soleil), `ledit_sky hybrid` (le soleil éclaire la scène et un repère jaune le marque), `ledit_sky physical`. Tester `ledit_sun 120 35`, `ledit_sun_here`, `ledit_sun_color 1 0.7 0.4`, `ledit_sun_brightness 2`, `ledit_sun_angle 4`. Le panneau (rien de sélectionné) donne « sky: hybrid (map) * » et « sun az … el … ». **Ctrl+Z** annule chaque changement. `ledit_save`, `devmap hoth2` : le ciel est relu. `ledit_sky_reset` : retour au ciel global.
30. **Émissifs** (`devmap kejim_base`, `lightedit`) : `ledit_emissive_list light` liste les shaders ; `ledit_emissive_select <index>` en choisit un : le panneau donne « emissive shader N » et des cadres jaunes marquent ses surfaces (outils 1, 5 ou 6). Touche **5**, **Molette** jusqu'à environ 8 : la scène s'éclaire. **Alt** : retour à 1. `ledit_emissive <nom> 8`, **Ctrl+Z**, `ledit_emissive_reset all`. Refaire `ledit_emissive … 8`, `ledit_save`, `devmap kejim_base` : le multiplicateur est relu (`ledit_emissive_list` marque `*`).
31. **Fichier** : ouvrir `maps/<map>.lgt` : les blocs `emissive` et `global` sont présents. `pt_lightgen <map>` refuse sans `force`.
32. **Aspect des émissifs** (`devmap kejim_base`, `lightedit`) : `ledit_emissive_select <index>`, outil **5**, **molette** vers 8 : la lumière projetée augmente et la surface du shader (néon, écran) brille davantage ; molette vers 0 : la surface s'éteint presque. **Alt** : retour à 1. **Ctrl+Z** après un changement : la surface revient aussi.
33. **Choix d'un shader émissif** : `ledit_emissive_select <index>` : cadres jaunes sur ses surfaces, panneau « emissive shader N » ; outil **1**, **Tir** dans le vide : le choix disparaît. `ledit_emissive_select none` le désélectionne aussi. Le tir sur un point gris ne choisit plus un shader : il sélectionne la surface (étapes 42 à 53).
34. **Menu, nom et cônes** : **Ctrl+M**, page « Lights », cliquer une ligne : les champs Intensity, Radius, Name et (spot) Cone se remplissent. Changer « Name », « Apply » : l'étiquette (`ledit_label 1`) et `ledit_get` donnent le nouveau nom. Sur un spot, changer « Cone ext » et « int », « Apply » : le cône filaire suit. « Apply » des cônes sur une sphère répond « is not a spot ».
35. **Menu, suivi du jeu** : fermer le menu (Échap), outil **1**, **Tir** sur une autre lumière, **Ctrl+M** : la ligne de cette lumière est choisie dans la liste et visible. Menu ouvert, sélectionner une autre lumière dans le jeu n'est pas possible à la souris : utiliser `ledit_select <id>` dans la console ouverte par-dessus : la ligne change. Même test avec un shader émissif (page « Emissive »).
36. **Menu, ciel** : page « Sky and sun » : les deux lignes donnent le ciel en vigueur. Cliquer « Hybrid » : le mode passe à `hybrid (map)`. Changer « Azimuth » et « Elevation », « Apply sun » : le repère jaune du soleil bouge. Changer « Brightness », « Colour r g b » et « Angle », « Apply sun ». **Ctrl+Z** après fermeture du menu annule chaque changement, et les champs de la page reprennent les valeurs. « Sun here » aligne le soleil sur la vue, « Reset sky » revient au ciel global.
37. **Menu, émissifs** : page « Emissive » : la liste donne index, polygones, multiplicateur, nom. Taper un morceau de nom dans « Search » : la liste se réduit. Cliquer une ligne : le champ « Multiplier » donne son multiplicateur et le jeu montre les cadres jaunes. Taper 8, « Set » : la colonne affiche `8 *`. « Reset » remet 1, « Reset all » remet tous les shaders à 1. `ledit_save`, `devmap kejim_base` : les multiplicateurs sont relus.
38. **Compactage** (`devmap t1_fatal`, `lightedit`) : noter les slots du bas de l'écran. Outil **8**, supprimer deux lumières d'origine (icônes rouges barrées) et en ajouter puis supprimer une (outil **2**, puis outil **8**) : les slots ne baissent pas, les icônes restent, **Alt** restaure une lumière désactivée. `ledit_compact` : la console donne « compacted N lights, released M slots, slots used U/L », les slots du bas de l'écran baissent, les icônes des lumières supprimées disparaissent, `compacted N` apparaît en bas, les autres lumières n'ont pas changé d'aspect. `ledit_compact` encore : « no removed light to compact ».
39. **Mode restauration** : `ledit_restore` : `RESTORE MODE` en bas, les lumières compactées reviennent en rouge barré (étiquette « compacted » avec `ledit_label 1` sur la lumière visée). Outil **1** : elles ne sont pas visées ; outil **8** : elles sont visées. **Alt** sur une lumière compactée : elle redevient active, les slots remontent. `ledit_restore 0` : plus de `RESTORE MODE`, les lumières encore compactées disparaissent.
40. **Undo et refus** : `ledit_compact` après une suppression, puis **Ctrl+Z** : la lumière revient (nouveaux slots). Pour le refus, remplir la réserve (`pt_ledit_stats` donne les slots), puis restaurer : « no free light slot » ou « cluster full », et la lumière reste compactée. Un compactage avec une lumière supprimée dans la sélection ou saisie vide la sélection et relâche la saisie.
41. **Menu et fichier** : **Ctrl+M**, filtre « Disabled » : les lumières compactées sont listées (état C), les autres filtres les cachent ; « Restore » en restaure une, « Select » sur une ligne C répond « compacted, restore it first ». `ledit_save`, `devmap t1_fatal` : le fichier garde les blocs désactivés des lumières d'origine compactées, et n'écrit pas les lumières ajoutées effacées. Après `ledit_compact`, `lightedit 0`, `lightedit` : le mode restauration est coupé.
Itération 5 (surfaces émissives une par une). Les étapes se font en mode `lightedit`, avec `ledit_show 2`.

42. **Choix d'une surface** (`devmap kejim_base`, `lightedit`, `ledit_show 2`) : de petits points gris marquent les surfaces émissives. Outil **1**, **Tir** sur un point : le panneau donne « emissive surface N », le shader, les triangles et « factor: surface 1 x shader 1 » ; le contour de la face est dessiné en jaune. **Tir** dans le vide : le choix disparaît. `ledit_emissive_surfaces light` liste les surfaces des shaders dont le nom contient « light ».
43. **Facteur, teinte et style** : outil **5**, `]` jusqu'à « intensity », **molette** : seule la lumière projetée par cette face change (la lueur de la texture reste), le panneau donne « intensity: x… » et la valeur d'origine en gris. `]` jusqu'à « hue » (la saturation doit être > 0, sinon `ledit_set color 1 0.4 0.2`) : la lumière prend la teinte. `]` jusqu'à « style », **molette** : la lumière de cette face clignote. `ledit_set radius 4` répond que la lumière est sautée. **Alt** remet la propriété d'origine ; `ledit_revert` rend facteur 1, teinte blanche et style 0.
44. **Facteur du shader** : `ledit_emissive <shader> 4` multiplie encore la lumière de toutes les faces du shader, **en plus** du facteur de la face (essayer sur une face qui a déjà un facteur). `ledit_emissive_select sel` choisit le shader de la face sélectionnée (cadres jaunes sur ses faces, panneau « emissive shader N »), outil **5**, **molette** change son multiplicateur.
45. **Pipette** : outil **6**, **Tir** sur une surface réglée (« copied emissive surface »), puis **Alt** sur une autre surface : filtre « all » colle facteur, teinte et style ; **molette** jusqu'à « colour » colle la teinte seule ; « shape » ne colle rien (message). Copier une lumière normale puis **Alt** sur une surface : seules la couleur (et le style avec « all ») passent, l'intensité est refusée. Outil **2** après avoir copié une surface : le préréglage reste celui des cvars.
46. **Outils sans objet** : outil **3**, **Tir maintenu** sur une surface : « cannot be moved » ; outil **7**, **Tir** : « clone does not apply » ; outil **4** : rien ne change.
47. **Désactiver, solo, muet** : outil **8**, **Tir** sur une surface : icône rouge barrée, la lumière de la face disparaît mais les slots du bas de l'écran ne baissent pas. **Alt** la restaure. Outil **9**, **Alt** : la surface devient un contour gris (muette) ; **Tir** sur une autre surface : toutes les lumières, surfaces comprises, s'éteignent sauf elle (`SOLO id`) ; **Tir** à nouveau : retour normal.
48. **Compactage** : désactiver 3 ou 4 grandes surfaces (outil **8**, **Tir**), noter les slots du bas de l'écran, `ledit_compact` : « compacted N lights, released M slots », M égal au nombre de triangles des surfaces, les slots utilisés baissent d'autant, les lumières d'entité et les autres surfaces gardent leur aspect. `ledit_restore` : les surfaces compactées reviennent en rouge barré ; outil **8**, **Alt** sur l'une : elle se rallume et reprend des slots (`pt_ledit_stats`). Remplir la réserve (ajouts) puis restaurer une grande surface : `no free light slot`, la surface reste compactée.
49. **Undo** : après une désactivation, `ledit_compact`, **Ctrl+Z** : la surface revient avec de nouveaux slots. **Ctrl+Z** et **Ctrl+Y** sur un réglage de facteur ou de teinte : une entrée d'undo par action (`ledit_history`), même pour une face de plusieurs triangles.
50. **Fichier** : `ledit_save`, ouvrir `maps/<map>.lgt` : un bloc `source emissive_surface` (clés `shader`, `face`, `check`, `scale`, `color`, `style`, `disabled`) par surface modifiée ou désactivée, y compris les compactées ; les surfaces non modifiées n'ont pas de bloc. `devmap <map>` : les réglages et les désactivations sont relus (« N emissive surfaces » dans le message de chargement). `pt_lightgen <map>` refuse sans `force`.
51. **BSP recompilé** : changer le `check` d'un bloc de plus d'une unité dans le fichier, `devmap` : « emissive surface … does not match the BSP, ignored » ; `ledit_save` garde le bloc tel quel. Un fichier sans bloc `emissive_surface` charge sans changement.
52. **Rechargement** : régler une surface sans sauvegarder, `ledit_reload` (ou `lightedit 0 discard`) : la surface reprend l'état du fichier, y compris celles qui étaient compactées.
53. **Menu** : **Ctrl+M**, filtre « Emissive » : les surfaces sont listées (source `emi R`, type `surface`, intensité = facteur) ; le filtre « All » ne les montre pas ; cliquer une ligne, « Select » puis « Go to » place la caméra devant la surface ; « Delete » / « Restore » marchent ; « Apply colour » règle la teinte.
54. **Nuage** (`devmap hoth2`, `lightedit`, `ledit_sky physical` ou `hybrid`, un sol éclairé par le soleil) : `ledit_sky_diffuse` affiche 0. `ledit_sky_diffuse 0.3` : les ombres du soleil s'adoucissent et la scène garde à peu près la même luminosité. `0.7` : ombres très floues, soleil faible, le ciel éclaire en plus. `1` : plus de soleil direct, plus de rayons de lumière, plus d'ombres du soleil, la scène est éclairée par le ciel seul, le disque du soleil a disparu de la vue. **Ctrl+Z** annule chaque valeur. `ledit_sky skybox` puis `ledit_sky_diffuse 0.5` : la commande indique que le mode n'a pas de soleil et que la valeur est ignorée (l'image ne change pas). `ledit_sky global` (avec `physical_sky 0`), `ledit_sky_diffuse 0.5` : le mode passe à `hybrid` avant. `pt_sky_set sky_diffuse 0.5` fait la même chose. `pt_sky_print` donne `sky_diffuse`.
55. **Plancher ambiant** (`devmap kejim_base`, `lightedit`, un coin sombre) : `ledit_ambient` affiche « no ambient floor ». `ledit_ambient 0.6 0.7 1 0.3` : les zones sombres ne descendent pas sous un bleu pâle, les zones éclairées ne changent pas. `pt_ambient_floor_scale 3`, puis `0` : le plancher monte, puis disparaît. `ledit_ambient 1 1 1 0` ou `ledit_ambient_reset` le coupe. Ni traînée ni bruit ajouté, avec A-SVGF comme avec NRD. Le plancher marche aussi en mode de ciel `global` (`ledit_sky global`, puis `ledit_ambient 1 0.8 0.6 0.2`). **Ctrl+Z** annule.
56. **Fichier et menu** : `ledit_save` avec un nuage et un plancher : `maps/<map>.lgt` donne `sky_diffuse`, `ambient_color` et `ambient_intensity` dans le bloc `global`. Avec `ledit_sky_reset` puis `ledit_save`, le bloc ne garde que `sky global` et le plancher. `devmap <map>` relit les deux. Une map sans plancher ni nuage ne change pas. Page « Sky and sun » : « Cloud (0-1) », « Floor colour r g b » et « Floor intensity » donnent les valeurs en vigueur, « Apply » les envoie, « No floor » coupe le plancher, **Ctrl+Z** annule.
57. **Sortie** : `lightedit 0`.
