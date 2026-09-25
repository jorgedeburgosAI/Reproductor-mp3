# Reproductor FLAC portátil — guía de montaje

ESP32-S3 + DAC PCM5102A + amplificador de auriculares + OLED 1.3" + microSD.
Carcasa de 62 × 96 × 32 mm, impresa en 3D.

Contenido del paquete:
- `reproductor/reproductor.ino`: el firmware.
- La carcasa sera diseñada por mi e impresa en 3D (se adjuntaran planos en un futuro)
- Esta guía.

---

## 1. Lista de componentes (~45–55 €)

Los precios son orientativos, tomados de AliExpress y Amazon España.

| Componente | Detalle | Precio aprox. |
|---|---|---|
| ESP32-S3 DevKitC **N16R8** | 16 MB de flash y 8 MB de PSRAM octal. Sin PSRAM no decodifica FLAC | 8–10 € |
| DAC PCM5102A | Módulo I2S morado (GY-PCM5102) | 4 € |
| Amplificador de auriculares | Módulo con TPA6132A2, PAM8908 o similar, a 3,3 V | 3–5 € |
| OLED 1,3" SH1106 I2C | 128×64, 4 pines (VCC, GND, SCL, SDA) | 4 € |
| Lector microSD SPI | Módulo pequeño **sin** regulador de 5 V (o con puente a 3,3 V) | 1–2 € |
| Jack hembra 3,5 mm de panel | PJ-392 o similar con rosca M6 | 1 € |
| Batería LiPo 3,7 V | 1500–2000 mAh, máx. ~50 × 34 × 10 mm (p. ej. 103450), **con protección** | 7–9 € |
| Cargador TP4056 USB-C | Versión **con protección** (6 pines: B+, B−, OUT+, OUT−) | 1–2 € |
| Regulador buck-boost a 3,3 V | TPS63020, TPS63802 o similar. Da 3,3 V estables con la batería entre 4,2 y 3,0 V | 2–4 € |
| Interruptor deslizante | SS12D00 | 0,5 € |
| 5 pulsadores táctiles 6×6 mm | Altura total de 5 mm | 1 € |
| 2 resistencias de 100 kΩ | Divisor para medir la batería | 0,1 € |
| Placa perforada, cable fino, tornillos M2 (4× 8 mm y 4× 6 mm) | | 4–5 € |
| Filamento PETG o PLA | ~60 g | 1,5 € |

**Por qué el buck-boost:** el regulador de la placa ESP32 necesita unos 4,3 V o más para dar 3,3 V limpios. Con una LiPo, que va de 4,2 a 3,0 V, se quedaría corto a media carga y oirías cortes o reinicios.

---

## 2. Pines del ESP32-S3

| Función | GPIO | Conecta a |
|---|---|---|
| I2S BCLK | 4 | PCM5102A **BCK** |
| I2S LRCK | 5 | PCM5102A **LCK** |
| I2S DATA | 6 | PCM5102A **DIN** |
| OLED SDA | 8 | OLED SDA |
| OLED SCL | 9 | OLED SCL |
| SD CS | 10 | microSD CS |
| SD MOSI | 11 | microSD MOSI |
| SD SCK | 12 | microSD SCK |
| SD MISO | 13 | microSD MISO |
| Botón VOL+ | 15 | pulsador → GND |
| Botón VOL− | 16 | pulsador → GND |
| Botón PREV | 17 | pulsador → GND |
| Botón PLAY | 18 | pulsador → GND |
| Botón NEXT | 21 | pulsador → GND |
| Batería | 1 | punto medio del divisor 100k/100k |

Los botones no necesitan resistencias, porque se usa el pull-up interno. No uses los GPIO 0, 3, 19, 20, 26–37, 45 ni 46: son de arranque, del USB o de la flash y PSRAM.

---

## 3. Conexiones

### Alimentación
```
LiPo (+/−) ──► TP4056  B+ / B−
TP4056 OUT+ ──► interruptor ──► buck-boost VIN ──► 3V3 del ESP32, OLED, microSD, DAC (VIN) y amplificador
TP4056 OUT− ──────────────────► GND común
Tras el interruptor: OUT+ ── 100k ──┬── 100k ── GND
                                    └── GPIO 1
```
- Alimenta el ESP32 **por su pin 3V3**, no por el de 5V.
- **Para programar por USB, pon el interruptor en OFF.** Si no, el regulador de la placa y el buck-boost alimentan la misma línea a la vez.
- El TP4056 carga igual con el interruptor apagado.

### DAC PCM5102A
Hay que fijar sus pines de configuración. Muchos módulos lo hacen con puentes de soldadura en la parte de atrás (H1L…H4L).

| Pin | Conectar a | Significado |
|---|---|---|
| SCK | GND | usa el PLL interno (no necesita reloj maestro) |
| FLT | GND | filtro normal |
| DEMP | GND | sin de-énfasis |
| XSMT | 3,3 V | sin silencio |
| FMT | GND | formato I2S |

