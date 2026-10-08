#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

MCUFRIEND_kbv tft;

// Colores
#define BLACK         0x0000
#define WHITE         0xFFFF
#define RED           0xF800       
#define NARANJA       0xFD20
#define AMARILLO      0xFFE0
#define BLUE_BLOCK    0x07FF      
#define GRAY          0x8410       
#define DARKGRAY      0x4208       
#define LIGHTGRAY     0xC618       
#define VERDE         0x07E0
#define NEW_BLACK_SCREEN 0x0000 

// --- Colores para los estados de la máquina ---
#define COLOR_HIGH    VERDE           
#define COLOR_LOW_COMP DARKGRAY      

// ===== VARIABLES GLOBALES PARA EL ESTADO =====
int datos[9] = {5, 5, 5, 5, 0, 0, -1, 0, 0}; 
int old_datos[9] = {-1, -1, -1, -1, -1, -1, -2, -1, -1};

// --- COORDENADAS AJUSTADAS ---
int textX_label = 15;   // Posición X para la etiqueta (ajustado por el nuevo marco)
int textX_value = 110;  // Posición X para el valor numérico
int textY[5] = {75, 90, 105, 120, 135}; 

const int yOffsets[5] = {0, 6, 12, 17, 22}; 

// Variables para el layout
int blackScreenY;
int blackScreenH;
int areaX;
int centerX;
int startX;
int totalBlocksWidth;

// --- VARIABLES GLOBALES PARA RESALTAR COMPONENTES (Dimensiones mejoradas para mejor dibujo) ---
int PISTON1_X, PISTON1_Y = 90, PISTON1_W = 18, PISTON1_H = 40;
int PISTON2_X, PISTON2_Y = 90, PISTON2_W = 18, PISTON2_H = 40;
int BOMBA_X, BOMBA_Y = 10, BOMBA_W = 26, BOMBA_H = 26;
int CINTA_X, CINTA_Y = 165, CINTA_W, CINTA_H = 45;

char serialBuffer[100];
bool keybidon = 0;

// --- Prototipos ---
bool recibirDatosSerial();
void actualizarValoresTiempos();
void actualizarCajaSeleccion();
void dibujarCajaSeleccion(int index, uint16_t color);
void resaltarComponente(int x, int y, int w, int h, uint16_t color, int tipo);
void actualizarEstado();

// Prototipos de nuevas funciones de dibujo visual
void dibujarPiston(int x, int y, int w, int h, uint16_t color);
void dibujarBomba(int x, int y, int w, int h, uint16_t color);
void dibujarCinta(int x, int y, int w, int h, uint16_t color);
void dibujarBidon(int x, int y, int w, int h, uint16_t color);
void dibujarPicos(int startX, int spacing, uint16_t color);
void dibujarAntigoteo(int centerX, uint16_t color);

