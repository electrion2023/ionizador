// Balanza Nano - Control de llenado con 2 relés
// Comunicación Serial con Uno/TFT
#include <EEPROM.h>
#include <HX711.h>

// Pines
#define DT 2
#define SCK 3
#define bs 6
#define bm 7
#define bn A2
#define comodin 4
#define rele1 10
#define rele2 12
#define BUZZER_PIN 8
#define nuevoBoton 9  // ← cambiado de A1 a D9

HX711 celda;

// Variables
int weight = 0;
int memo1 = 300;
int preMemo1 = 200;
bool editandoMemo = true;
bool bsAnt = false;
bool comodinAnt = false;
bool nuevoBotonAnt = HIGH;  // ← para detección de flanco
int servoEstado = 0;
bool cicloActivo = false;   // ← reemplaza botonInicio
bool estadoFinalizado = false;
int pesoAnterior = -1;

// 🔴 CORREGIDO: lógica de relés invertida
#define RELE_ON  HIGH
#define RELE_OFF LOW

void setup() {
  Serial.begin(9600);
  
  // Pines de botones (todos INPUT_PULLUP)
  pinMode(bs, INPUT_PULLUP);
  pinMode(bm, INPUT_PULLUP);
  pinMode(bn, INPUT_PULLUP);
  pinMode(comodin, INPUT_PULLUP);
  pinMode(nuevoBoton, INPUT_PULLUP);
  
  // Pines de salida
  pinMode(rele1, OUTPUT);
  pinMode(rele2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // 🔴 CORREGIDO: relés OFF al inicio (LOW = apagado)
  digitalWrite(rele1, RELE_OFF);
  digitalWrite(rele2, RELE_OFF);
  
  // Inicializar celda de carga
  celda.begin(DT, SCK);
  celda.set_scale(442.0);
  celda.tare();
  
  // Cargar valores de EEPROM
  EEPROM.get(130, memo1);
  EEPROM.get(160, preMemo1);
  
  // Validar valores cargados
  if (memo1 < 50 || memo1 > 5000) memo1 = 300;
  if (preMemo1 < 50 || preMemo1 > 5000) preMemo1 = 200;
  
  // Enviar valores iniciales al Uno
  enviarDato('a', memo1);
  enviarDato('b', preMemo1);
  
  // Beep de inicio
  tone(BUZZER_PIN, 1000, 200);
}

void loop() {
  leerPeso();
  manejarCiclo();
  manejarBotones();
  delay(50);
}

void leerPeso() {
  if (celda.is_ready()) {
    weight = celda.get_units(2);
  }
  
  // Enviar peso si cambió
  if (weight != pesoAnterior) {
    enviarDato('P', weight);
    pesoAnterior = weight;
  }
}

void manejarCiclo() {
  // 🔴 CORREGIDO: detección de flanco para arrancar el ciclo
  bool nuevoBotonNow = digitalRead(nuevoBoton);
  if (nuevoBotonAnt == HIGH && nuevoBotonNow == LOW) {
    // Flanco descendente: botón presionado
    cicloActivo = true;
    servoEstado = 1;
    estadoFinalizado = false;
    Serial.print("BotIni");
    tone(BUZZER_PIN, 500, 100);
  }
  nuevoBotonAnt = nuevoBotonNow;
  
  // Si el ciclo no está activo, mantener todo apagado
  if (!cicloActivo) {
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_OFF);
    return;
  }
  
  // Si ya se completó el llenado, mantener relés cerrados
  if (estadoFinalizado && weight > 40) {
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_OFF);
    return;
  }
  
  // Si se retiró el recipiente (peso bajo), reiniciar ciclo
  if (weight < 20 && estadoFinalizado) {
    cicloActivo = false;
    estadoFinalizado = false;
    servoEstado = 1;
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_OFF);
    tone(BUZZER_PIN, 400, 200);
    return;
  }
  
  // Control normal con peso ascendente (solo si cicloActivo == true)
  // Estado 2: llenado rápido (ambos relés ON)
  if (weight < preMemo1 && servoEstado != 2) {
    servoEstado = 2;
    digitalWrite(rele1, RELE_ON);
    digitalWrite(rele2, RELE_ON);
    tone(BUZZER_PIN, 600, 200);
  }
  // Estado 3: goteo final (solo rele2 ON)
  else if (weight >= preMemo1 && weight < memo1 && servoEstado != 3) {
    servoEstado = 3;
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_ON);
    tone(BUZZER_PIN, 800, 200);
  }
  // Estado 4: llenado completo (ambos relés OFF)
  else if (weight >= memo1 && servoEstado != 4) {
    servoEstado = 4;
    digitalWrite(rele1, RELE_OFF);
    digitalWrite(rele2, RELE_OFF);
    tone(BUZZER_PIN, 1000, 500);
    estadoFinalizado = true;
  }
}

void manejarBotones() {
  // Selección Memo1 / PreMemo1
  bool bsNow = digitalRead(bs) == HIGH;
  if (bsNow && !bsAnt) {
    editandoMemo = !editandoMemo;
    tone(BUZZER_PIN, 1200, 100);
  }
  bsAnt = bsNow;
  
  // Incremento (botón bm)
  if (digitalRead(bm) == HIGH) {
    if (editandoMemo) {
      memo1 = memo1 + 25;
      if (memo1 > 5000) memo1 = 5000;
    } else {
      preMemo1 = preMemo1 + 25;
      if (preMemo1 > memo1) preMemo1 = memo1;
    }
    enviarDato('a', memo1);
    enviarDato('b', preMemo1);
    delay(150);
  }
  
  // Decremento (botón bn)
  if (digitalRead(bn) == HIGH) {
    if (editandoMemo) {
      memo1 = memo1 - 25;
      if (memo1 < 50) memo1 = 50;
    } else {
      preMemo1 = preMemo1 - 25;
      if (preMemo1 < 50) preMemo1 = 50;
    }
    enviarDato('a', memo1);
    enviarDato('b', preMemo1);
    delay(150);
  }
  
  // Guardado en EEPROM (botón comodín)
  bool comodinNow = digitalRead(comodin) == HIGH;
  if (comodinNow && !comodinAnt) {
    EEPROM.put(130, memo1);
    EEPROM.put(160, preMemo1);
    Serial.print("comodin:guarda");
    tone(BUZZER_PIN, 2000, 200);
  }
  comodinAnt = comodinNow;
}

void enviarDato(char tipo, int valor) {
  Serial.print(tipo);
  Serial.println(valor);
}