#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

MCUFRIEND_kbv tft;

// ================= COLORES (RGB565) =================
#define BG_DARK       0x0825  
#define PANEL_BG      0x18E7  
#define NEON_CYAN     0x07FF  
#define NEON_GREEN    0x07E0  
#define NEON_ORANGE   0xFD20  
#define MACHINE_DARK  0x31A6  
#define MACHINE_LIGHT 0x5AEB  
#define BOTTLE_GLASS  0xCE79  
#define LIQUID_COLOR  0xF800  
#define CAP_COLOR     0xFFE0  // Ya no se usa, pero se deja por compatibilidad
#define WHITE         0xFFFF
#define BLACK         0x0000
#define ALERT_RED     0xF800

// ================= VARIABLES SERIALES =================
String estadoActual = "INICIANDO";
String estadoAnterior = "";
int contador = 0;
int contadorAnterior = -1;
int tiempoTapado = 0; // Ahora representa el "Tiempo de Llenado"
int tiempoTapadoAnterior = -1;
int tiempoDespacho = 0;
int tiempoDespachoAnterior = -1;
int modoMenu = 0; 
int modoMenuAnterior = -1;

// ================= VARIABLES DE ANIMACIÓN NO BLOQUEANTE =================
unsigned long lastAnimStep = 0;
const int ANIM_INTERVAL = 30; // ms entre cada paso de animación

int botellaX = 140;
bool configInicialHecha = false;

// === CAMBIO PARA LLENADO: Variables de la boquilla y líquido ===
int nozzleY = 60;          // Posición vertical de la boquilla de llenado
int liquidH = 0;           // Altura actual del líquido (0 a 60)
bool isFilling = false;    // Estado del proceso de llenado
unsigned long fillStartTime = 0;

// Buffer para lectura serial no bloqueante
String serialBuffer = "";

// =====================================================
// DIBUJA EL ENTORNO FIJO
// =====================================================
void drawStaticBackground() {
  tft.fillScreen(BG_DARK);

  tft.fillRect(0, 0, 480, 35, NEON_CYAN); 
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.print("MAQUINARIAS GUERRA");

  tft.fillRoundRect(10, 50, 140, 260, 8, PANEL_BG);
  tft.drawRoundRect(10, 50, 140, 260, 8, NEON_CYAN);

  tft.setTextColor(NEON_CYAN);
  tft.setTextSize(1);
  
  tft.setCursor(20, 65);  tft.print("ESTADO:");
  tft.setCursor(20, 115); tft.print("PRODUCCION:");
  tft.setCursor(20, 175); tft.print("T. LLENADO (T1):"); // === CAMBIO DE TEXTO ===
  tft.setCursor(20, 235); tft.print("T. DESPACHO (T2):");

  // Estructura de la máquina (ajustada visualmente para llenadora)
  tft.fillRect(340, 60, 40, 160, MACHINE_DARK);
  tft.fillRect(345, 60, 8, 160, MACHINE_LIGHT);
  
  // Brazo de soporte de la boquilla
  tft.fillRect(230, 60, 110, 20, MACHINE_DARK);
  tft.fillRect(230, 65, 110, 6, MACHINE_LIGHT);

  tft.fillRect(165, 130, 20, 30, MACHINE_DARK);
  tft.fillRect(167, 130, 5, 30, MACHINE_LIGHT);

  tft.fillRect(305, 130, 20, 30, MACHINE_DARK);
  tft.fillRect(307, 130, 5, 30, MACHINE_LIGHT);

  tft.fillRoundRect(155, 280, 315, 30, 5, MACHINE_DARK);
  tft.drawRoundRect(155, 280, 315, 30, 5, NEON_CYAN);
  
  tft.fillCircle(380, 75, 8, NEON_GREEN);
}

// =====================================================
// FUNCIONES GRÁFICAS
// =====================================================

// === CAMBIO PARA LLENADO: Ahora acepta el nivel de líquido ===
// El valor por defecto (60) dibuja la botella llena (útil para "SALIENDO")
void drawBottle(int x, int liquidH = 60) {
  // Cuello de la botella
  tft.fillRect(x + 12, 185, 16, 25, BOTTLE_GLASS);
  // Cuerpo de la botella
  tft.fillRoundRect(x, 210, 40, 70, 6, BOTTLE_GLASS);
  
  // Líquido dinámico en el interior
  if (liquidH > 0) {
    int maxH = 60; // Altura máxima del líquido
    int currentH = (liquidH > maxH) ? maxH : liquidH;
    int ly = 280 - currentH; // 280 es la base del cuerpo (210 + 70)
    
    tft.fillRoundRect(x + 3, ly, 34, currentH, 3, LIQUID_COLOR);
    // Pequeño brillo/espuma en la superficie del líquido para realismo
    tft.fillRect(x + 6, ly + 2, 28, 2, WHITE); 
  }
  
  // Brillo del vidrio
  tft.drawLine(x + 34, 215, x + 34, 275, WHITE);
}

