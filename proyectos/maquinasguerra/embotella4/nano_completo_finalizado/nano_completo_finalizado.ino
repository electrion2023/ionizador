#include <EEPROM.h>

#define CLK 2
#define DT 3
#define SW 4
#define BTN_START 11
#define BOMBA 5
#define PISTON2 6
#define CINTA 8
#define PICOS 7
#define ANTIGOTEO 9
#define IR_PIN 12

#define TIEMPO_ALINEACION 1000
#define TIEMPO_LEVANTE_PICOS 1000

long tiempo1, tiempo2, tiempoPicos = 1000, tiempox = 1000;
int cantidadMax = 4;
bool huboCambio = false;
int total;
int subtotal;
int contadorCiclos = 0;

int etapa = 0;
int contadorBotellas = 0;
bool sistemaActivo = false;
bool editando = false;
int variableSeleccionada = 0;
unsigned long etapaStartTime = 0;
volatile int lastClkState;
volatile int encoderPos = 0;
unsigned long lastButtonTime = 0;
int valorAntes;

void iniciarSecuencia();
void detenerSecuencia();
void ejecutarEtapas();
void leerEncoder();
void guardarVariablesEEPROM();
void cargarVariablesEEPROM();
void cambiarVariable();
void enviarEstadoSerial();

void setup() {
  Serial.begin(9600);

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(IR_PIN, INPUT_PULLUP);

  pinMode(BOMBA, OUTPUT);
  pinMode(PISTON2, OUTPUT);
  pinMode(CINTA, OUTPUT);
  pinMode(PICOS, OUTPUT);
  pinMode(ANTIGOTEO, OUTPUT);

  digitalWrite(BOMBA, HIGH);
  digitalWrite(PISTON2, HIGH);
  digitalWrite(CINTA, HIGH);
  digitalWrite(PICOS, HIGH);
  digitalWrite(ANTIGOTEO, HIGH);

  attachInterrupt(digitalPinToInterrupt(CLK), leerEncoder, CHANGE);
  cargarVariablesEEPROM();

  contadorCiclos = 0;
  valorAntes = tiempo1 + tiempo2;
  lastClkState = digitalRead(CLK);

  Serial.println("Sistema iniciado. Esperando Start...");
  enviarEstadoSerial();
}

void loop() {
  if (digitalRead(BTN_START) == LOW) {
    delay(200);
    sistemaActivo = !sistemaActivo;
    if (sistemaActivo) iniciarSecuencia();
    else detenerSecuencia();
    enviarEstadoSerial();
  }

  if (digitalRead(SW) == LOW) {
    if (millis() - lastButtonTime > 250) {
      Serial.println("click");
      guardarVariablesEEPROM();
      cambiarVariable();
      lastButtonTime = millis();
      enviarEstadoSerial();
    }
  }

  static int lastIR = HIGH;
  int currentIR = digitalRead(IR_PIN);

  if (currentIR == LOW && lastIR == HIGH && sistemaActivo && etapa == 1) {
    contadorBotellas++;
    delay(250);
    Serial.print("Botella detectada: ");
    Serial.println(contadorBotellas);
  }

  if (contadorBotellas >= cantidadMax && sistemaActivo && etapa == 1) {
    Serial.println("Cantidad alcanzada -> Etapa 2");
    etapa = 2;
    enviarEstadoSerial();
  }

  lastIR = currentIR;

  static unsigned long lastSerialSend = 0;
  if (millis() - lastSerialSend > 200) {
    enviarEstadoSerial();
    lastSerialSend = millis();
  }

  if (sistemaActivo) {
    ejecutarEtapas();
  }
}

void iniciarSecuencia() {
  etapa = 1;
  contadorBotellas = 0;
  Serial.println("Inicio de secuencia - ETAPA 1");
  digitalWrite(CINTA, LOW);
  digitalWrite(PISTON2, LOW);
  enviarEstadoSerial();
}

void detenerSecuencia() {
  sistemaActivo = false;
  etapa = 0;
  contadorBotellas = 0;
  Serial.println("Secuencia detenida");

  digitalWrite(BOMBA, HIGH);
  digitalWrite(PISTON2, HIGH);
  digitalWrite(CINTA, HIGH);
  digitalWrite(PICOS, HIGH);
  digitalWrite(ANTIGOTEO, HIGH);
  enviarEstadoSerial();
}

