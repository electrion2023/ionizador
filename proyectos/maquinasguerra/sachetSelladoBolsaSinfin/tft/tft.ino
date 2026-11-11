#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;

// --- PALETA DE COLORES INDUSTRIAL (Alto Contraste) ---
#define BG_MAIN       0x1082  
#define BG_PANEL      0x18E3  
#define BG_CARD       0x2104  
#define BORDER_COLOR  0x39E7  
#define TEXT_MAIN     0xFFFF  
#define TEXT_DIM      0x8410  
#define ACCENT_CYAN   0x07FF  
#define ACCENT_GREEN  0x07E0  
#define ACCENT_RED    0xF800  
#define ACCENT_ORANGE 0xFD20  
#define LIQUID_COLOR  0x051D  
#define METAL_LIGHT   0xBDF7  
#define METAL_DARK    0x4208  

// --- VARIABLES DE DATOS (Recibidas del Nano) ---
int t1 = 0, t2 = 0, t3 = 0, estado = 0, paramSel = 0;
String datosRecibidos = "";

// --- VARIABLES DE ANIMACIÓN ---
int liquidLevel = 0;       
int jawOffset = 0;         
int sachetY = 0;           
bool sachetVisible = false;
unsigned long lastAnimTime = 0;

// --- VARIABLES PARA EVITAR PARPADEO ---
int old_t1 = -1, old_t2 = -1, old_t3 = -1, old_estado = -1, old_paramSel = -1;
int old_liquidLevel = -1, old_jawOffset = -1, old_sachetY = -1;
bool old_sachetVisible = false;

void setup() {
  Serial.begin(9600);
  
  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486; 
  tft.begin(ID);
  tft.setRotation(1); 

  drawStaticUI();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      procesarDatos(datosRecibidos);
      datosRecibidos = "";
    } else {
      datosRecibidos += c;
    }
  }
  
  actualizarPanelDatos();
  animarProceso();
}