void eraseBottle(int x) {
  tft.fillRect(x, 170, 46, 115, BG_DARK);
}

// === CAMBIO PARA LLENADO: Boquilla en lugar de pistón de tapado ===
void drawNozzle(int x, int y, bool isFlowing) {
  // Tubo principal de la boquilla
  tft.fillRect(x + 8, 60, 14, y - 60, MACHINE_LIGHT);
  tft.fillRect(x + 10, 60, 8, y - 60, MACHINE_DARK); 
  // Punta de la boquilla
  tft.fillRect(x + 4, y, 22, 12, MACHINE_DARK);
  tft.fillRect(x + 6, y + 2, 18, 8, MACHINE_LIGHT);
  
  // Chorro de líquido (solo si está llenando activamente)
  if (isFlowing) {
    // El chorro va desde la punta de la boquilla hasta cerca del cuello de la botella
    tft.fillRect(x + 10, y + 12, 10, 200 - (y + 12), LIQUID_COLOR);
  }
}

void eraseNozzle(int x, int y, bool wasFlowing) {
  // Limpiar el área de la boquilla y el posible chorro
  tft.fillRect(x, 60, 30, 150, BG_DARK);
}

void drawPistonFisico(int nroPiston, bool abajo) {
  int xCoord = (nroPiston == 1) ? 170 : 310;
  if (abajo) {
    tft.fillRect(xCoord, 160, 10, 120, NEON_ORANGE);
  } else {
    tft.fillRect(xCoord, 160, 10, 120, BG_DARK);
  }
}

void animateConveyor(int frame) {
  for (int i = 0; i < 13; i++) {
    int x = 160 + (i * 22) + (frame % 22);
    if (x < 455) {
      tft.fillRect(x, 285, 8, 20, MACHINE_LIGHT);
      tft.fillRect(x - 5, 285, 5, 20, MACHINE_DARK);
    }
  }
}

// =====================================================
// DASHBOARD
// =====================================================
void updateDashboardValues() {
  if (estadoActual != estadoAnterior) {
    tft.setTextSize(2);
    uint16_t colorTxt = NEON_CYAN;
    // === CAMBIO: Soporte para estado LLENANDO ===
    if (estadoActual == "TAPANDO" || estadoActual == "LLENANDO") colorTxt = NEON_CYAN; // Cyan para agua/líquido
    if (estadoActual == "SALIENDO") colorTxt = NEON_GREEN;
    
    tft.fillRect(18, 80, 125, 25, PANEL_BG); 
    tft.setTextColor(colorTxt, PANEL_BG);
    tft.setCursor(20, 85);
    tft.print(estadoActual);
    estadoAnterior = estadoActual;
  }

  if (contador != contadorAnterior) {
    tft.setTextSize(3);
    tft.setTextColor(WHITE, PANEL_BG);
    tft.fillRect(18, 130, 125, 30, PANEL_BG);
    tft.setCursor(20, 135);
    if(contador < 10) tft.print("00");
    else if(contador < 100) tft.print("0");
    tft.print(contador);
    contadorAnterior = contador;
  }

  if (tiempoTapado != tiempoTapadoAnterior || modoMenu != modoMenuAnterior) {
    tft.setTextSize(2);
    tft.fillRect(18, 195, 125, 25, PANEL_BG);
    if (modoMenu == 1) { 
      tft.setTextColor(BLACK, NEON_ORANGE);
      tft.fillRect(18, 195, 120, 22, NEON_ORANGE);
    } else {
      tft.setTextColor(WHITE, PANEL_BG);
    }
    tft.setCursor(20, 198);
    tft.print(tiempoTapado);
    tft.print(" ms");
    tiempoTapadoAnterior = tiempoTapado;
  }

  if (tiempoDespacho != tiempoDespachoAnterior || modoMenu != modoMenuAnterior) {
    tft.setTextSize(2);
    tft.fillRect(18, 255, 125, 25, PANEL_BG);
    if (modoMenu == 2) { 
      tft.setTextColor(BLACK, NEON_ORANGE);
      tft.fillRect(18, 255, 120, 22, NEON_ORANGE);
    } else {
      tft.setTextColor(WHITE, PANEL_BG);
    }
    tft.setCursor(20, 258);
    tft.print(tiempoDespacho);
    tft.print(" ms");
    tiempoDespachoAnterior = tiempoDespacho;
  }
  modoMenuAnterior = modoMenu;
}

