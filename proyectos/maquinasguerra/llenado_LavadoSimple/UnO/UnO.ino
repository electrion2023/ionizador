#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>

MCUFRIEND_kbv tft;

// =====================================================
// COLORES
// =====================================================
#define BLACK            0x0000
#define WHITE            0xFFFF
#define NARANJA_OSCURO   0xFB00
#define NARANJA_MEDIO    0xFD68
#define NARANJA_BRILLANTE 0xFBE0
#define AMARILLO_BRILLANTE 0xFFE0
#define AMARILLO_SUAVE   0xFF80
#define GRIS_OSCURO      0x4208

// =====================================================
// GEOMETRIA UI
// =====================================================
int CARD_L_X, CARD_R_X, CARD_Y, CARD_W, CARD_H, BAR_Y;

// =====================================================
// DATOS
// =====================================================
unsigned long tiempoHidro = 5000;
unsigned long tiempoBomba = 10000;

unsigned long restanteHidroMs = 0;
unsigned long restanteBombaMs = 0;

// =====================================================
// CONTROL DE REDRAW
// =====================================================
unsigned long lastTiempoHidro = 0xFFFFFFFFUL;
unsigned long lastTiempoBomba = 0xFFFFFFFFUL;

unsigned long lastRestHidro = 0xFFFFFFFFUL;
unsigned long lastRestBomba = 0xFFFFFFFFUL;

int lastSegHidro = -1;
int lastSegBomba = -1;

bool lastActivoHidro = false;
bool lastActivoBomba = false;

int lastBarFill[2] = {-1, -1};
uint16_t lastBarColor[2] = {0, 0};

// =====================================================
// RECEPCION SERIAL
// =====================================================
const byte numChars = 32;
char receivedChars[numChars];
bool newData = false;

// =====================================================
// FUNCIONES UI
// =====================================================
void calcularGeometria() {
  int w = tft.width();
  int h = tft.height();

  CARD_Y = 44;
  CARD_L_X = 8;
  CARD_W = (w - 24) / 2;
  CARD_R_X = w - CARD_L_X - CARD_W;

  BAR_Y = h - 26;
  CARD_H = BAR_Y - CARD_Y - 12;
}

void textoCentrado(int areaX, int y, int areaW, const char* texto, uint16_t color, uint16_t bg, uint8_t tam) {
  int ancho = strlen(texto) * 6 * tam;
  int x = areaX + (areaW - ancho) / 2;

  if (x < areaX) x = areaX;

  tft.setTextColor(color, bg);
  tft.setTextSize(tam);
  tft.setCursor(x, y);
  tft.print(texto);
}

void dibujarBase() {
  tft.fillScreen(BLACK);

  // Título superior izquierdo
  textoCentrado(0, 12, tft.width() / 2, "HIDROLAVADORA", NARANJA_BRILLANTE, BLACK, 2);

  // Título superior derecho
  textoCentrado(tft.width() / 2, 12, tft.width() / 2, "BOMBA LLENADO", AMARILLO_SUAVE, BLACK, 2);

  // Líneas decorativas
  tft.drawFastHLine(0, 36, tft.width(), NARANJA_OSCURO);
  tft.drawFastVLine(tft.width() / 2, 42, CARD_Y + CARD_H - 42, GRIS_OSCURO);

  // Tarjetas
  tft.drawRoundRect(CARD_L_X, CARD_Y, CARD_W, CARD_H, 10, NARANJA_MEDIO);
  tft.drawRoundRect(CARD_R_X, CARD_Y, CARD_W, CARD_H, 10, NARANJA_MEDIO);

  // Línea superior decorativa de cada tarjeta
  tft.fillRect(CARD_L_X + 10, CARD_Y + 2, CARD_W - 20, 2, NARANJA_OSCURO);
  tft.fillRect(CARD_R_X + 10, CARD_Y + 2, CARD_W - 20, 2, AMARILLO_SUAVE);

  // Bordes de barras de progreso
  tft.drawRect(CARD_L_X + 10, CARD_Y + CARD_H - 24, CARD_W - 20, 12, GRIS_OSCURO);
  tft.drawRect(CARD_R_X + 10, CARD_Y + CARD_H - 24, CARD_W - 20, 12, GRIS_OSCURO);

  // Etiquetas
  textoCentrado(CARD_L_X, CARD_Y + CARD_H - 10, CARD_W, "SEGUNDOS", GRIS_OSCURO, BLACK, 1);
  textoCentrado(CARD_R_X, CARD_Y + CARD_H - 10, CARD_W, "SEGUNDOS", GRIS_OSCURO, BLACK, 1);

  // Barra inferior
  int yBar = tft.height() - 26;
  tft.fillRect(0, yBar, tft.width(), 26, NARANJA_BRILLANTE);
  textoCentrado(0, yBar + 5, tft.width(), "MAQUINAS GUERRA", BLACK, NARANJA_BRILLANTE, 2);
}