// ==========================================================
// DIBUJO ESTÁTICO
// ==========================================================
void drawStaticUI() {
  tft.fillScreen(BG_MAIN);

  tft.fillRect(0, 0, 480, 35, BG_PANEL);
  tft.drawFastHLine(0, 35, 480, ACCENT_CYAN);
  tft.setTextColor(ACCENT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(15, 8);
  tft.print("SACHET PACKER PRO");

  tft.fillRoundRect(10, 45, 140, 265, 8, BG_PANEL);
  tft.drawRoundRect(10, 45, 140, 265, 8, BORDER_COLOR);
  
  tft.setTextColor(TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(20, 55); tft.print("PARAMETROS DE TIEMPO");
  tft.drawFastHLine(20, 68, 120, BORDER_COLOR);

  drawDataCard(20, 80, "T1 SELLADO", 0);
  drawDataCard(20, 140, "T2 LLENADO", 1);
  drawDataCard(20, 200, "T3 CICLO", 2);

  tft.drawFastHLine(20, 260, 120, BORDER_COLOR);
  tft.setTextColor(TEXT_DIM);
  tft.setCursor(20, 270); tft.print("ESTADO DEL SISTEMA");
  tft.fillRoundRect(20, 280, 120, 20, 4, BG_CARD);

  tft.fillRoundRect(160, 45, 310, 265, 8, BG_PANEL);
  tft.drawRoundRect(160, 45, 310, 265, 8, BORDER_COLOR);

  tft.setTextColor(TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(280, 55); tft.print("VISTA DE PROCESO");
  tft.drawFastHLine(170, 68, 290, BORDER_COLOR);

  tft.fillCircle(315, 90, 20, METAL_DARK);
  tft.drawCircle(315, 90, 20, METAL_LIGHT);
  tft.fillCircle(315, 90, 6, BG_PANEL);
  
  tft.fillRect(305, 110, 20, 110, METAL_DARK);
  tft.drawRect(305, 110, 20, 110, METAL_LIGHT);

  tft.fillRect(260, 220, 110, 8, METAL_DARK);
  tft.drawRect(260, 220, 110, 8, METAL_LIGHT);

  tft.fillRect(250, 270, 130, 12, METAL_DARK);
  tft.drawRect(250, 270, 130, 12, METAL_LIGHT);
  tft.fillCircle(260, 276, 5, METAL_LIGHT);
  tft.fillCircle(370, 276, 5, METAL_LIGHT);
}

void drawDataCard(int x, int y, String label, int id) {
  tft.fillRoundRect(x, y, 120, 50, 6, BG_CARD);
  tft.drawRoundRect(x, y, 120, 50, 6, BORDER_COLOR);
  
  tft.setTextColor(TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(x + 10, y + 10);
  tft.print(label);
  
  tft.setTextColor(TEXT_MAIN);
  tft.setTextSize(2);
  tft.setCursor(x + 10, y + 25);
  tft.print("0000"); 
  tft.setTextSize(1);
  tft.print(" ms");
}

// ==========================================================
// PROCESAMIENTO DE DATOS
// ==========================================================
void procesarDatos(String datos) {
  int i1 = datos.indexOf(':');
  int i2 = datos.indexOf(',', i1);
  int new_t1 = datos.substring(i1 + 1, i2).toInt();

  i1 = datos.indexOf(':', i2);
  i2 = datos.indexOf(',', i1);
  int new_t2 = datos.substring(i1 + 1, i2).toInt();

  i1 = datos.indexOf(':', i2);
  i2 = datos.indexOf(',', i1);
  int new_t3 = datos.substring(i1 + 1, i2).toInt();

  i1 = datos.indexOf(':', i2);
  i2 = datos.indexOf(',', i1);
  int new_estado = datos.substring(i1 + 1, i2).toInt();

  i1 = datos.indexOf(':', i2);
  int new_paramSel = datos.substring(i1 + 1).toInt();

  if (new_t1 != t1 || new_t2 != t2 || new_t3 != t3 || new_estado != estado || new_paramSel != paramSel) {
    t1 = new_t1; t2 = new_t2; t3 = new_t3;
    estado = new_estado;
    paramSel = new_paramSel;
  }
}

// ==========================================================
// ACTUALIZACIÓN DE PANEL DE DATOS (CORREGIDO)
// ==========================================================
void actualizarPanelDatos() {
  // 1. Detectar si cambió la selección ANTES de dibujar
  bool selectionChanged = (paramSel != old_paramSel);

  // 2. Actualizar tarjetas pasando el estado de la selección
  updateDataCard(20, 80, t1, 0, selectionChanged);
  updateDataCard(20, 140, t2, 1, selectionChanged);
  updateDataCard(20, 200, t3, 2, selectionChanged);

  // 3. Actualizar Estado del sistema
  if (estado != old_estado) {
    tft.fillRoundRect(25, 282, 110, 16, 4, BG_CARD); 
    tft.setCursor(30, 284);
    tft.setTextSize(1);
    
    switch(estado) {
      case 0: tft.setTextColor(TEXT_DIM); tft.print("EN ESPERA"); break;
      case 1: tft.setTextColor(ACCENT_CYAN); tft.print(">> LLENANDO"); break;
      case 2: tft.setTextColor(ACCENT_RED); tft.print(">> SELLANDO"); break;
      case 3: tft.setTextColor(ACCENT_GREEN); tft.print(">> DESCARGA"); break;
    }
    old_estado = estado;
  }

  // 4. Actualizar la variable de control SOLO AL FINAL
  old_paramSel = paramSel; 
}

void updateDataCard(int x, int y, int valor, int id, bool selectionChanged) {
  bool isSel = (paramSel == id);
  bool valChanged = (id == 0 && t1 != old_t1) || (id == 1 && t2 != old_t2) || (id == 2 && t3 != old_t3);

  // Solo redibujar si cambió el valor O si cambió la selección del encoder
  if (valChanged || selectionChanged) {
    
    // Redibujar borde (Verde si está seleccionado, Azul si no)
    tft.drawRoundRect(x, y, 120, 50, 6, isSel ? ACCENT_GREEN : BORDER_COLOR);
    
    // Limpiar y escribir el valor
    tft.fillRect(x + 10, y + 25, 80, 20, BG_CARD);
    tft.setTextColor(isSel ? ACCENT_GREEN : TEXT_MAIN);
    tft.setTextSize(2);
    tft.setCursor(x + 10, y + 25);
    tft.print(valor);
    tft.setTextSize(1);
    tft.print(" ms");

    // Guardar el valor actual para la próxima comparación
    if(id == 0) old_t1 = valor;
    if(id == 1) old_t2 = valor;
    if(id == 2) old_t3 = valor;
  }
}

// ==========================================================
// ANIMACIÓN DEL PROCESO
// ==========================================================
void animarProceso() {
  if (millis() - lastAnimTime > 40) { 
    lastAnimTime = millis();
    processAnimationLogic();
  }

  if (liquidLevel != old_liquidLevel) {
    if (old_liquidLevel > 0) {
      int h = map(old_liquidLevel, 0, 100, 0, 100);
      tft.fillRect(307, 220 - h, 16, h, BG_PANEL); 
    }
    if (liquidLevel > 0) {
      int h = map(liquidLevel, 0, 100, 0, 100);
      tft.fillRect(307, 220 - h, 16, h, LIQUID_COLOR);
    }
    old_liquidLevel = liquidLevel;
  }

  if (jawOffset != old_jawOffset) {
    tft.fillRect(270, 220, 30, 20, BG_PANEL); 
    tft.fillRect(330, 220, 30, 20, BG_PANEL); 
    
    uint16_t color = (jawOffset > 10) ? ACCENT_RED : METAL_LIGHT;
    tft.fillRoundRect(270 + jawOffset, 220, 30, 20, 3, color);
    tft.fillRoundRect(330 - jawOffset, 220, 30, 20, 3, color);
    
    old_jawOffset = jawOffset;
  }

  if (sachetY != old_sachetY || sachetVisible != old_sachetVisible) {
    if (old_sachetVisible) {
      tft.fillRoundRect(300, old_sachetY, 30, 40, 4, BG_PANEL);
    }
    
    if (sachetVisible) {
      tft.fillRoundRect(300, sachetY, 30, 40, 4, ACCENT_CYAN);
      tft.drawRoundRect(300, sachetY, 30, 40, 4, TEXT_MAIN);
      tft.drawFastHLine(305, sachetY + 10, 20, TEXT_MAIN); 
    }
    
    old_sachetY = sachetY;
    old_sachetVisible = sachetVisible;
  }
}

void processAnimationLogic() {
  static int lastEstado = -1;
  
  if (estado != lastEstado) {
    if (estado == 1) { liquidLevel = 0; jawOffset = 0; sachetVisible = false; }
    if (estado == 2) { liquidLevel = 100; jawOffset = 0; sachetVisible = false; }
    if (estado == 3) { 
      liquidLevel = 0; 
      jawOffset = 20; 
      sachetY = 220; 
      sachetVisible = true; 
    }
    if (estado == 0) { liquidLevel = 0; jawOffset = 0; sachetVisible = false; }
    lastEstado = estado;
  }

  switch(estado) {
    case 1: 
      if (liquidLevel < 100) liquidLevel += 5;
      break;
    case 2: 
      if (jawOffset < 20) jawOffset += 3;
      break;
    case 3: 
      if (sachetVisible) {
        if (sachetY < 240) {
          sachetY += 4; 
        } else {
          sachetVisible = false; 
        }
      }
      if (jawOffset > 0) jawOffset -= 2;
      break;
  }
}