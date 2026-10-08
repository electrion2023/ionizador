// ============================================================
//  ESP32S3_TFT_HMI.ino  —  v3.1  (refresco incremental)
//  HMI para Waveshare ESP32-S3-Touch-LCD-7 (800x480)
//  Solo muestra estado y envía comandos cortos por UART.
//  NO redibuja toda la pantalla; solo las zonas que cambian.
// ============================================================
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include "logo.h"

// ---------- UART hacia la máquina ----------
HardwareSerial maquina(1);
#define HMI_TX 43
#define HMI_RX 44
#define HMI_BAUD 9600

// ---------- Display ----------
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  5, 3, 46, 7, 1, 2, 42, 41, 40, 39, 0, 45, 48, 47, 21, 14, 38, 18, 17, 10,
  0, 40, 48, 88, 0, 13, 3, 32, 1, 16000000
);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(800, 480, rgbpanel, 0, true);
TAMC_GT911 ts = TAMC_GT911(8, 9, 4, -1, 800, 480);

// ---------- Pantallas ----------
enum Pantalla { P_INTRO, P_PRINCIPAL, P_CONFIG, P_MANUAL };
Pantalla pantalla = P_INTRO;
uint32_t tInicioIntro = 0;
const uint32_t TIEMPO_INTRO = 3000;

// ---------- Estado reflejado ----------
uint8_t  estadoMaquina = 0;
bool     maquinaActiva = false;
bool     ir1 = false, ir2 = false;
bool     rele[6] = {false,false,false,false,false,false};
uint8_t  cntEntrada = 0, cntSalida = 0;
uint32_t cntCiclos = 0;
uint32_t tiempoRestante = 0;

// Parámetros locales
uint16_t pLlenado = 25;
uint16_t pDescomp = 10;
uint16_t pRetiro  = 1500;

// ---------- Estado ANTERIOR (para detectar cambios) ----------
uint8_t  estadoMaquinaAnt = 255;   // fuerza primer dibujo
bool     maquinaActivaAnt = false;
bool     ir1Ant = false, ir2Ant = false;
bool     releAnt[6] = {false,false,false,false,false,false};
uint8_t  cntEntradaAnt = 255;
uint8_t  cntSalidaAnt  = 255;
uint32_t cntCiclosAnt  = 0xFFFFFFFF;
uint32_t tiempoRestanteAnt = 0xFFFFFFFF;

// ---------- UART ----------
String bufUart = "";
uint32_t tUltChar = 0;
const uint32_t TIMEOUT_UART = 100;

// ---------- Touch ----------
uint32_t tUltToque = 0;
const uint32_t DEBOUNCE_TOUCH = 250;

// ---------- Nombres etapas ----------
const char* nombreEtapa(uint8_t e) {
  static const char* n[] = {
    "DETENIDO","INGRESO","ALINEACION","POSICIONANDO",
    "LLENADO","DESCOMPRESION","RETIRO PICOS",
    "SALIDA","ESPERA RETIRO","CIERRE CICLO"
  };
  if (e > 9) e = 0;
  return n[e];
}