// =====================================================
// LECTURA SERIAL NO BLOQUEANTE
// =====================================================
void processSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        int coma1 = serialBuffer.indexOf(',');
        int coma2 = serialBuffer.indexOf(',', coma1 + 1);
        int coma3 = serialBuffer.indexOf(',', coma2 + 1);
        int coma4 = serialBuffer.indexOf(',', coma3 + 1);

        if (coma1 > 0 && coma2 > 0 && coma3 > 0 && coma4 > 0) {
          estadoActual = serialBuffer.substring(0, coma1);
          contador = serialBuffer.substring(coma1 + 1, coma2).toInt();
          tiempoTapado = serialBuffer.substring(coma2 + 1, coma3).toInt();
          tiempoDespacho = serialBuffer.substring(coma3 + 1, coma4).toInt();
          modoMenu = serialBuffer.substring(coma4 + 1).toInt();
          
          configInicialHecha = true;
          
          // Resetear estados de animación si cambia el estado principal
          if (estadoActual != estadoAnterior) {
            isFilling = false;
            if (estadoActual == "DETECTADO") botellaX = 160;
            
            // === CAMBIO: Resetear variables de llenado ===
            if (estadoActual == "LLENANDO" || estadoActual == "TAPANDO") {
              nozzleY = 60;
              liquidH = 0;
              isFilling = false;
            }
          }
        }
        serialBuffer = ""; 
      }
    } else {
      serialBuffer += c;
      if (serialBuffer.length() > 64) serialBuffer = ""; 
    }
  }
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
  
  drawStaticBackground();
}

// =====================================================
// LOOP PRINCIPAL (NO BLOQUEANTE)
// =====================================================
void loop() {
  processSerial();

  if (!configInicialHecha) return;

  updateDashboardValues();

  if (millis() - lastAnimStep >= ANIM_INTERVAL) {
    lastAnimStep = millis();
    
    if (modoMenu == 0) {
      if (estadoActual == "ESPERANDO") {
        tft.fillCircle(380, 75, 8, NEON_CYAN);
        drawPistonFisico(1, false);
        drawPistonFisico(2, true);
        static int f = 0;
        animateConveyor(f++);
      }
      else if (estadoActual == "DETECTADO") {
        drawPistonFisico(1, true);
        if (botellaX < 250) {
          eraseBottle(botellaX - 5);
          botellaX += 5;
          drawBottle(botellaX, 0); // Botella vacía al entrar
          animateConveyor(botellaX);
        }
      }
      // === CAMBIO: Animación de LLENADO (acepta "LLENANDO" o "TAPANDO" por compatibilidad) ===
      else if (estadoActual == "LLENANDO" || estadoActual == "TAPANDO") {
        tft.fillCircle(380, 75, 8, NEON_CYAN); // Luz cyan para indicar líquido
        
        if (!isFilling) {
          // Fase 1: Bajada de la boquilla
          if (nozzleY < 180) {
            eraseNozzle(260, nozzleY - 5, false);
            nozzleY += 5;
            drawNozzle(260, nozzleY, false);
          } else {
            // Llegó a la posición de llenado, iniciar proceso
            isFilling = true;
            fillStartTime = millis();
            liquidH = 0;
          }
        } else {
          // Fase 2: Llenado progresivo del líquido
          unsigned long elapsed = millis() - fillStartTime;
          if (elapsed < (unsigned long)tiempoTapado) { 
            // Calcular altura del líquido (0 a 60) basado en el tiempo transcurrido
            liquidH = map(elapsed, 0, tiempoTapado, 0, 60);
            
            // Redibujar botella con el nivel actual de líquido
            eraseBottle(250);
            drawBottle(250, liquidH);
            
            // Dibujar boquilla con el chorro activo
            drawNozzle(260, nozzleY, true);
          } else {
            // Fase 3: Llenado completo, subir boquilla
            isFilling = false; 
            
            // Asegurar que la botella se dibuje completamente llena
            eraseBottle(250);
            drawBottle(250, 60);
            
            if (nozzleY > 60) {
              eraseNozzle(260, nozzleY + 5, true);
              nozzleY -= 5;
              drawNozzle(260, nozzleY, false);
            } else {
              eraseNozzle(260, 60, false);
              // Animación de llenado completa, esperando que el Nano envíe "SALIENDO"
            }
          }
        }
      }
      else if (estadoActual == "SALIENDO") {
        tft.fillCircle(380, 75, 8, NEON_GREEN);
        drawPistonFisico(2, false);
        
        if (botellaX < 480) {
          eraseBottle(botellaX - 5);
          botellaX += 5;
          drawBottle(botellaX, 60); // === CAMBIO: Botella llena al salir ===
          animateConveyor(botellaX);
        } else {
          eraseBottle(480);
        }
      }
    } 
    else {
      // Modo menú: parpadeo no bloqueante
      static bool ledOn = false;
      static unsigned long lastBlink = 0;
      if (millis() - lastBlink >= 200) {
        lastBlink = millis();
        ledOn = !ledOn;
        tft.fillCircle(380, 75, 8, ledOn ? ALERT_RED : BLACK);
      }
    }
  }
}