// ===================================
// =====         SETUP           =====
// ===================================
void setup() {
  Serial.begin(9600);
  
  uint16_t ID = tft.readID();
  if (ID == 0x0000 || ID == 0xFFFF) {
    Serial.println(F("ID invalido leido. Forzando ID 0x9341 para compatibilidad."));
    ID = 0x9341; 
  }
  
  tft.begin(ID);
  tft.setRotation(1);

  // ===== MEDIDAS Y POSICIONAMIENTO =====
  int dataAreaWidth = 180;
  areaX = dataAreaWidth;

  int areaWidth = 300;
  int machineAreaH = 220;
  centerX = areaX + areaWidth / 2;
  int numBlocks = 4;
  int blockSpacing = 45;
  totalBlocksWidth = numBlocks * blockSpacing - 5;
  startX = centerX - totalBlocksWidth / 2 + 2;

  PISTON1_X = areaX + 20;
  PISTON2_X = areaX + areaWidth - 20 - PISTON2_W;
  BOMBA_X = centerX - BOMBA_W / 2;
  CINTA_X = areaX + 10;
  CINTA_W = areaWidth - 20;

  // ===== DIBUJO DE FONDO Y PANELES =====
  tft.fillScreen(GRAY);
  
  // Panel izquierdo de tiempos (con marco elegante)
  tft.fillRoundRect(0, 35, areaX - 5, 180, 8, WHITE);
  tft.drawRoundRect(0, 35, areaX - 5, 180, 8, BLACK);
  
  // Panel derecho de la máquina
  tft.fillRoundRect(areaX, 0, areaWidth, machineAreaH, 8, VERDE);
  tft.drawRoundRect(areaX, 0, areaWidth, machineAreaH, 8, BLACK);
  
  
  // Título del área de máquina
  tft.fillRoundRect(0, 0, areaX + 10, 34, 20, BLACK);
  tft.drawRoundRect(0, 0, areaX +10, 34, 20, NARANJA);
  tft.setTextColor(AMARILLO);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("MAQUINAS GUERRA");

  // ===== DIBUJO DE LA MÁQUINA (Inicial) =====
  //resaltarComponente(PISTON1_X, PISTON1_Y, PISTON1_W, PISTON1_H, COLOR_LOW_COMP, 0);
  resaltarComponente(PISTON2_X, PISTON2_Y, PISTON2_W, PISTON2_H, COLOR_LOW_COMP, 0);
  resaltarComponente(BOMBA_X, BOMBA_Y, BOMBA_W, BOMBA_H, COLOR_LOW_COMP, 0); 
  resaltarComponente(CINTA_X, CINTA_Y, CINTA_W, CINTA_H, COLOR_LOW_COMP, 1);
  resaltarComponente(0, 0, 0, 0, COLOR_LOW_COMP, 2); // Picos
  resaltarComponente(0, 0, 0, 0, COLOR_LOW_COMP, 3); // Antigoteo
  
  tft.fillRect(startX + 7, 15, totalBlocksWidth - 20, 15, BLACK);
  tft.drawRect(areaX, 0, areaWidth, machineAreaH, NARANJA); 
  tft.fillRect(centerX - 5, 0, 10, 30, BLACK); 
  
  bidones(); // Dibuja los bidones iniciales
  
  // ===== PANTALLA INFERIOR (Notificaciones) =====
  blackScreenY = machineAreaH;
  blackScreenH = 320 - blackScreenY;
  tft.fillRect(0, blackScreenY, 480, blackScreenH, NEW_BLACK_SCREEN);
  tft.drawLine(0, blackScreenY, 480, blackScreenY, WHITE); // Línea separadora
  
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.setCursor(160, blackScreenY + 10); // Centrado visualmente
  tft.print("NOTIFICACIONES");
  
  tft.setTextColor(WHITE);
  tft.setCursor(180, blackScreenY + 60);
  tft.print("DETENIDO");
  
  // ===== TEXTOS DE TIEMPOS Y ESTADO =====
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 45);
  tft.print("TIEMPOS:");
  
  tft.setTextSize(2);
  // Etiquetas descriptivas
  tft.setCursor(textX_label, textY[0] + yOffsets[0]); tft.print("LLENADO:"); 
  tft.setCursor(textX_label, textY[1] + yOffsets[1]); tft.print("DESCOMP:");
  tft.setCursor(textX_label, textY[2] + yOffsets[2]); tft.print("INGRESO:");
  tft.setCursor(textX_label, textY[3] + yOffsets[3]); tft.print("DESPACHO:");
  tft.setCursor(textX_label, textY[4] + yOffsets[4]); tft.print("Ciclos:"); 
  
  // Valores iniciales
  for (int i = 0; i < 5; i++) {
    tft.setCursor(textX_value, textY[i] + yOffsets[i]);
    tft.print(datos[i]);
  }
  
  dibujarCajaSeleccion(datos[5], RED);
}

// ===================================
// =====      LOOP PRINCIPAL     =====
// ===================================
void loop() {
  if (recibirDatosSerial()) {
    actualizarValoresTiempos();
    actualizarCajaSeleccion();
    actualizarEstado();
  }
}

// ===================================
// ===== FUNCIONES AUXILIARES    =====
// ===================================

bool recibirDatosSerial() {
  if (Serial.available() > 0) {
    String dataIn = Serial.readStringUntil('\n');
    dataIn.trim();
    dataIn.toCharArray(serialBuffer, 100);
    
    char* part = strtok(serialBuffer, ",");
    int i = 0;
    while (part != NULL && i < 9) {
      datos[i] = atoi(part);
      part = strtok(NULL, ",");
      i++;
    }
    return (i == 9);
  }
  return false;
}

