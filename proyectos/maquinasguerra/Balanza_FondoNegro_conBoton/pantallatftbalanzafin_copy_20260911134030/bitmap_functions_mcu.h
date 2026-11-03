#ifndef BITMAP_FUNCTIONS_H
#define BITMAP_FUNCTIONS_H

#include <SD.h>
#include <Adafruit_GFX.h>

template <typename TFT>
void mostrarBMP(TFT &tft, const char *filename, int x, int y) {
  File bmpFile;
  int bmpWidth, bmpHeight;
  uint8_t bmpDepth;
  uint32_t bmpImageoffset;
  uint32_t rowSize;
  uint8_t sdbuffer[3*20];
  uint8_t buffidx = sizeof(sdbuffer);
  boolean goodBmp = false;
  boolean flip = true;
  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t pos = 0;

  if ((x >= tft.width()) || (y >= tft.height())) return;

  bmpFile = SD.open(filename);
  if (!bmpFile) {
    Serial.print("No se pudo abrir: ");
    Serial.println(filename);
    return;
  }

  if (read16(bmpFile) == 0x4D42) {
    read32(bmpFile); // Tamaño archivo
    read32(bmpFile); // Reservado
    bmpImageoffset = read32(bmpFile);
    read32(bmpFile); // Tamaño del encabezado
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) {
      bmpDepth = read16(bmpFile);
      if (bmpDepth == 24 && read32(bmpFile) == 0) {
        goodBmp = true;
        rowSize = (bmpWidth * 3 + 3) & ~3;
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip = false;
        }

        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;

        for (row = 0; row < h; row++) {
          if (flip) pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else      pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos) {
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer);
          }
          for (col = 0; col < w; col++) {
            if (buffidx >= sizeof(sdbuffer)) {
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0;
            }
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.drawPixel(x + col, y + row, tft.color565(r, g, b));
          }
        }
      }
    }
  }

  bmpFile.close();
  if (!goodBmp) Serial.println("BMP no válido");
}

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}

#endif
