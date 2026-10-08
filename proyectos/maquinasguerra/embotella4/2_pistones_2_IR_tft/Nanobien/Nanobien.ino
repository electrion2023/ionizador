#include <EEPROM.h>

// --- Pines ---
#define CLK 3
#define DT 2
#define SW 4
#define BTN_START 11
#define BOMBA 8
#define PISTON1 13
#define PISTON2 5
#define CINTA 7
#define PICOS 9
#define ANTIGOTEO 12

#define IR_PIN 6
#define IR_SALIDA 10

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// --- Variables EEPROM ---
long tiempo1, tiempo2, tiempoPicos = 1000, tiempox = 1000;
int cantidadMax = 4;

// --- NUEVOS TIEMPOS DE AJUSTE MECÁNICO (en milisegundos) ---
#define TIEMPO_ALINEACION 2000       // 2 segundos de cinta para alinear bidones con picos
#define TIEMPO_LEVANTE_PICOS 1500    // 1.5 segundos para que los picos se levanten antes de mover la cinta

// --- Variables de estado ---
int etapa = 0;
int contadorBotellas = 0;
int contadorSalida = 0;
int contadorCiclos = 0;
bool sistemaActivo = false;
bool editando = false;
int variableSeleccionada = 0;

unsigned long etapaStartTime = 0;

// --- DEBOUNCE ---
unsigned long lastButtonStartTime = 0;
unsigned long lastEncoderButtonTime = 0;
unsigned long lastSerialSend = 0;

// --- DETECCIÓN DE FLANCO + FILTRO TEMPORAL ---
int lastIREntrada = HIGH;
int lastIRSalida = HIGH;
unsigned long ultimaDeteccionEntrada = 0;
unsigned long ultimaDeteccionSalida = 0;

#define TIEMPO_MINIMO_ENTRADA 800
#define TIEMPO_MINIMO_SALIDA 800

// --- Variables del Encoder ---
volatile int lastClkState;
volatile int encoderPos = 0;
volatile bool encoderChanged = false;

// --- Prototipos ---
void iniciarSecuencia();
void detenerSecuencia();
void ejecutarEtapas();
void leerEncoder();
void guardarVariablesEEPROM();
void cargarVariablesEEPROM();
void cambiarVariable();
void enviarEstadoSerial();
bool leerBotonEstable(int pin);

void setup() {
  Serial.begin(9600);

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(IR_SALIDA, INPUT_PULLUP);

  pinMode(BOMBA, OUTPUT);
  pinMode(PISTON1, OUTPUT);
  pinMode(PISTON2, OUTPUT);
  pinMode(CINTA, OUTPUT);
  pinMode(PICOS, OUTPUT);
  pinMode(ANTIGOTEO, OUTPUT);

  digitalWrite(BOMBA, RELAY_OFF);
  digitalWrite(PISTON1, RELAY_OFF);
  digitalWrite(PISTON2, RELAY_OFF);
  digitalWrite(CINTA, RELAY_OFF);
  digitalWrite(PICOS, RELAY_OFF);
  digitalWrite(ANTIGOTEO, RELAY_OFF);

  attachInterrupt(digitalPinToInterrupt(CLK), leerEncoder, CHANGE);

  cargarVariablesEEPROM();
  lastClkState = digitalRead(CLK);

  lastIREntrada = digitalRead(IR_PIN);
  lastIRSalida = digitalRead(IR_SALIDA);

  Serial.println("Sistema iniciado. Esperando Start...");
  enviarEstadoSerial();
}