int obtenerSegundosVisibles(int idx) {
  unsigned long setMs = (idx == 0) ? tiempoHidro : tiempoBomba;
  unsigned long restMs = (idx == 0) ? restanteHidroMs : restanteBombaMs;

  if (restMs > 0) {
    return (int)((restMs + 999UL) / 1000UL);
  }

  return (int)(setMs / 1000UL);
}

void actualizarSet(int idx) {
  int x = (idx == 0) ? CARD_L_X : CARD_R_X;
  unsigned long setMs = (idx == 0) ? tiempoHidro : tiempoBomba;

  char buf[16];
  snprintf(buf, sizeof(buf), "SET %lu s", setMs / 1000UL);

  tft.fillRect(x + 5, CARD_Y + 8, CARD_W - 10, 12, BLACK);
  textoCentrado(x, CARD_Y + 10, CARD_W, buf, AMARILLO_SUAVE, BLACK, 1);
}

void actualizarEstado(int idx) {
  int x = (idx == 0) ? CARD_L_X : CARD_R_X;

  bool activo = (idx == 0) ? (restanteHidroMs > 0) : (restanteBombaMs > 0);

  const char* txt = activo ? "ACTIVO" : "LISTO";

  uint16_t color;
  if (activo) {
    color = (idx == 0) ? NARANJA_BRILLANTE : AMARILLO_BRILLANTE;
  } else {
    color = WHITE;
  }

  tft.fillRect(x + 5, CARD_Y + 28, CARD_W - 10, 20, BLACK);
  textoCentrado(x, CARD_Y + 31, CARD_W, txt, color, BLACK, 2);
}

void actualizarNumero(int idx, bool forzar) {
  int seg = obtenerSegundosVisibles(idx);

  int* last = (idx == 0) ? &lastSegHidro : &lastSegBomba;

  if (!forzar && seg == *last) return;

  *last = seg;

  int x = (idx == 0) ? CARD_L_X : CARD_R_X;

  bool activo = (idx == 0) ? (restanteHidroMs > 0) : (restanteBombaMs > 0);

  char buf[6];
  snprintf(buf, sizeof(buf), "%d", seg);

  uint16_t color;
  if (activo) {
    color = (idx == 0) ? NARANJA_BRILLANTE : AMARILLO_BRILLANTE;
  } else {
    color = WHITE;
  }

  tft.fillRect(x + 5, CARD_Y + 52, CARD_W - 10, 62, BLACK);
  textoCentrado(x, CARD_Y + 56, CARD_W, buf, color, BLACK, 7);
}

void actualizarBarra(int idx) {
  int x = (idx == 0) ? CARD_L_X : CARD_R_X;

  int bx = x + 10;
  int by = CARD_Y + CARD_H - 24;
  int bw = CARD_W - 20;
  int bh = 12;
  int innerW = bw - 2;

  unsigned long setMs = (idx == 0) ? tiempoHidro : tiempoBomba;
  unsigned long restMs = (idx == 0) ? restanteHidroMs : restanteBombaMs;

  int fill = 0;

  if (restMs == 0) {
    // En reposo muestro barra llena en gris suave.
    fill = innerW;
  } else if (setMs > 0) {
    fill = (int)(((unsigned long)restMs * (unsigned long)innerW) / setMs);
  }

  if (fill > innerW) fill = innerW;
  if (fill < 0) fill = 0;

  uint16_t color;
  if (restMs > 0) {
    color = (idx == 0) ? NARANJA_BRILLANTE : AMARILLO_BRILLANTE;
  } else {
    color = GRIS_OSCURO;
  }

  int oldFill = lastBarFill[idx];
  uint16_t oldColor = lastBarColor[idx];

  // Actualización optimizada para reducir parpadeo
  if (oldFill < 0 || oldColor != color) {
    tft.fillRect(bx + 1, by + 1, innerW, bh - 2, BLACK);
    if (fill > 0) {
      tft.fillRect(bx + 1, by + 1, fill, bh - 2, color);
    }
  } else if (fill < oldFill) {
    tft.fillRect(bx + 1 + fill, by + 1, oldFill - fill, bh - 2, BLACK);
  } else if (fill > oldFill) {
    tft.fillRect(bx + 1 + oldFill, by + 1, fill - oldFill, bh - 2, color);
  }

  lastBarFill[idx] = fill;
  lastBarColor[idx] = color;
}

