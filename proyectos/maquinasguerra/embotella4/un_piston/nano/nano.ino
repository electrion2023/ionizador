#include <EEPROM.h>

// --- Pines ---
/*
#define CLK 2
#define DT 3
#define SW 4
#define BTN_START 11
#define BOMBA 5
#define PISTON2 6       // Pistón delimitador
#define CINTA 8
#define PICOS 7
#define ANTIGOTEO 9
#define IR_PIN 12*/
#define CLK 2
#define DT 3
#define SW 9
#define BTN_START 11
#define BOMBA 5
#define PISTON2 6       // Pistón delimitador
#define CINTA 4
#define PICOS 7
#define ANTIGOTEO 13
#define IR_PIN 8

// --- Constantes de tiempo ---
#define TIEMPO_ALINEACION 1000  // 1 segundo para alinear
#define TIEMPO_LEVANTE_PICOS 1000 // 1 segundo espera que levanten picos

// --- Variables EEPROM ---
long tiempo1=5000, tiempo2=3000, tiempoPicos=100, tiempox=1000;
int cantidadMax = 4;
bool huboCambio = false;
int total;
int subtotal;
int contadorCiclos = 0;

// --- Variables de estado ---
int etapa = 0;
int contadorBotellas = 0;
bool sistemaActivo = false;
bool editando = false;
int variableSeleccionada = 0;
unsigned long etapaStartTime = 0;
volatile int lastClkState;
int encoderPos = 0;
unsigned long lastButtonTime = 0;
int valorAntes;

// --- Prototipos ---
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
  
  // --- Apagar todo al inicio (LÓGICA INVERTIDA: HIGH = APAGADO) ---
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
  // --- Botón Start/Stop ---
  if (digitalRead(BTN_START) == LOW) {
    delay(200);
    sistemaActivo = !sistemaActivo;
    if (sistemaActivo) iniciarSecuencia();
    else detenerSecuencia();
    enviarEstadoSerial();
  }
  
  // --- Botón del encoder ---
  if (digitalRead(SW) == LOW) {
    if (millis() - lastButtonTime > 250) {
      Serial.print("click");
      guardarVariablesEEPROM();
      cambiarVariable();
      lastButtonTime = millis();
      enviarEstadoSerial();
    }
  }
  
  // --- Contador IR (Solo funciona en Etapa 1) ---
  static int lastIR = HIGH;
  int currentIR = digitalRead(IR_PIN);
  
  if (currentIR == LOW && lastIR == HIGH && sistemaActivo && etapa == 1) {
    contadorBotellas++;
    delay(250); // Anti-rebote
    Serial.print("Botella detectada: ");
    Serial.println(contadorBotellas);
  }
  
  // Transición de Etapa 1 a Etapa 2
  if (contadorBotellas >= cantidadMax && sistemaActivo && etapa == 1) {
    Serial.println("Cantidad alcanzada -> Etapa 2");
    etapa = 2;
    etapaStartTime = millis();
    enviarEstadoSerial();
  }
  
  lastIR = currentIR;
  
  // --- Envío de estado periódico ---
  static unsigned long lastSerialSend = 0;
  if (millis() - lastSerialSend > 200) {
    enviarEstadoSerial();
    lastSerialSend = millis();
  }
  
  // --- Ejecutar etapas de la máquina ---
  if (sistemaActivo) {
    ejecutarEtapas();
  }
}

// ===================== FUNCIONES DE LA MÁQUINA =====================

void iniciarSecuencia() {
  etapa = 1;
  contadorBotellas = 0;
  Serial.println("Inicio de secuencia - ETAPA 1");
  // LÓGICA INVERTIDA: LOW = ENCENDIDO
  digitalWrite(CINTA, LOW);      // Cinta ON
  digitalWrite(PISTON2, LOW);    // Pistón delimitador ON (bajado)
  enviarEstadoSerial();
}

void detenerSecuencia() {
  sistemaActivo = false;
  etapa = 0;
  contadorBotellas = 0;
  Serial.println("Secuencia detenida");
  
  // Apagar todo (LÓGICA INVERTIDA: HIGH = APAGADO)
  digitalWrite(BOMBA, HIGH);
  digitalWrite(PISTON2, HIGH);
  digitalWrite(CINTA, HIGH);
  digitalWrite(PICOS, HIGH);
  digitalWrite(ANTIGOTEO, HIGH);
  enviarEstadoSerial();
}