void loop() {
  // --- 1. Botón Start/Stop ---
  if (leerBotonEstable(BTN_START)) {
    if (millis() - lastButtonStartTime > 500) {
      sistemaActivo = !sistemaActivo;
      if (sistemaActivo) iniciarSecuencia();
      else detenerSecuencia();
      
      lastButtonStartTime = millis();
      enviarEstadoSerial();
    }
  }

  // --- 2. Botón del Encoder ---
  if (leerBotonEstable(SW)) {
    if (millis() - lastEncoderButtonTime > 400) {
      cambiarVariable();
      lastEncoderButtonTime = millis();
    }
  }

  // --- 3. Procesar cambios del encoder ---
  if (encoderChanged && editando) {
    noInterrupts();
    int pos = encoderPos;
    encoderPos = 0;
    encoderChanged = false;
    interrupts();

    if (pos != 0) {
      if (variableSeleccionada == 0) {
        tiempo1 = constrain(tiempo1 + pos * 500, 0, 500000L);
      } else if (variableSeleccionada == 1) {
        tiempo2 = constrain(tiempo2 + pos * 500, 0, 500000L);
      }
      enviarEstadoSerial();
    }
  }

  // --- 4. Contador IR ENTRADA ---
  int currentIR = digitalRead(IR_PIN);
  
  if (currentIR == LOW && lastIREntrada == HIGH && 
      sistemaActivo && etapa == 1 && 
      (millis() - ultimaDeteccionEntrada > TIEMPO_MINIMO_ENTRADA)) {
    contadorBotellas++;
    ultimaDeteccionEntrada = millis();
    Serial.print("Botella entrada detectada: ");
    Serial.println(contadorBotellas);
    enviarEstadoSerial();
  }
  lastIREntrada = currentIR;

  // Transición Etapa 1 -> 2 (cuando se alcanza el máximo y el sensor se libera)
  if (contadorBotellas >= cantidadMax && currentIR == HIGH && sistemaActivo && etapa == 1) {
    contadorBotellas = 0;
    digitalWrite(PISTON1, RELAY_ON);
    etapa = 2;
    etapaStartTime = millis();
    Serial.println("Cantidad alcanzada -> Etapa 2 (alineando bidones)");
    enviarEstadoSerial();
  }

  // --- 5. Ejecutar máquina de estados ---
  if (sistemaActivo) {
    ejecutarEtapas();
  }

  // --- 6. Envío periódico ---
  if (millis() - lastSerialSend > 300) {
    enviarEstadoSerial();
    lastSerialSend = millis();
  }
}

// ===================== FUNCIONES =====================

bool leerBotonEstable(int pin) {
  int lecturas = 0;
  for (int i = 0; i < 3; i++) {
    if (digitalRead(pin) == LOW) {
      lecturas++;
    }
    delay(5);
  }
  return (lecturas == 3);
}

void iniciarSecuencia() {
  etapa = 1;
  contadorBotellas = 0;
  digitalWrite(CINTA, RELAY_ON);
  digitalWrite(PISTON2, RELAY_ON);
  Serial.println("Inicio de secuencia - ETAPA 1");
}

void detenerSecuencia() {
  sistemaActivo = false;
  etapa = 0;
  contadorBotellas = 0;
  
  digitalWrite(BOMBA, RELAY_OFF);
  digitalWrite(PISTON1, RELAY_OFF);
  digitalWrite(PISTON2, RELAY_OFF);
  digitalWrite(CINTA, RELAY_OFF);
  digitalWrite(PICOS, RELAY_OFF);
  digitalWrite(ANTIGOTEO, RELAY_OFF);
  
  Serial.println("Secuencia detenida");
}

