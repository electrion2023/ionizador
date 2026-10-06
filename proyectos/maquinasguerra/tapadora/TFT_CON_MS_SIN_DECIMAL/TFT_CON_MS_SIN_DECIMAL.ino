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
#define CAP_COLOR     0xFFE0  
#define WHITE         0xFFFF
#define BLACK         0x0000
#define ALERT_RED     0xF800

// ================= VARIABLES RECIBIDAS POR SERIAL =================
String estadoActual = "INICIANDO";
String estadoAnterior = "";
int contador = 0;
int contadorAnterior = -1;
int tiempoTapado = 0;
int tiempoTapadoAnterior = -1;
int tiempoDespacho = 0;
int tiempoDespachoAnterior = -1;
int modoMenu = 0; 
int modoMenuAnterior = -1;

// Variables internas de animación
int botellaX = 140;
int tapaY = 90;
bool configInicialHecha = false;

// =====================================================
// DIBUJA EL ENTORNO FIJO (Solo se ejecuta una vez)
// =====================================================
void drawStaticBackground() {
  tft.fillScreen(BG_DARK);

  // --- Barra superior ---
  tft.fillRect(0, 0, 480, 35, NEON_CYAN); 
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.print("MAQUINARIAS GUERRA");

  // --- Panel Izquierdo (Dashboard) ---
  tft.fillRoundRect(10, 50, 140, 260, 8, PANEL_BG);
  tft.drawRoundRect(10, 50, 140, 260, 8, NEON_CYAN);

  tft.setTextColor(NEON_CYAN);
  tft.setTextSize(1);
  
  // Títulos fijos
  tft.setCursor(20, 65);  tft.print("ESTADO:");
  tft.setCursor(20, 115); tft.print("PRODUCCION:");
  tft.setCursor(20, 175); tft.print("T. TAPADO (T1):");
  tft.setCursor(20, 235); tft.print("T. DESPACHO (T2):");

  // --- Estructura Fija de la Máquina ---
  // Torre de la tapadora
  tft.fillRect(340, 60, 40, 160, MACHINE_DARK);
  tft.fillRect(345, 60, 8, 160, MACHINE_LIGHT);
  // Brazo de la tapadora
  tft.fillRect(230, 60, 110, 30, MACHINE_DARK);
  tft.fillRect(230, 65, 110, 6, MACHINE_LIGHT);

  // Soporte físico Pistón 1 (Entrada - Delimitador)
  tft.fillRect(165, 130, 20, 30, MACHINE_DARK);
  tft.fillRect(167, 130, 5, 30, MACHINE_LIGHT);

  // Soporte físico Pistón 2 (Salida - Retenedor)
  tft.fillRect(305, 130, 20, 30, MACHINE_DARK);
  tft.fillRect(307, 130, 5, 30, MACHINE_LIGHT);

  // Base de la cinta transportadora
  tft.fillRoundRect(155, 280, 315, 30, 5, MACHINE_DARK);
  tft.drawRoundRect(155, 280, 315, 30, 5, NEON_CYAN);
  
  // Luz Piloto Central
  tft.fillCircle(380, 75, 8, NEON_GREEN);
}

// =====================================================
// FUNCIONES GRÁFICAS DE ANIMACIÓN
// =====================================================
// =====================================================
// DIBUJA BOTELLA (Se mantiene igual)
// =====================================================
void drawBottle(int x) {
  tft.fillRect(x + 12, 185, 16, 25, BOTTLE_GLASS);
  tft.fillRoundRect(x, 210, 40, 70, 6, BOTTLE_GLASS);
  tft.fillRoundRect(x + 3, 225, 34, 52, 3, LIQUID_COLOR);
  tft.drawLine(x + 34, 215, x + 34, 275, WHITE);
}

// =====================================================
// ERASE BOTELLA (CORREGIDO: Ya no invade la zona izquierda)
// =====================================================
void eraseBottle(int x) {
  // Cambiado x - 6 por x. Ahora limpia estrictamente desde donde empieza la botella hacia la derecha.
  tft.fillRect(x, 170, 46, 110, BG_DARK);
}
void drawCapAndPiston(int x, int y) {
  tft.fillRect(x + 6, 90, 10, y - 90, MACHINE_LIGHT);
  tft.fillRoundRect(x - 4, y - 8, 30, 10, 2, MACHINE_DARK);
  tft.fillRoundRect(x, y, 22, 10, 3, CAP_COLOR);
}

void eraseCapAndPiston(int x, int y) {
  tft.fillRect(x - 5, 90, 32, y + 12 - 90, BG_DARK);
}

