// balanza con todas las pantallas y bt. y boton de inicio de carga
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <Servo.h>
#include <SoftwareSerial.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SoftwareSerial Bt(8, 9); // D8 = RX, D9 = TX

// Pines
#define DT 2
#define SCK 3
#define pinPot A0
#define bs 6
#define bm 7
#define bn A2
#define comodin 4
#define servoPin 5
#define rele1 10
#define rele2 12
#define BUZZER_PIN 11
#define nuevoBoton A1
HX711 celda;
Servo myServo;

// Variables
int weight = 0;
int memo1 = 300;
int preMemo1 = 200;
int valorPot = 0;
int anguloMaximo = 0;
bool editandoMemo = true;
bool bsAnt = false;
bool comodinAnt = false;
int servoEstado = 0;
bool introMostrada = false;
bool estadoFinalizado = false;
bool pote=0;
bool botonInicio = false;

int barra;
int pesoAnterior;
#define RELE_ON  LOW
#define RELE_OFF HIGH
void setup() {
  Serial.begin(9600);
  Bt.begin(9600);      // Comunicación con HC-05

  pinMode(bs, INPUT_PULLUP);
  pinMode(bm, INPUT_PULLUP);
  pinMode(bn, INPUT); // y conectá una resistencia de 10k a GND
  pinMode(comodin, INPUT_PULLUP);
  pinMode(rele1, OUTPUT);
  pinMode(rele2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(rele1, RELE_OFF);
digitalWrite(rele2, RELE_OFF);
pinMode(nuevoBoton, INPUT_PULLUP);
  celda.begin(DT, SCK);
  celda.set_scale(442.0);
  celda.tare();

  myServo.attach(servoPin);
  myServo.write(0);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("No se pudo iniciar la pantalla"));
    while (true);
  }

  mostrarPantallaInicio();

  EEPROM.get(130, memo1);
  EEPROM.get(160, preMemo1);
   Serial.print("a");
    Serial.println(memo1);
    Serial.print("b");
    Serial.println(preMemo1); 
}


void loop() {
  leerPesoYPot();
  manejarServo();
  manejarBotones();
  mostrarPantalla();
  delay(50);
  //Serial2.println("2");
}

void leerPesoYPot() {
  if (celda.is_ready()) {
    weight = celda.get_units(2);
      
  }
  valorPot = analogRead(pinPot);
   
  anguloMaximo = map(valorPot, 0, 1023, 0, 180);

}

void manejarServo() {
  // Si ya se completó el llenado, mantener servo cerrado
  if (estadoFinalizado && weight > 40) {
    myServo.write(0);
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_OFF);
    return;
  }else {
  estadoFinalizado=false;
  }

  // Si peso muy bajo, reiniciar estado
  if (weight < 20 && estadoFinalizado ) {
    estadoFinalizado = false;
    servoEstado = 1;
    myServo.write(0);
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_OFF);
    tone(BUZZER_PIN, 400, 200);
    return;
  }

  // Control normal con peso ascendente
  

   if (digitalRead(nuevoBoton) == LOW) {
    botonInicio = false;
   // Serial.print("botonInicio");
   } else {
   botonInicio=true;
   }
 if(botonInicio == true){
  if (weight < preMemo1 && servoEstado != 2) {

    servoEstado = 2;
    //botonInicio = false;
    myServo.write(anguloMaximo);
    digitalWrite(rele1, RELE_ON);
    digitalWrite(rele2, RELE_ON);
    tone(BUZZER_PIN, 600, 200);
  } else if (weight >= preMemo1 && weight < memo1 && servoEstado != 3) {
    servoEstado = 3;
    int anguloIntermedio = 0;
    myServo.write(0);
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_ON);
    tone(BUZZER_PIN, 800, 200);
  } else if (weight >= memo1 && servoEstado != 4) {
    servoEstado = 4;
    myServo.write(0);
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_OFF);
    tone(BUZZER_PIN, 1000, 500);
    estadoFinalizado = true;  // Evitar que el servo vuelva a abrir si el peso baja
  }

  // Si está en apertura (estado 2 o 3), actualizar el ángulo en tiempo real
  if (servoEstado == 2) {
    myServo.write(anguloMaximo); // completamente abierto según potenciómetro
  }
  if (servoEstado == 3) {
    int anguloIntermedio =0;
    myServo.write(0);
  }
   
}
}

void manejarBotones() {
  // Selección Memo1 / PreMemo1
  bool bsNow = digitalRead(bs) == LOW;
  if (bsNow && !bsAnt) {
    editandoMemo = !editandoMemo;
    tone(BUZZER_PIN, 1200, 100);
  }
  bsAnt = bsNow;

  // Incremento / decremento
  if (digitalRead(bm) == HIGH) {
    if (editandoMemo) memo1=memo1+25;
    else preMemo1=preMemo1+25;  Serial.print("a");
    Serial.println(memo1);
    Serial.print("b");
    Serial.println(preMemo1);   
    delay(10);
  }
  if (digitalRead(bn) == HIGH) {
    if (editandoMemo) memo1=memo1-25;
    else preMemo1=preMemo1-25;
      Serial.print("a");
    Serial.println(memo1);
    Serial.print("b");
    Serial.println(preMemo1); 
    delay(5);
  }

  // Guardado en EEPROM solo al tocar el comodín (detección de flanco)
  bool comodinNow = digitalRead(comodin) == HIGH;
  if (comodinNow && !comodinAnt) {
    EEPROM.put(130, memo1);
    EEPROM.put(160, preMemo1);
    tone(BUZZER_PIN, 2000, 200);
  }
  comodinAnt = comodinNow;
}


void mostrarPantalla() {
  display.clearDisplay();

  int barWidth = map(valorPot, 0, 1023, 0, SCREEN_WIDTH - 2);
  display.fillRect(1, 1, barWidth, 8, WHITE);
  if (abs(barWidth - barra) >= 2) {
    Serial.print("V");
    Serial.println(barWidth);
    barra=barWidth;
    Bt.print(weight);Bt.print("_");
    Bt.println(anguloMaximo);

  }
  
  
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(90, 25);
  display.print("<< Grs");

  display.setTextSize(3);
  display.setCursor(4, 15);
  display.print(weight);
  Serial.print("P");
  Serial.println(weight);
 int peso = (int)weight;       // Solo la parte entera


   if(pesoAnterior != weight ){
    Bt.print(weight);
    Bt.print("_");// Bt.println("");
    Bt.println(anguloMaximo);
   
    pesoAnterior=weight;}




  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("Max:");
  display.setCursor(30, 55);
  display.print(memo1);
  display.setCursor(70, 55);
  display.print("Pre:");
  display.setCursor(100, 55);
  display.print(preMemo1);

  display.setCursor(editandoMemo ? 22 : 92, 55);
  display.print("<");

  display.display();
}

void mostrarPantallaInicio() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(" MAQUINAS");
  display.setCursor(10, 45);
  display.print(" GUERRA");
  display.display();
  delay(3000);
}