// =====================================================
// RECEPCION Y PARSEO
// =====================================================
void recibirDatos() {
  static bool recvInProgress = false;
  static byte ndx = 0;

  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) ndx = numChars - 1;
      } else {
        receivedChars[ndx] = '\0';
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }
    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

void parsearDatos() {
  char* p = receivedChars;

  bool tieneH = false;
  bool tieneB = false;

  while (*p) {
    if (*p == 'H') {
      p++;
      tiempoHidro = strtoul(p, &p, 10);
      tieneH = true;
    }
    else if (*p == 'B') {
      p++;
      tiempoBomba = strtoul(p, &p, 10);
      tieneB = true;
    }
    else if (*p == 'h') {
      p++;
      restanteHidroMs = strtoul(p, &p, 10);
    }
    else if (*p == 'b') {
      p++;
      restanteBombaMs = strtoul(p, &p, 10);
    }
    else {
      p++;
    }
  }

  // Si por alguna razón no vinieron los tiempos, mantengo límites seguros.
  if (tiempoHidro < 1000UL) tiempoHidro = 1000UL;
  if (tiempoHidro > 60000UL) tiempoHidro = 60000UL;

  if (tiempoBomba < 1000UL) tiempoBomba = 1000UL;
  if (tiempoBomba > 60000UL) tiempoBomba = 60000UL;

  if (restanteHidroMs > tiempoHidro) restanteHidroMs = tiempoHidro;
  if (restanteBombaMs > tiempoBomba) restanteBombaMs = tiempoBomba;
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(9600);

  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486;

  tft.begin(ID);
  tft.setRotation(1);

  calcularGeometria();
  dibujarBase();

  // Primer render
  actualizarSet(0);
  actualizarSet(1);

  actualizarEstado(0);
  actualizarEstado(1);

  actualizarNumero(0, true);
  actualizarNumero(1, true);

  actualizarBarra(0);
  actualizarBarra(1);

  lastTiempoHidro = tiempoHidro;
  lastTiempoBomba = tiempoBomba;

  lastRestHidro = restanteHidroMs;
  lastRestBomba = restanteBombaMs;

  lastActivoHidro = (restanteHidroMs > 0);
  lastActivoBomba = (restanteBombaMs > 0);
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  recibirDatos();

  if (newData) {
    parsearDatos();
    newData = false;
  }

  // ===================================================
  // CAMBIOS EN TIEMPO CONFIGURADO HIDRO
  // ===================================================
  if (tiempoHidro != lastTiempoHidro) {
    actualizarSet(0);
    actualizarBarra(0);
    actualizarNumero(0, true);
    lastTiempoHidro = tiempoHidro;
  }

  // ===================================================
  // CAMBIOS EN TIEMPO CONFIGURADO BOMBA
  // ===================================================
  if (tiempoBomba != lastTiempoBomba) {
    actualizarSet(1);
    actualizarBarra(1);
    actualizarNumero(1, true);
    lastTiempoBomba = tiempoBomba;
  }

  // ===================================================
  // CAMBIOS EN RESTANTE HIDRO
  // ===================================================
  if (restanteHidroMs != lastRestHidro) {
    bool activo = (restanteHidroMs > 0);

    actualizarBarra(0);

    if (activo != lastActivoHidro) {
      actualizarEstado(0);
      actualizarNumero(0, true);
      lastActivoHidro = activo;
    } else {
      actualizarNumero(0, false);
    }

    lastRestHidro = restanteHidroMs;
  }

  // ===================================================
  // CAMBIOS EN RESTANTE BOMBA
  // ===================================================
  if (restanteBombaMs != lastRestBomba) {
    bool activo = (restanteBombaMs > 0);

    actualizarBarra(1);

    if (activo != lastActivoBomba) {
      actualizarEstado(1);
      actualizarNumero(1, true);
      lastActivoBomba = activo;
    } else {
      actualizarNumero(1, false);
    }

    lastRestBomba = restanteBombaMs;
  }
}