// Control Visual de los dos pistones neumáticos de la cinta
void drawPistonFisico(int nroPiston, bool abajo) {
  int xCoord = (nroPiston == 1) ? 170 : 310;
  if (abajo) {
    tft.fillRect(xCoord, 160, 10, 120, NEON_ORANGE); // Vástago abajo (bloquea)
  } else {
    tft.fillRect(xCoord, 160, 10, 120, BG_DARK);      // Vástago arriba (libre)
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
// REFRESCAR TEXTOS DEL DASHBOARD (Optimizado)
// =====================================================
// =====================================================
// REFRESCAR TEXTOS DEL DASHBOARD (CORREGIDO A MILISEGUNDOS)
// =====================================================
void updateDashboardValues() {
  // Actualizar Estado
  if (estadoActual != estadoAnterior) {
    tft.setTextSize(2);
    uint16_t colorTxt = NEON_CYAN;
    if (estadoActual == "TAPANDO") colorTxt = NEON_ORANGE;
    if (estadoActual == "SALIENDO") colorTxt = NEON_GREEN;
    
    tft.fillRect(18, 80, 125, 25, PANEL_BG); 
    tft.setTextColor(colorTxt, PANEL_BG);
    tft.setCursor(20, 85);
    tft.print(estadoActual);
    estadoAnterior = estadoActual;
  }

  // Actualizar Contador
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

  // Actualizar T1 (Tiempo de Tapado en ms enteros)
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
    tft.print(tiempoTapado); // Muestra el valor entero directo del Nano
    tft.print(" ms");       // Cambiado de 's' a 'ms'
    tiempoTapadoAnterior = tiempoTapado;
  }

  // Actualizar T2 (Tiempo de Despacho en ms enteros)
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
    tft.print(tiempoDespacho); // Muestra el valor entero directo del Nano
    tft.print(" ms");         // Cambiado de 's' a 'ms'
    tiempoDespachoAnterior = tiempoDespacho;
    modoMenuAnterior = modoMenu;
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(9600); // Mismo baudaje que el Nano

  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486; 
  tft.begin(ID);
  tft.setRotation(1); 
  
  drawStaticBackground();
}

// =====================================================
// LOOP PRINCIPAL (Recepción Serial Parseada)
// =====================================================
void loop() {
  
  // Escuchar si el Nano mandó datos nuevos
  if (Serial.available() > 0) {
    String cadena = Serial.readStringUntil('\n');
    
    // Encontrar las posiciones de las comas para parsear
    int coma1 = cadena.indexOf(',');
    int coma2 = cadena.indexOf(',', coma1 + 1);
    int coma3 = cadena.indexOf(',', coma2 + 1);
    int coma4 = cadena.indexOf(',', coma3 + 1);

    if (coma1 > 0 && coma2 > 0 && coma3 > 0 && coma4 > 0) {
      // Extraer datos de la cadena
      estadoActual = cadena.substring(0, coma1);
      contador = cadena.substring(coma1 + 1, coma2).toInt();
      tiempoTapado = cadena.substring(coma2 + 1, coma3).toInt();
      tiempoDespacho = cadena.substring(coma3 + 1, coma4).toInt();
      modoMenu = cadena.substring(coma4 + 1).toInt();
      
      configInicialHecha = true;
    }
  }

  // Si no ha llegado ningún dato válido aún, no dibuja la máquina
  if (!configInicialHecha) return;

  // Actualizar textos de la pantalla
  updateDashboardValues();

  // =====================================================
  // GESTIÓN MÁQUINA VISUAL SEGÚN EL ESTADO DEL NANO
  // =====================================================
  if (modoMenu == 0) {
    
    if (estadoActual == "ESPERANDO") {
      tft.fillCircle(380, 75, 8, NEON_CYAN);
      drawPistonFisico(1, false); // Piston 1 Arriba (0)
      drawPistonFisico(2, true);  // Piston 2 Abajo (1)
      
      // Anima la cinta en espera
      static int f = 0;
      animateConveyor(f++);
      delay(5);
    }
    
   else if (estadoActual == "DETECTADO") {
      drawPistonFisico(1, true); // Piston 1 Abajo (1) inmediatamente
      
      // CORREGIDO: Arranca en 160 para no tocar el panel del dashboard
      for (botellaX = 160; botellaX < 250; botellaX += 5) {
        eraseBottle(botellaX - 5);
        drawBottle(botellaX);
        animateConveyor(botellaX);
      }
    }
    
    else if (estadoActual == "TAPANDO") {
      tft.fillCircle(380, 75, 8, NEON_ORANGE);
      
      // Animación de bajada del pistón tapador
      for (tapaY = 90; tapaY <= 175; tapaY += 5) {
        eraseCapAndPiston(259, tapaY - 5);
        drawCapAndPiston(259, tapaY);
      }
      
      // Se queda quieto retenido el tiempo exacto configurado (T1)
      delay(tiempoTapado); 
      
      // Animación de subida dejando la tapa pintada en la botella
      for (tapaY = 175; tapaY >= 90; tapaY -= 5) {
        eraseCapAndPiston(259, tapaY + 5);
        drawCapAndPiston(259, tapaY);
        tft.fillRoundRect(259, 175, 22, 10, 3, CAP_COLOR); 
      }
      eraseCapAndPiston(259, 90);
    }
    
    else if (estadoActual == "SALIENDO") {
      tft.fillCircle(380, 75, 8, NEON_GREEN);
      drawPistonFisico(2, false); // Abre salida (Piston 2 = 0)
      
      // Animación de despacho de la botella saliendo por la derecha
      for (int x = 250; x < 480; x += 5) {
        eraseBottle(x - 5);
        drawBottle(x);
        tft.fillRoundRect(x + 9, 175, 22, 10, 3, CAP_COLOR); 
        animateConveyor(x);
      }
      eraseBottle(480);
    }
  } 
  else {
    // Si estás en modo edición de menús, la luz piloto titila en rojo como advertencia
    tft.fillCircle(380, 75, 8, ALERT_RED);
    delay(200);
    tft.fillCircle(380, 75, 8, BLACK);
    delay(200);
  }
}