void ejecutarEtapas() {
  switch (etapa) {
    case 2: // Espera para alinear (1 seg) y apaga cinta
      digitalWrite(CINTA, HIGH); // Cinta OFF
      Serial.println("ETAPA 2 -> Alineando...");
      etapa = 3;
      etapaStartTime = millis();
      enviarEstadoSerial();
      break;
      
    case 3: // Espera el tiempo de alineación, luego baja picos y activa antigoteo
      if (millis() - etapaStartTime >= TIEMPO_ALINEACION) {
        digitalWrite(PICOS, LOW);      // Picos ON (Bajados)
        digitalWrite(ANTIGOTEO, LOW);  // Antigoteo ON
        Serial.println("ETAPA 3 -> Picos bajan / Antigoteo ON");
        etapa = 4;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
      
    case 4: // Llenado (Bomba ON)
      // Espera un momento mínimo para que bajen los picos (opcional, 500ms)
      if (millis() - etapaStartTime >= 500) {
        digitalWrite(BOMBA, LOW); // Bomba ON
        Serial.println("ETAPA 4 -> Bomba ON (Llenando)");
        etapa = 5;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
      
    case 5: // Fin de llenado, espera tiempo1
      if (millis() - etapaStartTime >= tiempo1) {
        digitalWrite(BOMBA, HIGH); // Bomba OFF
        Serial.println("ETAPA 5 -> Bomba OFF (Descompresión)");
        etapa = 6;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
      
    case 6: // Fin de descompresión, espera tiempo2
      if (millis() - etapaStartTime >= tiempo2) {
        digitalWrite(PICOS, HIGH);     // Picos OFF (Suben)
        digitalWrite(ANTIGOTEO, LOW);  // Antigoteo ON (Se mantiene o activa para evitar goteo al subir)
        Serial.println("ETAPA 6 -> Picos suben / Antigoteo ON");
        etapa = 7;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
      
    case 7: // Espera 1 segundo a que levanten los picos
      if (millis() - etapaStartTime >= TIEMPO_LEVANTE_PICOS) {
        digitalWrite(CINTA, LOW);    // Cinta ON (para sacar bidones)
        digitalWrite(PISTON2, HIGH); // Pistón delimitador OFF (Levantado, deja pasar)
        Serial.println("ETAPA 7 -> Cinta ON / Pistón delimitador OFF");
        etapa = 8;
        // No reseteamos startTime aquí, lo haremos en la etapa 8 si es necesario, 
        // pero usaremos detección de flanco.
        enviarEstadoSerial();
      }
      break;
      
    case 8: // Espera detectar el PRIMER nuevo envase para reiniciar
      {
        static int lastIRReset = HIGH;
        int currentIR = digitalRead(IR_PIN);
        
        // Detecta cuando entra el primer nuevo envase (flanco de bajada)
        if (currentIR == LOW && lastIRReset == HIGH) {
          Serial.println("Primer nuevo envase detectado -> Reiniciando ciclo");
          digitalWrite(PISTON2, LOW); // Pistón delimitador ON (Baja para detener los nuevos)
          contadorBotellas = 0;       // Ya contamos 1, faltan 3 para llegar a 4
          contadorCiclos++;
          etapa = 1;                  // Vuelve a la etapa de conteo
          enviarEstadoSerial();
        }
        lastIRReset = currentIR;
      }
      break;
  }
}

// --- ENCODER ---
void leerEncoder() {
  int clkState = digitalRead(CLK);
  int dtState = digitalRead(DT);
  
  if (clkState != lastClkState) {
    if (dtState != clkState) encoderPos++;
    else encoderPos--;
    
    if (editando) {
      switch (variableSeleccionada) {
        case 0:  tiempo1 = constrain(tiempo1 + encoderPos * 1000, 0, 99000L); break;
        case 1:  tiempo2 = constrain(tiempo2 + encoderPos * 1000, 0, 99000L); break;
      }
      
      Serial.print("Cambio variable ");
      Serial.print(variableSeleccionada);
      Serial.print(" = ");
      Serial.println(variableSeleccionada == 0 ? tiempo1 : tiempo2);
      encoderPos = 0;
      enviarEstadoSerial();
    }
  }
  lastClkState = clkState;
}

// --- CAMBIAR VARIABLE A EDITAR ---
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

// --- EEPROM ---
void cargarVariablesEEPROM() {
  EEPROM.get(0, tiempo1);
  EEPROM.get(4, tiempo2);
  
  if (tiempo1 == 0xFFFF) { // nunca grabado
    tiempo1 = tiempo2 = 6000;
    guardarVariablesEEPROM();
  }
}

void guardarVariablesEEPROM() {
  EEPROM.put(0, tiempo1);
  EEPROM.put(4, tiempo2);
  Serial.println("💾 Guardado en EEPROM");
}

// ===================== COMUNICACIÓN SERIAL =====================
void enviarEstadoSerial() {
  int tiempoRestante_ms = 0;
  unsigned long elapsedTime = 0;
  
  if (sistemaActivo && (etapa >= 4 && etapa <= 6)) {
    elapsedTime = millis() - etapaStartTime;
    long tiempoTotalEtapa = 0;
    
    if (etapa == 4) tiempoTotalEtapa = 500; // Tiempo mínimo bajada picos
    else if (etapa == 5) tiempoTotalEtapa = tiempo1;
    else if (etapa == 6) tiempoTotalEtapa = tiempo2;
    
    tiempoRestante_ms = (tiempoTotalEtapa > elapsedTime) ?
                        (tiempoTotalEtapa - elapsedTime) : 0;
  }
  
  int tiempoRestante_s = tiempoRestante_ms / 1000;
  
  total = tiempo1 + tiempo2 + contadorBotellas + contadorCiclos + 
          variableSeleccionada + etapa + tiempoRestante_s;
          
  if (total != subtotal) {
    subtotal = total;
    
    Serial.print(tiempo1/1000);
    Serial.print(",");
    Serial.print(tiempo2/1000);
    Serial.print(",");
    Serial.print(contadorBotellas);
    Serial.print(",");
    Serial.print("0"); // contadorSalida eliminado
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