void actualizarValoresTiempos() {
  tft.setTextSize(2);
  for (int i = 0; i < 5; i++) {
    if (datos[i] != old_datos[i]) {
      int currentY = textY[i] + yOffsets[i]; 
      // Limpia el valor antiguo con el color del panel (WHITE)
      tft.fillRect(textX_value - 3, currentY - 4, 60, 20, WHITE); 
      tft.setTextColor(BLACK);
      tft.setCursor(textX_value, currentY);
      tft.print(datos[i]);
      old_datos[i] = datos[i];
    }
  }
}

void actualizarCajaSeleccion() {
  int seleccionActual = datos[5];
  int seleccionAntigua = old_datos[5];
  
  if (seleccionActual != seleccionAntigua) {
    if (seleccionAntigua >= 0 && seleccionAntigua < 4) {
      dibujarCajaSeleccion(seleccionAntigua, WHITE); // Limpia con el fondo del panel
    }
    if (seleccionActual >= 0 && seleccionActual < 4) {
      dibujarCajaSeleccion(seleccionActual, RED);
    }
    old_datos[5] = seleccionActual;
  }
}

void dibujarCajaSeleccion(int index, uint16_t color) {
  if (index >= 0 && index < 4) { 
    int x = textX_value;
    int y = textY[index] + yOffsets[index]; 
    tft.drawRect(x - 5, y - 5, 70, 20, color); 
  }
}

/**
 * Dibuja/resalta un componente específico con diseño mejorado.
 */
void resaltarComponente(int x, int y, int w, int h, uint16_t color, int tipo) {
  uint16_t activeColor = (color == COLOR_HIGH) ? COLOR_HIGH : COLOR_LOW_COMP;
  
  if (tipo == 0) { 
    // Diferenciamos bomba de pistón por su altura (la bomba es más pequeña en el diseño)
    if (h <= 30) {
      dibujarBomba(x, y, w, h, activeColor);
    } else {
      dibujarPiston(x, y, w, h, activeColor);
    }
  } else if (tipo == 1) { 
    dibujarCinta(x, y, w, h, activeColor);
  } else if (tipo == 2) { 
    dibujarPicos(startX, 45, activeColor);
  } else if (tipo == 3) { 
    dibujarAntigoteo(centerX, activeColor);
  }
}