void ejecutarEtapas() {
  switch (etapa) {
    
    case 2:
      // ⭐ NUEVO: La cinta SIGUE ANDANDO durante TIEMPO_ALINEACION para alinear bidones
      // (La cinta ya estaba ON desde la etapa 1, se mantiene ON)
      
      if (millis() - etapaStartTime >= TIEMPO_ALINEACION) {
        // Después del tiempo de alineación, recién ahora se detiene la cinta
        digitalWrite(CINTA, RELAY_OFF);
        digitalWrite(ANTIGOTEO, RELAY_ON);
        
        Serial.println("Alineacion completada -> Cinta OFF, Etapa 3");
        etapa = 3;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 3:
      if (millis() - etapaStartTime >= tiempox) {
        digitalWrite(PICOS, RELAY_ON);
        etapa = 4;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 4:
      if (millis() - etapaStartTime >= tiempoPicos) {
        digitalWrite(BOMBA, RELAY_ON);
        etapa = 5;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 5:
      if (millis() - etapaStartTime >= (tiempo1)) {
        digitalWrite(BOMBA, RELAY_OFF);
        etapa = 6;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 6:
      if (millis() - etapaStartTime >= (tiempo2)) {
        // Apagar picos y antigoteo
        digitalWrite(PICOS, RELAY_OFF);
        digitalWrite(ANTIGOTEO, RELAY_OFF);
        digitalWrite(PISTON2, RELAY_OFF);
        
        Serial.println("Picos bajando -> Esperando levante completo");
        etapa = 65;  // ⭐ NUEVA SUB-ETAPA: Espera a que los picos se levanten
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    // ⭐ NUEVA SUB-ETAPA 6.5: Espera a que los picos se levanten antes de mover la cinta
    case 65:  // Usamos 65 porque switch no soporta decimales
      if (millis() - etapaStartTime >= TIEMPO_LEVANTE_PICOS) {
        Serial.println("Picos levantados -> Etapa 7 (salida)");
        etapa = 7;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;

    case 7:
      digitalWrite(CINTA, RELAY_ON);
      digitalWrite(PISTON1, RELAY_ON);
      
      // Contador de botellas de salida
      int currentIRSalida = digitalRead(IR_SALIDA);
      
      if (currentIRSalida == LOW && lastIRSalida == HIGH && 
          (millis() - ultimaDeteccionSalida > TIEMPO_MINIMO_SALIDA)) {
        contadorSalida++;
        ultimaDeteccionSalida = millis();
        Serial.print("Botella salida detectada: ");
        Serial.println(contadorSalida);
        enviarEstadoSerial();
      }
      lastIRSalida = currentIRSalida;
      
      if (contadorSalida >= cantidadMax) {
        contadorCiclos++;
        contadorBotellas = 0;
        contadorSalida = 0;
        
        digitalWrite(PISTON1, RELAY_OFF);
        digitalWrite(PISTON2, RELAY_ON);
        digitalWrite(CINTA, RELAY_ON);
        
        etapa = 1;
        etapaStartTime = millis();
        Serial.println("Ciclo completado -> Reinicio");
        enviarEstadoSerial();
      }
      
      if (millis() - etapaStartTime > 30000) {
        Serial.println("TIMEOUT ETAPA 7 - Reinicio forzoso");
        contadorCiclos++;
        contadorBotellas = 0;
        contadorSalida = 0;
        
        digitalWrite(PISTON1, RELAY_OFF);
        digitalWrite(PISTON2, RELAY_ON);
        
        etapa = 1;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
  }
}

void leerEncoder() {
  int clkState = digitalRead(CLK);
  int dtState = digitalRead(DT);

  if (clkState != lastClkState) {
    if (dtState != clkState) {
      encoderPos++;
    } else {
      encoderPos--;
    }
    encoderChanged = true;
  }
  lastClkState = clkState;
}

void cambiarVariable() {
  if (!editando) {
    editando = true;
    variableSeleccionada = 0;
    Serial.println("Modo edicion: tiempo1");
  } else {
    variableSeleccionada++;
    if (variableSeleccionada > 1) {
      editando = false;
      variableSeleccionada = 0;
      guardarVariablesEEPROM();
      Serial.println("Edicion finalizada. Guardado en EEPROM.");
    } else {
      Serial.println(variableSeleccionada == 0 ? "Editando: tiempo1" : "Editando: tiempo2");
    }
  }
  enviarEstadoSerial();
}

void cargarVariablesEEPROM() {
  EEPROM.get(0, tiempo1);
  EEPROM.get(4, tiempo2);

  if (tiempo1 == 0xFFFFFFFF || tiempo1 <= 0) {
    tiempo1 = 6000;
    tiempo2 = 6000;
    tiempoPicos = 1000;
    tiempox = 1000;
    cantidadMax = 4;
    guardarVariablesEEPROM();
    Serial.println("Valores por defecto cargados en EEPROM");
  }
}

void guardarVariablesEEPROM() {
  EEPROM.put(0, tiempo1);
  EEPROM.put(4, tiempo2);
  Serial.println("Variables guardadas en EEPROM");
}

void enviarEstadoSerial() {
  int tiempoRestante_s = 0;

  if (sistemaActivo && etapa >= 2) {
    unsigned long elapsedTime = millis() - etapaStartTime;
    long tiempoTotalEtapa = 0;

    if (etapa == 2) tiempoTotalEtapa = TIEMPO_ALINEACION;
    else if (etapa == 3) tiempoTotalEtapa = tiempox;
    else if (etapa == 4) tiempoTotalEtapa = tiempoPicos;
    else if (etapa == 5) tiempoTotalEtapa = tiempo1;
    else if (etapa == 6) tiempoTotalEtapa = tiempo2;
    else if (etapa == 65) tiempoTotalEtapa = TIEMPO_LEVANTE_PICOS;
    else if (etapa == 7) tiempoTotalEtapa = 30000;

    long tiempoRestante_ms = (tiempoTotalEtapa > elapsedTime) ? (tiempoTotalEtapa - elapsedTime) : 0;
    tiempoRestante_s = tiempoRestante_ms / 1000;
  }

  // Para la TFT, enviamos la etapa 6.5 como "6" para que la pantalla no se confunda
  int etapaParaTFT = (etapa == 65) ? 6 : etapa;

  Serial.print(tiempo1 / 1000);
  Serial.print(",");
  Serial.print(tiempo2 / 1000);
  Serial.print(",");
  Serial.print(contadorBotellas);
  Serial.print(",");
  Serial.print(contadorSalida);
  Serial.print(",");
  Serial.print(contadorCiclos);
  Serial.print(",");
  Serial.print(variableSeleccionada);
  Serial.print(",");
  Serial.print(etapaParaTFT);
  Serial.print(",");
  Serial.print(sistemaActivo ? 1 : 0);
  Serial.print(",");
  Serial.println(tiempoRestante_s);
}