void ejecutarEtapas() {
  switch (etapa) {
    case 2:
      Serial.println("ETAPA 2 -> Alineando (Cinta sigue ON)...");
      etapaStartTime = millis();
      etapa = 3;
      enviarEstadoSerial();
      break;

    case 3:
      if (millis() - etapaStartTime >= TIEMPO_ALINEACION) {
        digitalWrite(CINTA, HIGH);
        digitalWrite(PICOS, LOW);
        digitalWrite(ANTIGOTEO, LOW);
        Serial.println("ETAPA 3 -> Alineación completa. Cinta OFF, Picos bajan.");
        etapa = 4;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 4:
      if (millis() - etapaStartTime >= 500) {
        digitalWrite(BOMBA, LOW);
        Serial.println("ETAPA 4 -> Bomba ON (Llenando)");
        contadorBotellas = 0;
        etapa = 5;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 5:
      if (millis() - etapaStartTime >= tiempo1) {
        digitalWrite(BOMBA, HIGH);
        Serial.println("ETAPA 5 -> Bomba OFF (Descompresión)");
        etapa = 6;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 6:
      if (millis() - etapaStartTime >= tiempo2) {
        digitalWrite(PICOS, HIGH);
        digitalWrite(ANTIGOTEO, LOW);
        Serial.println("ETAPA 6 -> Picos suben / Antigoteo ON");
        etapa = 7;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 7:
      if (millis() - etapaStartTime >= TIEMPO_LEVANTE_PICOS) {
        digitalWrite(CINTA, LOW);
        digitalWrite(PISTON2, HIGH);
        Serial.println("ETAPA 7 -> Cinta ON / Pistón delimitador OFF");
        etapa = 8;
        enviarEstadoSerial();
      }
      break;

    case 8:
      {
        static int lastIRReset = HIGH;
        int currentIR = digitalRead(IR_PIN);

        if (currentIR == LOW && lastIRReset == HIGH) {
          Serial.println("Primer nuevo envase detectado -> Reiniciando ciclo");
          digitalWrite(PISTON2, LOW);
          contadorBotellas = 1;
          contadorCiclos++;
          etapa = 1;
          enviarEstadoSerial();
        }

        lastIRReset = currentIR;
      }
      break;
  }
}

void leerEncoder() {
  int clkState = digitalRead(CLK);

  if (clkState != lastClkState) {
    int dtState = digitalRead(DT);
    if (dtState != clkState) {
      encoderPos++;
    } else {
      encoderPos--;
    }

    if (editando) {
      switch (variableSeleccionada) {
        case 0: tiempo1 = constrain(tiempo1 + encoderPos * 1000, 0, 99000L); break;
        case 1: tiempo2 = constrain(tiempo2 + encoderPos * 1000, 0, 99000L); break;
      }

      Serial.print("Cambio variable ");
      Serial.print(variableSeleccionada);
      Serial.print(" = ");
      Serial.println(variableSeleccionada == 0 ? tiempo1 : tiempo2);
      enviarEstadoSerial();
    }

    encoderPos = 0;
    lastClkState = clkState;
  }
}

void cambiarVariable() {
  variableSeleccionada++;
  if (variableSeleccionada > 1) variableSeleccionada = 0;
  editando = true;

  Serial.print("Editando variable ");
  switch (variableSeleccionada) {
    case 0: Serial.println("tiempo1"); break;
    case 1: Serial.println("tiempo2"); break;
  }
}

void cargarVariablesEEPROM() {
  EEPROM.get(0, tiempo1);
  EEPROM.get(4, tiempo2);

  if (tiempo1 <= 0 || tiempo1 > 99000L || tiempo2 <= 0 || tiempo2 > 99000L) {
    tiempo1 = 6000;
    tiempo2 = 6000;
    guardarVariablesEEPROM();
  }
}

void guardarVariablesEEPROM() {
  EEPROM.put(0, tiempo1);
  EEPROM.put(4, tiempo2);
  Serial.println("💾 Guardado en EEPROM");
}

void enviarEstadoSerial() {
  int tiempoRestante_ms = 0;
  unsigned long elapsedTime = 0;

  if (sistemaActivo && (etapa >= 4 && etapa <= 6)) {
    elapsedTime = millis() - etapaStartTime;
    long tiempoTotalEtapa = 0;

    if (etapa == 4) tiempoTotalEtapa = 500;
    else if (etapa == 5) tiempoTotalEtapa = tiempo1;
    else if (etapa == 6) tiempoTotalEtapa = tiempo2;

    tiempoRestante_ms = (tiempoTotalEtapa > (long)elapsedTime) ?
                        (tiempoTotalEtapa - elapsedTime) : 0;
  }

  int tiempoRestante_s = tiempoRestante_ms / 1000;

  total = tiempo1 + tiempo2 + contadorBotellas + contadorCiclos +
          variableSeleccionada + etapa + tiempoRestante_s;

  if (total != subtotal) {
    subtotal = total;

    Serial.print(tiempo1 / 1000);
    Serial.print(",");
    Serial.print(tiempo2 / 1000);
    Serial.print(",");
    Serial.print(contadorBotellas);
    Serial.print(",");
    Serial.print("0");
    Serial.print(",");
    Serial.print(contadorCiclos);
    Serial.print(",");
    Serial.print(variableSeleccionada);
    Serial.print(",");
    Serial.print(etapa);
    Serial.print(",");
    Serial.print(sistemaActivo ? 1 : 0);
    Serial.print(",");
    Serial.println(tiempoRestante_s);
  }
}