void actualizarEstado() {
  int etapa = datos[6];
  int enProceso = datos[7];
  int tiempoRestante = datos[8];

  if (etapa != old_datos[6] || enProceso != old_datos[7] || tiempoRestante != old_datos[8]) {
    
    tft.fillRect(0, blackScreenY + 40, 480, 80, NEW_BLACK_SCREEN);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);

    uint16_t CINTA_COLOR = COLOR_LOW_COMP;
    uint16_t P1_COLOR = COLOR_LOW_COMP;
    uint16_t P2_COLOR = COLOR_LOW_COMP;
    uint16_t BOMBA_COLOR = COLOR_LOW_COMP;
    uint16_t PICOS_COLOR = COLOR_LOW_COMP;
    uint16_t ANTIGOTEO_COLOR = COLOR_LOW_COMP;
    
    if (enProceso == 0) {
      if (old_datos[7] == 1) {
        tft.setCursor(170, blackScreenY + 60);
        tft.print("PROCESO DETENIDO");
      } else {
        tft.setCursor(190, blackScreenY + 60);
        tft.print("EN PAUSA");
      }
    } else {
      tft.setCursor(10, blackScreenY + 40);
      tft.print("PROCESO ACTIVO - ETAPA: ");
      
      // Corrección de lógica de impresión para evitar "llenando5"
      if (etapa == 5) { tft.print("LLENANDO"); }
      else if (etapa == 6) { tft.print("DESCOMPRES"); }
      else { tft.print(etapa); }

      tft.setCursor(10, blackScreenY + 70);
      tft.print("TIEMPO RESTANTE: ");
      tft.print(tiempoRestante);
      tft.print("s");
      
      switch (etapa) {
          case 1: // ETAPA 1: Cinta y Pistón 2 activos
              CINTA_COLOR = COLOR_HIGH;
              P2_COLOR = COLOR_HIGH;
              break;
          case 2: // ETAPA 2: Pistón 1 activo, Antigoteo ON
              P1_COLOR = COLOR_HIGH;
              ANTIGOTEO_COLOR = COLOR_HIGH;
              break;
          case 3: // ETAPA 3: Picos ON (espera de 500ms)
              P1_COLOR = COLOR_HIGH;
              ANTIGOTEO_COLOR = COLOR_HIGH;
              PICOS_COLOR = COLOR_HIGH;
              break;
          case 4: // ETAPA 4: Bomba ON (Tiempo 1)
              P1_COLOR = COLOR_HIGH;
              ANTIGOTEO_COLOR = COLOR_HIGH;
              PICOS_COLOR = COLOR_HIGH;
             
              break;
          case 5: // ETAPA 5: Bomba OFF (Tiempo 2)
          keybidon=1;
               BOMBA_COLOR = COLOR_HIGH;
              P1_COLOR = COLOR_HIGH;
              bidones();
              ANTIGOTEO_COLOR = COLOR_HIGH;
              PICOS_COLOR = COLOR_HIGH;
              break;
          case 6: // ETAPA 6: Picos OFF, Antigoteo OFF, Pistón 2 OFF (Tiempo 3)
              P1_COLOR = COLOR_HIGH;
               BOMBA_COLOR = COLOR_LOW_COMP;
              break;
          case 7: // ETAPA 7: Pistón 1 OFF (Tiempo 4) -> Reinicio
          CINTA_COLOR = COLOR_HIGH;P1_COLOR = COLOR_HIGH;
          
          keybidon=0;bidones();
              break;
      }
    } 

    resaltarComponente(PISTON1_X, PISTON1_Y, PISTON1_W, PISTON1_H, P1_COLOR, 0); 
    resaltarComponente(PISTON2_X, PISTON2_Y, PISTON2_W, PISTON2_H, P2_COLOR, 0);
    resaltarComponente(BOMBA_X, BOMBA_Y, BOMBA_W, BOMBA_H, BOMBA_COLOR, 0); 
    resaltarComponente(CINTA_X, CINTA_Y, CINTA_W, CINTA_H, CINTA_COLOR, 1);
    resaltarComponente(0, 0, 0, 0, PICOS_COLOR, 2);
    resaltarComponente(0, 0, 0, 0, ANTIGOTEO_COLOR, 3);
    
    old_datos[6] = etapa;
    old_datos[7] = enProceso;
    old_datos[8] = tiempoRestante;
  }
}

void bidones() {
  int blockSpacing = 45;
  int blockY = 100;
  int blockWidth = 40;
  int blockHeight = 50;
  
  for (int i = 0; i < 4; i++) {
    int x = startX + i * blockSpacing;
    uint16_t color = (keybidon == 0) ? GRAY : BLUE_BLOCK;
    dibujarBidon(x, blockY, blockWidth, blockHeight, color);
  }
}

// =========================================================
// ===== NUEVAS FUNCIONES DE DIBUJO VISUAL MEJORADO =====
// =========================================================

void dibujarPiston(int x, int y, int w, int h, uint16_t color) {
  // Cuerpo principal del pistón
  tft.fillRoundRect(x, y + h/3, w, h - h/3, 4, color);
  tft.drawRoundRect(x, y + h/3, w, h - h/3, 4, BLACK);
  // Vástago (parte superior)
  tft.fillRect(x + w/3, y, w/3, h/3, LIGHTGRAY);
  tft.drawRect(x + w/3, y, w/3, h/3, BLACK);
  // Brillo lateral (efecto 3D)
  tft.drawLine(x + 3, y + h/3 + 2, x + 3, y + h - 3, WHITE);
}

