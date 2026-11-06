// TFT Balanza - Arduino Uno + MCUFRIEND shield
// Recibe datos del Nano por Serial (9600 baud)
// Protocolo: P<peso>, a<memo1>, b<preMemo1>, V<pot>

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <SD.h>
#include <SPI.h>
#include "bitmap_functions_mcu.h"

#define SD_CS 10
MCUFRIEND_kbv tft;

// Colores
#define BLACK   0x0000
#define WHITE   0xFFFF
#define BLUE    0x001F
#define GREEN   0x07E0
#define YELLOW  0xFFE0
#define RED     0xF800

// Variables de estado
int memo1 = 300;
int memo2 = 200;       // preMemo1 (nombre histórico)
int pesoAnterior = -1;
uint16_t ultimoColor = 0xFFFF;  // último color del círculo

// Centro del círculo
int cx, cy;

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50);  // 🔴 CRÍTICO: evita bloqueos de 1s
  
  // Inicializar TFT
  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486;
  tft.begin(ID);
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  
  cx = tft.width() / 2;
  cy = tft.height() / 2;
  
  // Fondo dividido
  int mitad = tft.height() / 2;
  tft.fillRect(0, 0, tft.width(), mitad, BLACK);
  tft.fillRect(0, mitad, tft.width(), mitad, BLUE);
  tft.fillRoundRect(235, 0, 12, 320, 8, BLACK);
  tft.fillRoundRect(0, 155, 480, 12, 8, BLACK);
  
  // Texto estático "MAQUINAS GUERRA" centro
  tft.setTextSize(2);
  tft.setTextColor(YELLOW);
  tft.setCursor(140, 160);
  tft.print("MAQUINAS GUERRA");
  delay(1000);
  
  // Círculo central inicial (vacío, blanco)
  tft.fillRect(tft.width() - 120, tft.height() - 30, 120, 30, BLUE);
  tft.fillCircle(cx, cy, 135, BLACK);
  
  // Textos esquina inferior izquierda
  tft.setTextSize(2);
  tft.setTextColor(YELLOW);
  tft.setCursor(20, 260);
  tft.println("MAQUINAS");
  tft.setCursor(20, 280);
  tft.println(" GUERRA");
  delay(1000);
  
  // Etiquetas MEM y PRE (arriba izquierda)
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(10, 20);  tft.print("MEM:");
  tft.setCursor(10, 60);  tft.print("PRE:");
  
  // Inicializar SD y mostrar logo
  if (SD.begin(SD_CS)) {
    mostrarBMP(tft, "/LOGO.BMP", 390, 10);
  }
  
  // Dibujar círculo inicial blanco (vacío)
  tft.fillCircle(cx, cy, 120, WHITE);
  tft.drawCircle(cx, cy, 122, BLACK);
  ultimoColor = WHITE;
  
  // Mostrar "0" inicial
  mostrarTextoPeso("0");
}

void loop() {
  if (!Serial.available()) return;
  
  // 🟢 Buffer char en vez de String (evita fragmentación RAM)
  char buf[32];
  int n = Serial.readBytesUntil('\n', buf, sizeof(buf) - 1);
  if (n <= 0) return;
  buf[n] = 0;
  
  // Limpiar posibles \r finales
  if (buf[n-1] == '\r') buf[n-1] = 0;
  
  if (strlen(buf) < 2) return;
  
  char tipo = buf[0];
  int valor = atoi(buf + 1);  // 🟢 Más rápido que substring().toInt()
  
  switch (tipo) {
    case 'a':
      memo1 = valor;
      tft.fillRect(60, 20, 80, 20, BLACK);
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.setCursor(60, 20);
      tft.print(memo1);
      break;
      
    case 'b':
      memo2 = valor;
      tft.fillRect(60, 60, 80, 20, BLACK);
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.setCursor(60, 60);
      tft.print(memo2);
      break;
      
    case 'P':
      if (valor < 0) valor = 0;  // 🟢 Evita pesos negativos
      if (valor != pesoAnterior) {
        procesarPeso(valor);
        pesoAnterior = valor;
      }
      break;
      
    case 'V':
      mostrarPot(constrain(valor, 0, 126));
      break;
  }
}

void procesarPeso(int peso) {
  // Determinar color según rango
  uint16_t colorCir;
  if (peso < 30)        colorCir = WHITE;
  else if (peso <= memo1) colorCir = YELLOW;
  else if (peso <= memo2) colorCir = RED;
  else                    colorCir = GREEN;
  
  // 🟡 Solo redibujar el círculo si cambió el color (evita parpadeo)
  if (colorCir != ultimoColor) {
    tft.fillCircle(cx, cy, 120, colorCir);
    tft.drawCircle(cx, cy, 122, BLACK);
    ultimoColor = colorCir;
  }
  
  // Actualizar texto del peso (siempre que cambió el valor)
  char txt[8];
  sprintf(txt, "%d", peso);
  mostrarTextoPeso(txt);
}

void mostrarTextoPeso(const char* txt) {
  // Borrar solo la zona del texto anterior (no el círculo entero)
  tft.fillRect(cx - 80, cy - 50, 160, 80, ultimoColor);
  
  // Valor del peso (tamaño 6)
  tft.setTextColor(BLACK);
  tft.setTextSize(6);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w/2, cy - 40);
  tft.print(txt);
  
  // Unidad "GRS" (tamaño 3)
  tft.setTextSize(3);
  tft.getTextBounds("GRS", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w/2, cy + 15);
  tft.print("GRS");
}

void mostrarPot(int val) {
  // Limpiar zona POT
  tft.fillRect(tft.width() - 130, tft.height() - 60, 130, 60, BLUE);
  tft.setTextSize(3);
  tft.setTextColor(BLACK);
  tft.setCursor(tft.width() - 130, tft.height() - 50);
  tft.print("POT:");
  tft.print(val);
}