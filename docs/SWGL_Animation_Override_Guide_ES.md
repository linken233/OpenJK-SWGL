# SWGL -- Sustitución de animaciones: dar a un personaje o a un NPC sus propias animaciones

2026-10-13

## Qué es una sustitución de animaciones

Todos los modelos humanoides del juego comparten un único esqueleto y un único archivo de animaciones, `models/players/_humanoid/_humanoid.gla`. Por eso todos los personajes se mueven igual. Una **sustitución de animaciones** (*animation override*) permite que un modelo (o un solo NPC) use sus propias animaciones en lugar de algunas de ellas, **sin reconstruir el enorme `_humanoid.gla`**. Se crea un GLA pequeño que contiene solo las animaciones que se quieren cambiar, más un archivo de texto que las nombra. El juego hace el resto. Son **solo datos**: no hay que cambiar el código.

Cómo funciona, en resumen:

1. Al cargar, el renderizador construye un GLA *virtual*: `_humanoid.gla` (con `_weapons.gla`, ver más abajo) y, al final, el GLA propio.
2. El juego lee el `.cfg` con las mismas reglas que `animation.cfg`. Cada animación listada **reemplaza la animación del mismo nombre** del conjunto base. Las animaciones que no se listan siguen siendo las originales.
3. El personaje reproduce las nuevas animaciones en todas partes: en el juego, en el menú de personajes, como jugador, como NPC y en las partidas guardadas.

Solo SP, y solo con los renderizadores **rd-vanilla**, **rd-rend2** y **rd-vulkan** (los tres renderizadores de SWGL).

| Archivo (ejemplo) | Función | Obligatorio |
|---|---|---|
| `models/players/_myanims/_myanims.gla` | Las animaciones: mismo esqueleto que `_humanoid.gla`, solo los fotogramas necesarios. | Sí |
| `models/players/_myanims/_myanims.cfg` (o `animation.cfg`) | Nombre, primer fotograma, duración, bucle y fps de cada animación del GLA. | Sí |
| `models/players/_myanims/animevents.cfg` | Eventos de sonido / efectos de estas animaciones. | No |
| `models/players/<model>/animoverride.cfg` | Hace que un **modelo** use la sustitución. | Solo para la vía "por modelo" |

## 1. Las dos formas de usar una sustitución

| | Por modelo | Por instancia |
|---|---|---|
| Cómo | `animoverride.cfg` en la carpeta del modelo | `animOverride` en el `.npc`, o el comando de consola `animoverride` |
| Quién la recibe | Todos los personajes que usan ese modelo, jugador y NPC | Una sola entidad |
| Duración | Siempre | Hasta el cambio de mapa (consola), o durante toda la vida del NPC (`.npc`) |
| Ideal para | Un personaje con estilo propio (Vader, Malak...) | Pruebas, NPC puntuales, scripts |

Una sustitución por instancia tiene prioridad sobre la del modelo.

## 2. Crear el GLA de animaciones

### 2.1 La regla del esqueleto

El GLA debe tener **exactamente el esqueleto de `_humanoid.gla`**: el mismo número de huesos, con los mismos nombres y los mismos padres, en el mismo orden. El juego los compara hueso por hueso. Si uno difiere, la sustitución **no se usa** y la consola muestra:

```
GLA merge: models/players/_myanims/_myanims.gla does not have the skeleton of models/players/_humanoid/_humanoid.gla, or too many frames
```

Exporte desde su herramienta 3D con el esqueleto `_humanoid` (el flujo con `.glm` / `.gla` es el habitual de Jedi Academy: Blender o 3ds Max + carcass / assimilate). Llame al archivo de animación interno del GLA `models/players/_humanoid/_humanoid`: es lo que buscan el ragdoll y otras comprobaciones. Use la extensión habitual `.gla`.

### 2.2 El presupuesto de fotogramas

Los fotogramas de las tres partes se apilan en un solo archivo: `_humanoid.gla` (21 384 fotogramas), `_weapons.gla` y después su GLA. El número de fotograma debe quedar por debajo de **65 535** en total. Eso deja sitio para unos pocos miles de fotogramas. Si la suma es demasiado grande, la sustitución se rechaza con el mensaje anterior.

### 2.3 Dónde va la carpeta