void dibujarBomba(int x, int y, int w, int h, uint16_t color) {
  int cx = x + w/2;
  int cy = y + h/2;
  int r = h/2;
  // Cuerpo de la bomba
  tft.fillCircle(cx, cy, r, color);
  tft.drawCircle(cx, cy, r, BLACK);
  // Detalle central (eje)
  tft.fillCircle(cx, cy, r/3, WHITE);
  tft.drawCircle(cx, cy, r/3, BLACK);
  // Aspas o detalle de motor
  tft.drawLine(cx - r + 4, cy, cx + r - 4, cy, BLACK);
  tft.drawLine(cx, cy - r + 4, cx, cy + r - 4, BLACK);
}

void dibujarCinta(int x, int y, int w, int h, uint16_t color) {
  // Base metálica exterior
  tft.fillRoundRect(x, y, w, h, 8, DARKGRAY);
  tft.drawRoundRect(x, y, w, h, 8, BLACK);
  
  // Rodillos laterales
  int r = h/2 - 6;
  tft.fillCircle(x + r + 6, y + h/2, r, LIGHTGRAY);
  tft.drawCircle(x + r + 6, y + h/2, r, BLACK);
  tft.fillCircle(x + w - r - 6, y + h/2, r, LIGHTGRAY);
  tft.drawCircle(x + w - r - 6, y + h/2, r, BLACK);
  
  // Superficie de la cinta (parte móvil)
  tft.fillRoundRect(x + r + 6, y + 6, w - 2*r - 12, h - 12, 4, color);
  tft.drawRoundRect(x + r + 6, y + 6, w - 2*r - 12, h - 12, 4, BLACK);
  
  // Segmentos para simular textura o movimiento
  for(int i = x + r + 14; i < x + w - r - 14; i += 20) {
    tft.drawLine(i, y + 8, i, y + h - 8, BLACK);
  }
}

void dibujarBidon(int x, int y, int w, int h, uint16_t color) {
  // Cuerpo del bidón
  tft.fillRoundRect(x, y + h/3, w, h - h/3, 5, color);
  tft.drawRoundRect(x, y + h/3, w, h - h/3, 5, BLACK);
  // Cuello del bidón
  tft.fillRect(x + w/4, y, w/2, h/3, color);
  tft.drawRect(x + w/4, y, w/2, h/3, BLACK);
  // Tapa
  tft.fillRect(x + w/4 - 2, y, w/2 + 4, 5, BLACK);
  // Brillo y sombra lateral (efecto 3D)
  tft.drawLine(x + 4, y + h/3 + 4, x + 4, y + h - 4, WHITE);
  tft.drawLine(x + w - 4, y + h/3 + 4, x + w - 4, y + h - 4, DARKGRAY);
}

void dibujarPicos(int startX, int spacing, uint16_t color) {
  int numBlocks = 4;
  int clawY = 35;
  int clawWidth = 18;
  int clawHeight = 35;
  for (int i = 0; i < numBlocks; i++) {
    int cx = startX + 10 + i * spacing;
    // Embudo superior (triángulo)
    tft.fillTriangle(cx, clawY, cx + clawWidth, clawY, cx + clawWidth/2, clawY + clawHeight/2, color);
    tft.drawTriangle(cx, clawY, cx + clawWidth, clawY, cx + clawWidth/2, clawY + clawHeight/2, BLACK);
    // Boquilla inferior (rectángulo)
    tft.fillRect(cx + clawWidth/4, clawY + clawHeight/2, clawWidth/2, clawHeight/2, color);
    tft.drawRect(cx + clawWidth/4, clawY + clawHeight/2, clawWidth/2, clawHeight/2, BLACK);
    // Brillo
    tft.drawLine(cx + 3, clawY + 2, cx + 3, clawY + clawHeight/2, WHITE);
  }
}

void dibujarAntigoteo(int centerX, uint16_t color) {
  int x = centerX - 85;
  int y = 75;
  int w = 170;
  int h = 14;
  // Bandeja
  tft.fillRoundRect(x, y, w, h, 4, color);
  tft.drawRoundRect(x, y, w, h, 4, BLACK);
  // Brillo superior
  tft.drawLine(x + 4, y + 2, x + w - 4, y + 2, WHITE);
  // Sombra inferior
  tft.drawLine(x + 4, y + h - 2, x + w - 4, y + h - 2, DARKGRAY);
}