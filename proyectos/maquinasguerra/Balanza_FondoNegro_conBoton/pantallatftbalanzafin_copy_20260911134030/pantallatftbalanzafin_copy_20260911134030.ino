#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <SD.h>
#include <SPI.h>

#include "bitmap_functions_mcu.h"


#define SD_CS 10                // Pin CS de la SD

MCUFRIEND_kbv tft;

#define BLACK   0x0000
#define WHITE   0xFFFF
#define CYAN    0x07FF
#define GRAY    0x8410
#define GREEN   0x07E0
#define YELLOW  0xFFE0
#define RED     0xF800
#define BLUE    0x001F

String ultimoPeso = "";
String ultimoPot  = "";
int pesoAnterior  = -1;
int memo1;
int memo2;
void setup() {
  Serial.begin(9600);

  // Inicializar TFT
  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486;
  tft.begin(ID);
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  // Fondo dividido
  int mitad = tft.height() / 2;
  tft.fillRect(0, 0, tft.width(), mitad, BLACK);
  tft.fillRect(0, mitad, tft.width(), mitad, BLUE);
  tft.fillRoundRect(235, 0, 12, 320, 8, BLACK);
  tft.fillRoundRect(0, 155, 480, 12, 8, BLACK);

  // Texto estático
  tft.setTextSize(2);
  tft.setTextColor(YELLOW);
  tft.setCursor(140, 160);
  tft.print("MAQUINAS GUERRA");
  delay(1500);

  tft.fillRect(tft.width() - 120, tft.height() - 30, 120, 30, BLUE);
  tft.fillCircle(240, 160, 135, BLACK);

//tft.fillCircle(240, 160, 135, BLACK);
  tft.setTextSize(2);
  tft.setTextColor(YELLOW);
  tft.setCursor(20, 260);
  tft.println("MAQUINAS");
  tft.setCursor(20, 280);
  tft.println(" GUERRA");
  delay(1500);

  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(10, 20);  tft.print("MEM: ");
  tft.setCursor(10, 60);  tft.print("PRE: ");

  // Inicializar SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicializar SD");
    // seguir con el resto aunque falle SD
  } else {
    Serial.println("SD inicializada OK");
    // Mostrar BMP en esquina superior derecha
    int bmpW = 100, bmpH = 100;
    int x = tft.width() - bmpW;
    int y = 0;
    mostrarBMP(tft, "/LOGO.BMP", 390, 10);  // Ajustá la posición


  }
}

void loop() {
  if (!Serial.available()) return;

  String linea = Serial.readStringUntil('\n');
  linea.trim();
  if (linea.length() < 2) return;

  char tipo   = linea.charAt(0);
  String valor = linea.substring(1);

  if (tipo == 'a') {
    memo1 = valor.toInt();
    tft.fillRect(60, 20, 50, 16, BLACK);  // limpiar antes
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.setCursor(60, 20);
    tft.print(memo1);
  }
  else if (tipo == 'b') {
    memo2 = valor.toInt();
    tft.fillRect(60, 60, 50, 16, BLACK);
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.setCursor(60, 60);
    tft.print(memo2);
  }
  else if (tipo == 'P' && valor != ultimoPeso) {
    int peso = valor.toInt();
    mostrarPeso(valor, peso);
    ultimoPeso = valor;
  }
  else if (tipo == 'V' && valor != ultimoPot) {
    int pot = constrain(valor.toInt(), 0, 126);
    mostrarPot(pot);
    ultimoPot = valor;
  }
}

void mostrarPeso(const String &valor, int peso) {
  int cx = tft.width() / 2;
  int cy = tft.height() / 2;

  uint16_t colorCir = (peso < 30 ? WHITE :
                      (peso <= memo1 ? YELLOW :
                      (peso <= memo2 ? RED : GREEN)));

  tft.fillCircle(cx, cy, 120, colorCir);
  tft.drawCircle(cx, cy, 122, BLACK);
  
  

  // Borrar zona del texto anterior (aprox)
  tft.fillRect(cx-70, cy-50, 140, 60, colorCir);

  // Imprimir valor
  tft.setTextColor(BLACK);
  tft.setTextSize(6);
  int16_t x1,y1; uint16_t w,h;
  tft.getTextBounds(valor, 0,0, &x1,&y1,&w,&h);
  tft.setCursor(cx - w/2, cy - 40);
  tft.print(valor);

  // Unidad "GRS"
  tft.setTextSize(3);
  tft.getTextBounds("GRS",0,0,&x1,&y1,&w,&h);
  tft.setCursor(cx - w/2, cy + 10);
  tft.print("GRS");
}

void mostrarPot(int val) {
  // Limpiar zona POT
  tft.fillRect(tft.width()-120, tft.height()-60, 120, 60, BLUE);
  tft.setTextSize(3);
  tft.setTextColor(BLACK);
  tft.setCursor(tft.width() - 140, tft.height() - 50);
  tft.print("POT:");
  tft.print(val);
}