El nombre de la carpeta es el nombre de la sustitución. La convención es un guion bajo inicial, `_myanims`, para que no se confunda con un modelo de personaje: el GLA es `models/players/_myanims/_myanims.gla`. El juego también acepta la carpeta sin guion bajo (`myanims/myanims.gla`); si existen las dos, gana la que lleva guion bajo.

## 3. El archivo de animaciones

Primero se lee `models/players/_myanims/_myanims.cfg`. Si no existe, el juego lee `models/players/_myanims/animation.cfg`. El formato es el del `animation.cfg` original:

```
// name              start   length   loop   fps
BOTH_STAND1          0       3        0      1
BOTH_STAND2          8       1        -1     60
BOTH_SABERFAST_STANCE 8      1        -1     60
BOTH_MELEE_COMBO_1   10      32       -1     50
```

| Columna | Significado |
|---|---|
| name | Nombre de una animación del juego (`BOTH_STAND1`, `TORSO_RAISEWEAP1`, `LEGS_WALK1`...). La lista es `animTable` en `bg_panimate.cpp` / `anims.h`. Un nombre desconocido se muestra en rojo (`WARNING: Unknown token ... in ...`) y se ignora. `ROOT` se omite sin mensaje. |
| start | Primer fotograma **en su GLA**. Se cuenta desde 0. El juego suma por usted el desplazamiento de su GLA dentro del archivo combinado. |
| length | Número de fotogramas. |
| loop | Fotograma al que se vuelve en bucle, relativo al inicio. `-1` = sin bucle (se reproduce una vez y se queda en el último fotograma, o lo que haga el juego con esa animación). |
| fps | Fotogramas por segundo. Un valor negativo reproduce hacia atrás. |

Reglas:

- **Liste solo lo que cambió.** Una animación que no está en su archivo conserva la de `_humanoid`.
- Una animación puede tener un solo fotograma (una pose, por ejemplo una postura de arma): `length 1`.
- Varios nombres pueden apuntar a los mismos fotogramas (véase `BOTH_STAND2`, `BOTH_STAND2IDLE1`... sobre el mismo fotograma en las sustituciones originales).
- Comentarios con `//`. Tabuladores o espacios.

### Eventos (opcional)

`models/players/_myanims/animevents.cfg` usa exactamente la sintaxis del `animevents.cfg` original (bloques `UPPEREVENTS` / `LOWEREVENTS`: pasos, sonidos, efectos, golpes de sable). El fotograma de un evento es el fotograma **dentro de la animación** (0 = primer fotograma), no un fotograma del GLA. Los eventos de la misma animación y el mismo fotograma pisan los del conjunto base.

## 4. Usarla en un modelo: `animoverride.cfg`

En la carpeta del modelo (la que contiene `model.glm`), cree `animoverride.cfg`:

```
overridename _myanims
```

Una línea, una clave. El valor es el **nombre exacto de la carpeta** de su sustitución (`_myanims`, con su guion bajo si lo tiene). Condiciones:

- El modelo debe usar el **esqueleto `_humanoid`** (su `.glm` apunta a `models/players/_humanoid/_humanoid`). Un modelo con otro esqueleto ignora el archivo.
- Debe existir `models/players/_myanims/_myanims.gla`. Si no, el renderizador muestra `models/players/<model>/animoverride.cfg: models/players/_myanims/_myanims.gla is missing` y el modelo usa las animaciones originales.
- Si falta la clave `overridename`, se muestra `no "overridename" key`.

Todos los personajes cuya variante `.cha` tenga `model <model>` la usan desde ese momento, también en la vista previa del menú de personajes.

Para dar el mismo estilo a dos modelos, haga que los dos `animoverride.cfg` apunten a la misma carpeta.

## 5. Usarla en un NPC o en una entidad

### En un `.npc`

```
elween
{
	playerModel		elween
	animOverride	_myanims
	...
}
```

`animOverride` va junto a los demás campos del NPC (véase la guía de personajes, sección 1.4). Si la sustitución no se puede aplicar, la consola muestra `NPC elween: animOverride _myanims is not used`.

### Con la consola

| Comando | Efecto |
|---|---|
| `animoverride` | Muestra el conjunto de animaciones del jugador y el modo de uso. |
| `animoverride <name>` | Fuerza la sustitución `<name>` en el jugador. `<name>` es la carpeta: `_myanims` o `myanims`, ambas funcionan. |
| `animoverride <name> <targetname>` | Lo mismo, en el NPC cuyo `targetname` es `<targetname>`. |
| `animoverride default` | Fuerza el `_humanoid.gla` original, incluso si el modelo tiene un `animoverride.cfg`. |
| `animoverride none` | Quita la sustitución forzada. El modelo vuelve a usar su propio `animoverride.cfg`, si lo tiene. |

