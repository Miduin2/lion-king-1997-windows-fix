# Informe técnico — The Lion King (Windows, 1997)

## Resumen sencillo

El juego original no estaba “roto” en un único punto. Dependía de varias
suposiciones válidas en Windows 95: color indexado de 8 bits, llamadas BIOS,
instrucciones privilegiadas toleradas, punteros de 32 bits tratados como
enteros de 16 bits y rutas de instalación muy cortas.

La solución conserva el ejecutable y los recursos originales. Solo aplica
cambios mínimos y comprobables al EXE, añade una capa local `ddraw.dll` y usa
un lanzador nativo. No recompila ni sustituye el juego.

## Fallos y reparación

| Síntoma | Causa | Reparación |
|---|---|---|
| “Lionking must run in at least 256 colours” | Detección Win95 incorrecta en un escritorio moderno | Se cambia el salto de `0x2EA7` de `7D` a `EB` |
| “Can't open EPFS file” | El motor considera erróneo el descriptor válido cero | Se cambia `0x5A3F` de `7F` a `7D` |
| Excepción al terminar los logos | Llamada BIOS `INT 1Ah` no válida en Win32 moderno | En `0xC165` se usa `GetTickCount` importado por el propio EXE |
| “A privileged instruction was executed” | Cuatro instrucciones `CLI`/`STI` | Se sustituyen por `NOP` en `0x3771`, `0x377F`, `0x3D5B` y `0x3D75` |
| Imagen negra, color incorrecto o cambio de modo inestable | DirectDraw de 8 bits sobre compositor moderno | GameVaultDraw mantiene la paleta lógica y presenta en 32 bits |
| Juego diminuto o demasiado rápido | Tamaño VGA y bucle ligado al refresco | Presentación 4:3 sin bordes y límite de 60 FPS |
| Corrupción o caída al reproducir MIDI | El comando MCI cabe en un búfer muy corto | El lanzador crea temporalmente una ruta como `V:\` |
| Joystick/Keyboard cierran el juego | Los callbacks truncan un puntero de `EM_GETSEL` a 16 bits | Hook limitado a los doce callbacks conocidos y reconstrucción validada del puntero |
| Menú oculto en pantalla sin bordes | La barra Win32 original queda fuera de la presentación | `F2` envía el comando Properties nativo |
| `Esc` no ofrece salida | Ruta de cierre original perdida | Diálogo modal seguro, en inglés, con “No” por defecto |
| Cursor azul de espera permanente | El juego conserva el cursor ocupado | Se oculta solo sobre la ventana principal habilitada y reaparece en diálogos |
| Vista extraña en Alt+Tab | DWM no puede capturar la superficie primaria antigua | Miniatura y vista previa generadas desde el último fotograma |

## Límites de seguridad

- El instalador exige el SHA-256 exacto del EXE soportado.
- Comprueba todos los bytes originales antes de cambiar uno.
- Genera el archivo nuevo aparte y verifica su hash final.
- Conserva una copia original identificada por hash.
- La restauración se niega a sobrescribir un EXE desconocido.
- `ddraw.dll` solo se carga al colocarlo junto al juego; no se instala en
  Windows ni se inyecta en otros procesos.
- El repositorio y la descarga no incluyen archivos propietarios.

## Resultado

El resultado sigue siendo la conversión Windows original, pero se comporta
como una aplicación moderna razonable: pantalla completa 4:3 estable, velocidad
normal, audio, pausa y salida, panel de opciones, taskbar, Alt+Tab y cursor
correcto. Las asignaciones de controles funcionan durante la sesión, aunque el
propio juego restaura sus valores predeterminados al volver a arrancar.