// ============================================================
//  Intro
// ============================================================
void mostrarIntro() {
  uint16_t cols[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
  for (int i = 0; i < 5; i++) { gfx->fillScreen(cols[i]); delay(60); }
  gfx->fillScreen(0x0000);
  int lx = (800 - LOGO_WIDTH) / 2;
  int ly = (480 - LOGO_HEIGHT) / 2;
  gfx->draw16bitRGBBitmap(lx, ly, logo, LOGO_WIDTH, LOGO_HEIGHT);
  gfx->setTextSize(2); gfx->setTextColor(0x07E0);
  gfx->setCursor(250, 370); gfx->println("CONTROL LLENADORA");
  gfx->setTextSize(1); gfx->setTextColor(0x8410);
  gfx->setCursor(300, 400); gfx->println("Iniciando sistema...");
}

// ============================================================
//  Utilidades gráficas
// ============================================================
void box(int x, int y, int w, int h, uint16_t fill, uint16_t border) {
  gfx->fillRoundRect(x, y, w, h, 10, fill);
  gfx->drawRoundRect(x, y, w, h, 10, border);
}
void txt(int x, int y, uint8_t size, uint16_t color, const char *s) {
  gfx->setTextSize(size); gfx->setTextColor(color);
  gfx->setCursor(x, y); gfx->print(s);
}

// Borra un rectángulo y escribe texto centrado (para actualizaciones parciales)
void clearAndPrint(int x, int y, int w, int h, uint16_t bg,
                   uint8_t size, uint16_t color, const char *s) {
  gfx->fillRect(x, y, w, h, bg);
  gfx->setTextSize(size); gfx->setTextColor(color);
  gfx->setCursor(x + 4, y + 4); gfx->print(s);
}

// ============================================================
//  DIBUJAR PANTALLA PRINCIPAL COMPLETA (solo al entrar)
// ============================================================
void dibujarPrincipal() {
  gfx->fillScreen(0x0000);

  // Header
  gfx->fillRect(0, 0, 800, 55, 0x1082);
  txt(20, 15, 3, 0x07E0, "CONTROL LLENADORA");
  gfx->drawLine(20, 50, 780, 50, CYAN);

  // Tabs
  box(560, 5, 70, 40, 0x0000, 0x8410); txt(572, 15, 1, 0xFFFF, "CONFIG");
  box(640, 5, 70, 40, 0x0000, 0x8410); txt(656, 15, 1, 0xFFFF, "MANUAL");
  box(720, 5, 70, 40, 0x0000, 0x8410); txt(736, 15, 1, 0xFFFF, "SYNC");

  // Cajas estáticas (bordes y labels)
  box(20, 70, 360, 70, 0x1082, WHITE);
  txt(35, 82, 1, 0xFFFF, "ESTADO:");

  box(400, 70, 380, 70, 0x1082, WHITE);
  txt(415, 82, 1, 0xFFFF, "ETAPA:");

  box(20, 150, 360, 90, 0x1082, WHITE);
  txt(35, 162, 1, 0xFFFF, "ENTRADA");
  txt(200, 162, 1, 0xFFFF, "SALIDA");
  txt(35, 220, 1, 0xFFFF, "CICLOS:");

  box(400, 150, 180, 90, 0x1082, WHITE);
  txt(415, 162, 1, 0xFFFF, "IR1");
  txt(415, 210, 1, 0xFFFF, "IR2");

  box(600, 150, 180, 90, 0x1082, WHITE);
  txt(615, 162, 1, 0xFFFF, "ACTUADORES");

  // Botones INICIAR / STOP (estáticos, color se actualiza)
  box(20, 260, 180, 70, 0x05A0, WHITE);
  txt(60, 285, 3, 0x0000, "INICIAR");
  box(220, 260, 160, 70, 0x8000, WHITE);
  txt(260, 285, 3, 0x0000, "STOP");

  // Parámetros rápidos (caja estática)
  box(20, 345, 760, 50, 0x1082, WHITE);
  txt(35, 360, 1, 0xFFFF, "LLENADO:");
  txt(230, 360, 1, 0xFFFF, "DESCOMP:");
  txt(430, 360, 1, 0xFFFF, "RETIRO:");

  // Footer
  gfx->fillRect(0, 410, 800, 70, 0x1082);
  txt(20, 430, 1, 0x8410, "v3.1 | GUERRA MAQUINAS & AUTOMATIZACION");

  // Forzar actualización de todas las zonas dinámicas
  estadoMaquinaAnt = 255;
  maquinaActivaAnt = !maquinaActiva;
  ir1Ant = !ir1; ir2Ant = !ir2;
  for (uint8_t i = 0; i < 6; i++) releAnt[i] = !rele[i];
  cntEntradaAnt = 255; cntSalidaAnt = 255;
  cntCiclosAnt = 0xFFFFFFFF;
  tiempoRestanteAnt = 0xFFFFFFFF;
}

// ============================================================
//  ACTUALIZACIONES PARCIALES (solo la zona que cambió)
// ============================================================

// --- Estado máquina ---
void actualizarEstado() {
  if (maquinaActiva == maquinaActivaAnt && estadoMaquina == estadoMaquinaAnt) return;
  maquinaActivaAnt = maquinaActiva;
  estadoMaquinaAnt = estadoMaquina;

  // Texto estado
  clearAndPrint(35, 100, 330, 35, 0x1082, 3,
                maquinaActiva ? 0x07E0 : 0xF800,
                maquinaActiva ? "AUTOMATICO" : "DETENIDA   ");

  // Color botones INICIAR/STOP
  box(20, 260, 180, 70, maquinaActiva ? 0x05A0 : 0x07E0, WHITE);
  txt(60, 285, 3, 0x0000, "INICIAR");
  box(220, 260, 160, 70, maquinaActiva ? 0x8000 : 0xF800, WHITE);
  txt(260, 285, 3, 0x0000, "STOP");
}

// --- Etapa + tiempo restante ---
void actualizarEtapa() {
  if (estadoMaquina == estadoMaquinaAnt && tiempoRestante == tiempoRestanteAnt) return;

  // Solo redibujar si la etapa cambió
  if (estadoMaquina != estadoMaquinaAnt) {
    estadoMaquinaAnt = estadoMaquina;
    clearAndPrint(415, 100, 240, 30, 0x1082, 2, 0x07E0, nombreEtapa(estadoMaquina));
  }

  // Tiempo restante (siempre actualizar si cambió)
  if (tiempoRestante != tiempoRestanteAnt) {
    tiempoRestanteAnt = tiempoRestante;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lums   ", tiempoRestante);
    clearAndPrint(660, 100, 110, 30, 0x1082, 2, 0xFFE0, buf);
  }
}

// --- Contadores ---
void actualizarContadores() {
  if (cntEntrada == cntEntradaAnt && cntSalida == cntSalidaAnt && cntCiclos == cntCiclosAnt) return;

  if (cntEntrada != cntEntradaAnt) {
    cntEntradaAnt = cntEntrada;
    char buf[8]; snprintf(buf, sizeof(buf), "%d/4  ", cntEntrada);
    clearAndPrint(35, 182, 100, 30, 0x1082, 3, 0x07E0, buf);
  }
  if (cntSalida != cntSalidaAnt) {
    cntSalidaAnt = cntSalida;
    char buf[8]; snprintf(buf, sizeof(buf), "%d/4  ", cntSalida);
    clearAndPrint(200, 182, 100, 30, 0x1082, 3, 0x07E0, buf);
  }
  if (cntCiclos != cntCiclosAnt) {
    cntCiclosAnt = cntCiclos;
    char buf[12]; snprintf(buf, sizeof(buf), "%lu    ", cntCiclos);
    clearAndPrint(110, 218, 120, 25, 0x1082, 2, 0xFFE0, buf);
  }
}

// --- Sensores ---
void actualizarSensores() {
  if (ir1 == ir1Ant && ir2 == ir2Ant) return;

  if (ir1 != ir1Ant) {
    ir1Ant = ir1;
    gfx->fillRoundRect(415, 180, 150, 22, 4, ir1 ? 0x07E0 : 0x8000);
    txt(425, 185, 1, ir1 ? 0x0000 : 0xFFFF, ir1 ? "ACTIVO" : "LIBRE ");
  }
  if (ir2 != ir2Ant) {
    ir2Ant = ir2;
    gfx->fillRoundRect(415, 228, 150, 22, 4, ir2 ? 0x07E0 : 0x8000);
    txt(425, 233, 1, ir2 ? 0x0000 : 0xFFFF, ir2 ? "ACTIVO" : "LIBRE ");
  }
}

// --- Actuadores ---
void actualizarActuadores() {
  bool algunCambio = false;
  for (uint8_t i = 0; i < 6; i++) {
    if (rele[i] != releAnt[i]) { algunCambio = true; break; }
  }
  if (!algunCambio) return;

  const char* nombres[6] = {"CINTA", "P1", "P2", "PICOS", "ANTIG", "BOMBA"};
  for (uint8_t i = 0; i < 6; i++) {
    if (rele[i] == releAnt[i]) continue;
    releAnt[i] = rele[i];
    int x = 615 + (i % 3) * 55;
    int y = 185 + (i / 3) * 25;
    gfx->fillRect(x, y, 50, 18, rele[i] ? 0x07E0 : 0x4000);
    gfx->drawRect(x, y, 50, 18, WHITE);
    txt(x + 4, y + 3, 1, rele[i] ? 0x0000 : 0xFFFF, nombres[i]);
  }
}

// --- Parámetros rápidos ---
void actualizarParametrosRapidos() {
  // Solo se actualizan cuando volvemos de CONFIG, así que se dibujan al entrar
  static uint16_t pLAnt = 0xFFFF, pDAnt = 0xFFFF, pRAnt = 0xFFFF;
  if (pLlenado == pLAnt && pDescomp == pDAnt && pRetiro == pRAnt) return;
  pLAnt = pLlenado; pDAnt = pDescomp; pRAnt = pRetiro;

  char buf[16];
  snprintf(buf, sizeof(buf), "%us   ", pLlenado);
  clearAndPrint(110, 358, 100, 25, 0x1082, 2, 0x07E0, buf);
  snprintf(buf, sizeof(buf), "%us   ", pDescomp);
  clearAndPrint(320, 358, 90, 25, 0x1082, 2, 0x07E0, buf);
  snprintf(buf, sizeof(buf), "%ums  ", pRetiro);
  clearAndPrint(510, 358, 120, 25, 0x1082, 2, 0x07E0, buf);
}

// ============================================================
//  Refresco incremental (llamada desde loop)
// ============================================================
void refrescarIncremental() {
  if (pantalla != P_PRINCIPAL) return;
  actualizarEstado();
  actualizarEtapa();
  actualizarContadores();
  actualizarSensores();
  actualizarActuadores();
  actualizarParametrosRapidos();
}

// ============================================================
//  Pantalla CONFIG (dibujo completo, solo al entrar)
// ============================================================
void dibujarConfig() {
  gfx->fillScreen(0x0000);
  gfx->fillRect(0, 0, 800, 55, 0x1082);
  txt(20, 15, 3, 0x07E0, "CONFIGURACION");
  gfx->drawLine(20, 50, 780, 50, CYAN);
  box(720, 5, 70, 40, 0x0000, 0x8410); txt(736, 15, 1, 0xFFFF, "VOLVER");

  // LLENADO
  txt(50, 90, 2, 0xFFFF, "LLENADO (s)");
  box(50, 125, 60, 60, 0x05A0, WHITE); txt(70, 145, 3, 0xFFFF, "-");
  box(130, 125, 160, 60, 0x0000, 0x07E0);
  box(310, 125, 60, 60, 0x05A0, WHITE); txt(330, 145, 3, 0xFFFF, "+");

  // DESCOMPRESION
  txt(430, 90, 2, 0xFFFF, "DESCOMP. (s)");
  box(430, 125, 60, 60, 0x05A0, WHITE); txt(450, 145, 3, 0xFFFF, "-");
  box(510, 125, 160, 60, 0x0000, 0x07E0);
  box(690, 125, 60, 60, 0x05A0, WHITE); txt(710, 145, 3, 0xFFFF, "+");

  // RETIRO
  txt(50, 220, 2, 0xFFFF, "RETIRO (ms)  pasos 100ms");
  box(50, 255, 60, 60, 0x05A0, WHITE); txt(70, 275, 3, 0xFFFF, "-");
  box(130, 255, 220, 60, 0x0000, 0x07E0);
  box(370, 255, 60, 60, 0x05A0, WHITE); txt(390, 275, 3, 0xFFFF, "+");

  // GRABAR
  box(280, 360, 240, 70, 0xF800, WHITE);
  txt(330, 385, 3, 0x0000, "GRABAR");

  actualizarValoresConfig();
}

void actualizarValoresConfig() {
  char buf[8];
  snprintf(buf, sizeof(buf), "%3u", pLlenado);
  clearAndPrint(145, 135, 130, 40, 0x0000, 4, 0x07E0, buf);
  snprintf(buf, sizeof(buf), "%2u", pDescomp);
  clearAndPrint(525, 135, 130, 40, 0x0000, 4, 0x07E0, buf);
  snprintf(buf, sizeof(buf), "%4u", pRetiro);
  clearAndPrint(145, 265, 190, 40, 0x0000, 4, 0x07E0, buf);
}

// ============================================================
//  Pantalla MANUAL (dibujo completo, solo al entrar)
// ============================================================
void dibujarManual() {
  gfx->fillScreen(0x0000);
  gfx->fillRect(0, 0, 800, 55, 0x1082);
  txt(20, 15, 3, 0x07E0, "PRUEBA MANUAL");
  gfx->drawLine(20, 50, 780, 50, CYAN);
  box(720, 5, 70, 40, 0x0000, 0x8410); txt(736, 15, 1, 0xFFFF, "VOLVER");
  txt(50, 80, 1, 0xFFFF, "Solo disponible con maquina DETENIDA");

  const char* nombres[6] = {"R1 CINTA", "R2 PISTON1", "R3 PISTON2",
                            "R4 PICOS", "R5 ANTIGOTEO", "R6 BOMBA"};
  for (uint8_t i = 0; i < 6; i++) {
    int y = 110 + i * 55;
    txt(50, y + 15, 2, 0xFFFF, nombres[i]);
    box(350, y, 120, 45, 0x05A0, WHITE); txt(385, y + 12, 2, 0x0000, "ON");
    box(490, y, 120, 45, 0xF800, WHITE); txt(520, y + 12, 2, 0x0000, "OFF");
  }
  actualizarEstadosManual();
}

void actualizarEstadosManual() {
  for (uint8_t i = 0; i < 6; i++) {
    int y = 110 + i * 55;
    clearAndPrint(640, y + 10, 80, 30, 0x0000, 2,
                  rele[i] ? 0x07E0 : 0xF800,
                  rele[i] ? "ON " : "OFF");
  }
}

// ============================================================
//  Touch
// ============================================================
void detectarToque() {
  ts.read();
  if (!ts.isTouched) return;
  int tx = map(ts.points[0].y, 12, 785, 0, 800);
  int ty = map(ts.points[0].x, 795, 325, 0, 480);
  tx = constrain(tx, 0, 799); ty = constrain(ty, 0, 479);
  if (millis() - tUltToque < DEBOUNCE_TOUCH) return;
  tUltToque = millis();

  auto hit = [&](int x, int y, int w, int h) {
    return tx >= x && tx <= x + w && ty >= y && ty <= y + h;
  };

  if (pantalla == P_PRINCIPAL) {
    if (hit(560, 5, 70, 40))   { pantalla = P_CONFIG; dibujarConfig(); return; }
    if (hit(640, 5, 70, 40))   { pantalla = P_MANUAL; dibujarManual(); return; }
    if (hit(720, 5, 70, 40))   { maquina.println("QS"); return; }
    if (hit(20, 260, 180, 70)) { maquina.println("OK"); return; }
    if (hit(220, 260, 160, 70)){ maquina.println("ST"); return; }
  }
  else if (pantalla == P_CONFIG) {
    if (hit(720, 5, 70, 40)) {
      pantalla = P_PRINCIPAL;
      dibujarPrincipal();
      maquina.println("QS");
      return;
    }
    bool cambio = false;
    if (hit(50, 125, 60, 60))  { if (pLlenado > 0)   { pLlenado--; cambio = true; } }
    if (hit(310, 125, 60, 60)) { if (pLlenado < 999)  { pLlenado++; cambio = true; } }
    if (hit(430, 125, 60, 60)) { if (pDescomp > 0)    { pDescomp--; cambio = true; } }
    if (hit(690, 125, 60, 60)) { if (pDescomp < 99)   { pDescomp++; cambio = true; } }
    if (hit(50, 255, 60, 60))  { if (pRetiro >= 100)  { pRetiro -= 100; cambio = true; } }
    if (hit(370, 255, 60, 60)) { if (pRetiro <= 9899) { pRetiro += 100; cambio = true; } }
    if (cambio) actualizarValoresConfig();

    if (hit(280, 360, 240, 70)) {
      char cmd[16];
      snprintf(cmd, sizeof(cmd), "G%03u%02u%04u", pLlenado, pDescomp, pRetiro);
      maquina.println(cmd);
    }
  }
  else if (pantalla == P_MANUAL) {
    if (hit(720, 5, 70, 40)) {
      pantalla = P_PRINCIPAL;
      dibujarPrincipal();
      maquina.println("QS");
      return;
    }
    for (uint8_t i = 0; i < 6; i++) {
      int y = 110 + i * 55;
      if (hit(350, y, 120, 45)) { char c[5]; snprintf(c, 5, "R%dA", i + 1); maquina.println(c); return; }
      if (hit(490, y, 120, 45)) { char c[5]; snprintf(c, 5, "R%dB", i + 1); maquina.println(c); return; }
    }
  }
}

// ============================================================
//  Procesar mensajes de la máquina
// ============================================================
void procesarRespuesta(const String &r) {
  if (r.length() == 0) return;

  if (r == "MA") { maquinaActiva = true;  return; }
  if (r == "MB") { maquinaActiva = false; return; }
  if (r == "I1A") { ir1 = true;  return; }
  if (r == "I1B") { ir1 = false; return; }
  if (r == "I2A") { ir2 = true;  return; }
  if (r == "I2B") { ir2 = false; return; }
  if (r.startsWith("R") && r.length() == 3) {
    uint8_t i = r[1] - '1';
    if (i < 6) rele[i] = (r[2] == 'A');
    return;
  }
  if (r.startsWith("E") && r.length() == 3) {
    estadoMaquina = (r[1] - '0') * 10 + (r[2] - '0');
    return;
  }
  if (r.startsWith("C")) {
    int p1 = r.indexOf('/');
    int p2 = r.indexOf('/', p1 + 1);
    if (p1 > 0 && p2 > 0) {
      cntEntrada = r.substring(1, p1).toInt();
      cntSalida  = r.substring(p1 + 1, p2).toInt();
      cntCiclos  = r.substring(p2 + 1).toInt();
    }
    return;
  }
  if (r.startsWith("T")) {
    tiempoRestante = r.substring(1).toInt();
    return;
  }
  if (r.startsWith("P") && r.length() >= 8) {
    pLlenado = (r[1] - '0') * 100 + (r[2] - '0') * 10 + (r[3] - '0');
    pDescomp = (r[4] - '0') * 10  + (r[5] - '0');
    uint16_t R = 0;
    for (uint8_t i = 6; i < r.length(); i++) R = R * 10 + (r[i] - '0');
    pRetiro = R;
    return;
  }
}

void leerUart() {
  while (maquina.available()) {
    char c = maquina.read();
    tUltChar = millis();
    if (c == '\n' || c == '\r') {
      if (bufUart.length() > 0) {
        bufUart.trim();
        procesarRespuesta(bufUart);
        bufUart = "";
      }
    } else {
      bufUart += c;
      if (bufUart.length() > 32) bufUart = "";
    }
  }
  if (bufUart.length() > 0 && (millis() - tUltChar > TIMEOUT_UART)) bufUart = "";
}

// ============================================================
//  Setup / Loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== HMI Llenadora v3.1 ===");

  gfx->begin();
  gfx->fillScreen(BLACK);
  ts.begin();
  ts.setRotation(0);

  maquina.begin(HMI_BAUD, SERIAL_8N1, HMI_RX, HMI_TX);

  mostrarIntro();
  tInicioIntro = millis();
}

void loop() {
  if (pantalla == P_INTRO) {
    if (millis() - tInicioIntro >= TIEMPO_INTRO) {
      pantalla = P_PRINCIPAL;
      dibujarPrincipal();          // dibujo completo UNA sola vez
      maquina.println("QS");
    }
  } else {
    leerUart();
    detectarToque();
    refrescarIncremental();        // solo actualiza zonas que cambiaron
    if (pantalla == P_MANUAL) actualizarEstadosManual();
  }
  delay(5);
}