En los puentes traseros eso equivale a: **1 → L, 2 → L, 3 → H, 4 → L**.

### Audio
```
PCM5102A  L / R / GND  ──►  entradas del amplificador (IN L / IN R / GND)
Amplificador  OUT L / OUT R / GND  ──►  jack: punta = L, anillo = R, manguito = GND
```
El PCM5102A no está pensado para mover auriculares directamente, por eso lleva el amplificador. No uses el jack que trae soldado el módulo DAC; saca la señal de sus pines.

---

## 4. Distribución dentro de la caja

Por capas, desde el frontal hacia la tapa:

1. **Frontal:** la OLED encajada en su rebaje (fíjala con una gota de cola caliente) y la placa de botones atornillada a los 4 separadores, con los pulsadores mirando al frontal.
2. **Capa media (unos 13–20 mm desde el frente):**
   - ESP32 en vertical, sin pines soldados; suelda los cables directamente para ganar altura.
   - DAC y amplificador al lado del ESP32.
   - TP4056 y lector microSD pegados a la pared inferior, alineados con sus ranuras.
   - Buck-boost donde quepa.
3. **Contra la tapa:** la batería, sujeta con cinta de doble cara.

Si el USB-C, la ranura de la SD o el interruptor no coinciden con los huecos, ajusta `usb_z`, `sd_z`, `sw_z` y compañía en `carcasa.scad`.

---

## 5. Impresión

- En `carcasa.scad` cambia `part` a `"shell"`, `"lid"` y `"buttons"` y exporta cada pieza a STL (F6 y luego F7).
- **Primero, una prueba:** con `D = 5` imprimes solo el frontal en unos 20 minutos. Comprueba que encajan la pantalla, los botones y los separadores.
- Carcasa: frontal sobre la cama, capa de 0,2 mm, 3 perímetros, relleno del 20 %, sin soportes (los huecos laterales son pequeños y puentean bien).
- Pulsadores: de pie, con la pestaña sobre la cama y capa de 0,12 mm.
- Usa PETG si va a estar al sol o en el coche. Si no, PLA sirve.

---

## 6. Programar el ESP32

1. En **Arduino IDE 2**, abre Preferencias → URLs adicionales y añade `https://espressif.github.io/arduino-esp32/package_esp32_index.json`. Luego instala **esp32 de Espressif** (3.x) desde el Gestor de placas.
2. Instala las librerías:
   - **U8g2**, desde el Gestor de bibliotecas.
   - **ESP32-audioI2S**, descargando el ZIP de `github.com/schreibfaul1/ESP32-audioI2S` e instalándolo con *Programa → Incluir biblioteca → Añadir .ZIP*.
3. En *Herramientas*, elige:
   - Placa: **ESP32S3 Dev Module**
   - Flash Size: **16MB**
   - PSRAM: **OPI PSRAM**
   - USB CDC On Boot: **Enabled**
4. Abre `reproductor/reproductor.ino` y súbelo con el interruptor en OFF.

Si la pantalla sale desplazada o con basura, tu módulo lleva un controlador SSD1306. Cambia el constructor por `U8G2_SSD1306_128X64_NONAME_F_HW_I2C`.

---

## 7. Preparar la microSD

- Formátela en **FAT32**. Windows no lo permite desde el menú en tarjetas de más de 32 GB; para esas usa *FAT32 Format* (guiformat).
- Organiza la música por carpetas, que el reproductor muestra como álbumes:
  ```
  /Artista - Álbum/01 - Canción.flac
  ```
  El número inicial se usa para ordenar las pistas y se oculta en pantalla.
- **La música de Spotify no sirve:** está cifrada con DRM y no se puede copiar. Necesitas FLAC propios: de tus CD (ripeados con EAC o fre:ac) o comprados en Bandcamp o Qobuz.

### Sobre la calidad
- La librería de audio trabaja internamente a **16 bits**. Un FLAC de 24/44,1 se reproduce a calidad CD (16/44,1). La diferencia no se distingue en una escucha a ciegas, pero conviene que lo sepas.
- Prueba un FLAC de 24/44,1 antes de cerrar la caja. Si alguno da problemas, convierte tu biblioteca a 16/44,1 con fre:ac y se acaba el problema.

---

## 8. Uso

| Pantalla | Botón | Acción |
|---|---|---|
| Reproducción | PLAY | pausa / reanudar |
| | PLAY (mantener) | abrir el navegador |
| | NEXT / PREV | pista siguiente / anterior (PREV reinicia la pista si llevas más de 3 s) |
| | VOL+ / VOL− | volumen (mantén pulsado para cambiarlo rápido) |
| Navegador | PREV / NEXT | subir / bajar (mantén pulsado para desplazarte rápido) |
| | PLAY | entrar en la carpeta / reproducir |
| | PLAY (mantener) | volver a la reproducción |
| | VOL− | atrás |

Al terminar una canción pasa a la siguiente de la misma carpeta y se detiene al llegar al final.

Autonomía estimada: **10–15 h** con 2000 mAh, según el volumen y los auriculares.
