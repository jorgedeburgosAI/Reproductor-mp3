// ============================================================
//  Carcasa del reproductor FLAC — ESP32-S3 + OLED 1.3" + microSD
//  Medidas exteriores: 62 x 96 x 32 mm (cabe en una cama de 100x100)
//
//  Piezas (cambia "part" y exporta cada una a STL con F6 -> F7):
//    "shell"    -> frontal + paredes. Imprimir con la cara frontal sobre la cama.
//    "lid"      -> tapa trasera. Imprimir plana.
//    "buttons"  -> 5 pulsadores. Imprimir de pie, con la pestaña sobre la cama.
//    "all"      -> vista previa de todo
//
//  IMPORTANTE: los módulos baratos cambian de medidas según el vendedor.
//  Mide los tuyos con un calibre y ajusta los parámetros marcados con [MIDE].
//  Imprime primero solo la cara frontal (pon D = 5) para comprobar la
//  pantalla y los botones antes de gastar filamento en la caja completa.
// ============================================================

part = "all";   // "shell" | "lid" | "buttons" | "all"
$fn = 48;

// ---------- Caja ----------
W = 62;          // ancho
H = 96;          // alto
D = 32;          // grosor total (carcasa + tapa)
r = 5;           // radio de las esquinas
wall = 2;        // grosor de pared
front_t = 2.5;   // grosor del frontal
lid_t = 2;       // grosor de la tapa
shell_h = D - lid_t;

// ---------- Pantalla OLED 1.3" SH1106 ----------
oled_cy     = H - 24;           // centro vertical de la pantalla
oled_win    = [30, 15.5];       // ventana visible (zona activa ~29.4 x 14.7)       [MIDE]
oled_win_dy = 0;                // desplazamiento de la zona activa respecto al cristal [MIDE]
oled_glass  = [35.6, 23.5, 1.4];// cristal: ancho, alto, profundidad del rebaje       [MIDE]

// ---------- Botones ----------
// Pulsadores táctiles de 6x6 mm soldados a una placa perforada de 50x40,
// atornillada detrás del frontal. Un pulsador impreso transmite la pulsación.
btn_hole_d  = 8;     // agujero en el frontal
plunger_d   = 7.4;   // vástago del pulsador impreso
flange_d    = 10.5;  // pestaña que impide que se salga
btn_protrude= 1.5;   // lo que sobresale del frontal
tact_h      = 5.0;   // altura del pulsador 6x6 (de la placa a la punta)  [MIDE]
sb_h        = 7.0;   // altura de los separadores de la placa de botones
//              PREV          PLAY        NEXT          VOL-          VOL+
btn_pos = [[W/2-18, 38], [W/2, 38], [W/2+18, 38], [W/2-11, 18], [W/2+11, 18]];
board        = [50, 40];   // placa perforada de botones
board_cy     = 29;
board_inset  = 2.5;        // distancia de los agujeros al borde de la placa
screw_hole   = 1.8;        // para tornillo M2 autorroscante

// ---------- Puertos (z = distancia desde la cara frontal) ----------
jack_d = 6.2;  jack_x = W/2 - 14;  jack_z = 18;   // jack panel PJ-392 (rosca M6), pared superior
usb    = [9.8, 4.0];  usb_x = W/2 + 13;  usb_z = 17;   // USB-C del TP4056, pared inferior  [MIDE]
sd     = [14, 2.8];   sd_x  = W/2 - 13;  sd_z  = 17;   // ranura microSD, pared inferior     [MIDE]
sw     = [9.5, 4.4];  sw_y  = H - 40;    sw_z  = 18;   // interruptor SS12D00, pared izquierda

// ============================================================

module rbox(w, h, t, rr) {
  translate([rr, rr, 0]) linear_extrude(t) offset(r = rr) square([w - 2*rr, h - 2*rr]);
}

module corners() {
  for (x = [r, W - r], y = [r, H - r]) translate([x, y, 0]) children();
}

module board_holes() {
  for (sx = [-1, 1], sy = [-1, 1])
    translate([W/2 + sx*(board[0]/2 - board_inset), board_cy + sy*(board[1]/2 - board_inset), 0])
      children();
}

module shell() {
  difference() {
    union() {
      difference() {
        rbox(W, H, shell_h, r);
        translate([wall, wall, front_t]) rbox(W - 2*wall, H - 2*wall, shell_h, r - wall);
      }
      // columnas de las esquinas para los tornillos de la tapa
      corners() cylinder(d = 2*(r - wall) + 0.2, h = shell_h);
      // separadores de la placa de botones
      board_holes() cylinder(d = 5, h = front_t + sb_h);
    }
    // agujeros de tornillo
    corners() translate([0, 0, shell_h - 10]) cylinder(d = screw_hole, h = 11);
    board_holes() translate([0, 0, front_t + 1]) cylinder(d = screw_hole, h = sb_h);

    // ventana de la pantalla y rebaje del cristal (desde dentro)
    translate([W/2 - oled_win[0]/2, oled_cy - oled_win[1]/2 + oled_win_dy, -1])
      cube([oled_win[0], oled_win[1], front_t + 2]);
    translate([W/2 - oled_glass[0]/2, oled_cy - oled_glass[1]/2, front_t - oled_glass[2]])
      cube([oled_glass[0], oled_glass[1], oled_glass[2] + 1]);

    // botones
    for (p = btn_pos) translate([p[0], p[1], -1]) cylinder(d = btn_hole_d, h = front_t + 2);

    // jack 3.5 mm (pared superior)
    translate([jack_x, H - wall - 1, jack_z]) rotate([-90, 0, 0]) cylinder(d = jack_d, h = wall + 2);

    // USB-C y microSD (pared inferior)
    translate([usb_x, -1, usb_z]) slot(usb, wall + 2);
    translate([sd_x,  -1, sd_z])  slot(sd,  wall + 2);
    // hueco para sacar la tarjeta con la uña
    translate([sd_x, -0.01, sd_z]) scale([1, 1, 0.6]) rotate([-90, 0, 0]) cylinder(d = 10, h = 0.8);

    // interruptor de encendido (pared izquierda)
    translate([-1, sw_y - sw[0]/2, sw_z - sw[1]/2]) cube([wall + 2, sw[0], sw[1]]);
  }
}

// ranura rectangular con extremos redondeados, centrada en x y z
module slot(size, depth) {
  rr = size[1] / 2;
  hull() for (sx = [-1, 1])
    translate([sx*(size[0]/2 - rr), 0, 0]) rotate([-90, 0, 0]) cylinder(r = rr, h = depth);
}

module lid() {
  difference() {
    rbox(W, H, lid_t, r);
    corners() {
      translate([0, 0, -1]) cylinder(d = 2.4, h = lid_t + 2);
      translate([0, 0, -1]) cylinder(d = 4.4, h = 1 + 1.0);   // avellanado para cabeza M2
    }
  }
}

module plunger() {
  inner = sb_h - tact_h;   // de la cara interior del frontal a la punta del pulsador
  cylinder(d = flange_d, h = inner);
  cylinder(d = plunger_d, h = inner + front_t + btn_protrude);
}

module buttons() {
  for (i = [0:4]) translate([i * (flange_d + 3), 0, 0]) plunger();
}

if (part == "shell") shell();
else if (part == "lid") lid();
else if (part == "buttons") buttons();
else {
  shell();
  translate([W + 10, 0, 0]) lid();
  translate([0, -20, 0]) buttons();
}