Las animaciones en curso se reinician al instante con los nuevos fotogramas. El ajuste dura hasta que cambia el mapa. **Se conserva en las partidas guardadas** (el conjunto de animaciones de la entidad se guarda y se restaura al cargar).

## 6. Armas: `_weapons.gla`

`models/weapons2/_animations/_weapons.gla` (con su `_weapons.cfg` y su `animevents.cfg`) contiene las poses de las armas y se añade a **todos** los `_humanoid*.gla`, y por tanto también a todas las sustituciones. No hace falta copiar animaciones de armas en su GLA. Si quiere que un personaje sostenga un arma de otra manera, liste el mismo nombre de animación en su `.cfg` (por ejemplo `TORSO_PLX_AIM`) y su versión reemplaza a la original solo para ese personaje.

## 7. Empaquetar y probar

1. Ponga los archivos en un `Z_<name>.pk3` (las rutas parten de la raíz del juego: `models/players/_myanims/...`, `models/players/<model>/animoverride.cfg`) dentro de `GameData\SWGL\`. Se carga después de los `SWGL_*.pk3` y puede reemplazar una sustitución original (misma ruta).
2. Prueba rápida sin tocar ningún modelo: inicie un mapa y escriba `animoverride _myanims` en la consola. Si no cambia nada, lea la consola. Un personaje que se queda en pose en T o se congela indica un `.cfg` incorrecto (revise los nombres y los números de fotograma).
3. Active `developer 1` para ver las líneas de fusión: `GLA merge: animation override _myanims: frames 21800 to 22100 come from models/players/_myanims/_myanims.gla`.
4. Cuando funcione, añada `animoverride.cfg` al modelo, o `animOverride` al `.npc`.

## 8. Lista de comprobación y errores frecuentes

| Síntoma | Causa |
|---|---|
| `animoverride: <name> is not used, see the console` | Falta el GLA, su esqueleto difiere de `_humanoid`, o el total supera los 65 535 fotogramas. |
| No cambia nada con `animoverride.cfg` | El modelo no usa el esqueleto `_humanoid`, el nombre de carpeta en `overridename` está mal escrito, o falta la clave. |
| Algunas animaciones son las originales | Es lo esperado: solo se reemplazan los nombres listados en su `.cfg`. |
| `WARNING: Unknown token X in ...cfg` | El nombre de la animación no está en la tabla de animaciones del juego (ortografía). |
| La animación reproduce fotogramas equivocados | `start` se cuenta en su propio GLA desde 0, no en el archivo combinado. No sume ningún desplazamiento usted mismo. |
| Un evento suena en el momento equivocado | En `animevents.cfg` el fotograma es relativo al inicio de la animación. |
| La sustitución se pierde al cambiar de mapa (consola) | Es lo esperado: use `animoverride.cfg` o `animOverride` para algo permanente. |
| Ragdoll o animación de muerte extraños | El nombre interno del archivo de animación del GLA no es `models/players/_humanoid/_humanoid`. |
| Mismo nombre que una sustitución original | Los nombres deben ser únicos entre los paquetes: gana el último pk3 cargado. |

Referencias de código: `rd-common/mdx_merge.h` (reglas de fusión y constantes), `rd-common/tr_glamerge.cpp` (GLA virtual, lectura de `animoverride.cfg`), `game/NPC_stats.cpp` (`G_ParseMergedAnimations`, `G_SetAnimOverride`, `G_RestoreAnimOverrides`, clave `animOverride`), `game/g_cmds.cpp` (`Cmd_AnimOverride_f`).

Sustituciones originales para copiar, en `SWGL_Animations.pk3`: `_malak` (ejemplo de un solo archivo, sobre todo poses), `_kotor`, `_clones`, `_dvader`, `_jedi`, `_jedi2`, `_lanakin`, `_oldben`, `_palpatine`, `_shaak`, `_sidious`, `_vader`, `_zombie` y otras. Cada una tiene su GLA, su `.cfg` y el `animoverride.cfg` correspondiente en la carpeta del modelo.
