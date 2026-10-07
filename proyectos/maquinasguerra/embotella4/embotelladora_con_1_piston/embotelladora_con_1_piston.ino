#include <EEPROM.h>

// --- Pines ---
#define CLK 2
#define DT 3
#define SW 4
#define BTN_START 11
#define BOMBA 5
#define PISTON1 10
#define PISTON2 6
#define CINTA 8
#define PICOS 7
#define ANTIGOTEO 9

#define IR_PIN 12
#define IR_SALIDA 6
// --- Variables EEPROM ---
// --- Variables EEPROM --- (ANTES: float)
long tiempo1, tiempo2, tiempoPicos=1000, tiempox=1000;  // ✅ AHORA: long (milisegundos enteros)
int cantidadMax = 4;
bool huboCambio = false;
int total;
int subtotal;
// NUEVA VARIABLE: Contador de ciclos completados
int contadorCiclos = 0;
int contadorSalida = 0;
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
  pinMode(PISTON1, OUTPUT);
  pinMode(PISTON2, OUTPUT);
  pinMode(CINTA, OUTPUT);
  pinMode(PICOS, OUTPUT);
  pinMode(ANTIGOTEO, OUTPUT);
  pinMode(IR_SALIDA, INPUT_PULLUP);
  // --- Apagar todo al inicio ---
  digitalWrite(BOMBA, LOW);
  digitalWrite(PISTON1, LOW);
  digitalWrite(PISTON2, LOW);
  digitalWrite(CINTA, LOW);
  digitalWrite(PICOS, LOW);
  digitalWrite(ANTIGOTEO, LOW);

  attachInterrupt(digitalPinToInterrupt(CLK), leerEncoder, CHANGE);

  cargarVariablesEEPROM();
  
  // INICIALIZACIÓN: El contador de ciclos empieza en 0
  contadorCiclos = 0;

  valorAntes=tiempo1+tiempo2;

  lastClkState = digitalRead(CLK);
  Serial.println("Sistema iniciado. Esperando Start...");
  Serial.println("Variables cargadas de EEPROM:");
  Serial.print("t1="); Serial.print(tiempo1);
  Serial.print(" t2="); Serial.print(tiempo2);
  Serial.print(" t3="); Serial.print(tiempoPicos);
  Serial.print(" t4="); Serial.print(tiempox);
  Serial.print(" ciclos="); Serial.println(contadorCiclos);
  
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

  // --- Botón del encoder (para cambiar variable a editar) ---
  if (digitalRead(SW) == LOW) {
    if (millis() - lastButtonTime > 250) {
      Serial.print("click");
      
      guardarVariablesEEPROM();
     
      
      cambiarVariable();
      lastButtonTime = millis();
      enviarEstadoSerial();
    }
  }

  // --- Contador IR ---
  static int lastIR = HIGH;
  int currentIR = digitalRead(IR_PIN);
  if (currentIR == LOW && lastIR == HIGH && sistemaActivo && etapa == 1) {
    contadorBotellas++;delay(250);
    Serial.print("Botella detectada: ");
    Serial.println(contadorBotellas);
    
  }
  if (contadorBotellas >= cantidadMax && currentIR == HIGH) {
    contadorBotellas=0;
      digitalWrite(PISTON1, HIGH);
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
  
  // --- Si el sistema está corriendo, ejecutar etapas ---
  if (sistemaActivo) {
    ejecutarEtapas();
  }
}

// ===================== FUNCIONES DE LA MÁQUINA =====================

void iniciarSecuencia() {
  etapa = 1;
  contadorBotellas = 0;
  Serial.println("Inicio de secuencia");
  Serial.println("ETAPA 1");
  digitalWrite(CINTA, HIGH);
  digitalWrite(PISTON2, HIGH);
}

void detenerSecuencia() {
  sistemaActivo = false;
  etapa = 0;
  contadorBotellas = 0;
  Serial.println("Secuencia detenida");
  digitalWrite(BOMBA, LOW);
  digitalWrite(PISTON1, LOW);
  digitalWrite(PISTON2, LOW);
  digitalWrite(CINTA, LOW);
  digitalWrite(PICOS, LOW);
  digitalWrite(ANTIGOTEO, LOW);
}

void ejecutarEtapas() {
  switch (etapa) {
    case 2:
      digitalWrite(CINTA, LOW);
      digitalWrite(ANTIGOTEO, HIGH);
      Serial.println("ETAPA 3");
      etapa = 3;
      etapaStartTime = millis();
      enviarEstadoSerial();
      break;
    case 3:
      if (millis() - etapaStartTime >= tiempox) { 
        digitalWrite(PICOS, HIGH);
        Serial.println("ETAPA 4 -> Picos ON");
        etapa = 4;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
    case 4:
      if (millis() - etapaStartTime >= tiempoPicos) {
        digitalWrite(BOMBA, HIGH);
        Serial.println("ETAPA 5 -> Bomba ON");
        etapa = 5;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
    case 5:
      if (millis() - etapaStartTime >= tiempo1) {
        digitalWrite(BOMBA, LOW);
        Serial.println("ETAPA 6 -> Bomba OFF");
        etapa = 6;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
    case 6:
      if (millis() - etapaStartTime >= tiempo2) {
        digitalWrite(PICOS, LOW);
        digitalWrite(ANTIGOTEO, LOW);
        digitalWrite(PISTON2, LOW);
        contadorSalida = 0;
        delay (1000);
        Serial.println("ETAPA 6 -> Picos OFF / Antigoteo OFF / Piston2 OFF");
        etapa = 7;
        etapaStartTime = millis();
        enviarEstadoSerial();
      }
      break;
    case 7:
      // Activar cinta para sacar bidones
  digitalWrite(CINTA, HIGH);
  digitalWrite(PISTON1, HIGH);
  static int lastIRSalida = HIGH;
  int currentIRSalida = digitalRead(IR_SALIDA);

  // Detectar flanco (igual que hiciste en entrada)
  if (currentIRSalida == LOW && lastIRSalida == HIGH) {
    contadorSalida++;
    delay(200); // anti-rebote simple

    //Serial.print("Salida detectada: ");
    Serial.println(contadorSalida);

   
  }
 if (contadorSalida >= cantidadMax && currentIRSalida == HIGH) {
      Serial.println("Ciclo completo -> Reinicio");

      contadorCiclos++;

      contadorBotellas = 0;
      contadorSalida = 0;

      digitalWrite(PISTON1, LOW);
      digitalWrite(PISTON2, HIGH);
      digitalWrite(CINTA, HIGH);

      etapa = 1;
      enviarEstadoSerial();
    }
  lastIRSalida = currentIRSalida;
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
         
       
        // Solo se editan tiempos (variableSeleccionada 0 a 3)
        case 0:  tiempo1 = constrain(tiempo1 + encoderPos * 1000, 0, 99000L); break;
        case 1:  tiempo2 = constrain(tiempo2 + encoderPos * 1000, 0, 99000L); break;
       
        //if (suma != valorAntes) huboCambio = true;valorAntes = tiempo1;Serial.print(huboCambio);
        //if (tiempo2 != valorAntes) huboCambio = true;valorAntes = tiempo2;Serial.print(huboCambio);
      }
      Serial.print("Cambio variable ");
      Serial.print(variableSeleccionada);
      Serial.print(" = ");
      Serial.println(variableSeleccionada == 0 ? tiempo1 : variableSeleccionada == 1 ? tiempo2 : variableSeleccionada == 2 ? tiempoPicos : tiempox);
       
      encoderPos = 0;
      enviarEstadoSerial();
    }
  }
  lastClkState = clkState;
}

// --- CAMBIAR VARIABLE A EDITAR ---
void cambiarVariable() {
 
  variableSeleccionada++;
  // RANGO DE EDICIÓN: 0 a 3 (solo tiempos)
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
  //EEPROM.get(8, tiempoPicos);
  //EEPROM.get(12, tiempox);
  
  if (tiempo1 == 0xFFFF) { // nunca grabado
    tiempo1 = tiempo2 = tiempoPicos = tiempox = 6000;
    cantidadMax = 4; // Se mantiene por defecto para la lógica de la máquina
    guardarVariablesEEPROM();
  }
}

void guardarVariablesEEPROM() {
  EEPROM.put(0, tiempo1);
  EEPROM.put(4, tiempo2);
  //EEPROM.put(8, tiempoPicos);
  //EEPROM.put(12, tiempox);
   Serial.println("💾 Guardado en EEPROM");
}

// ===================== COMUNICACIÓN SERIAL =====================

/**
 * Ensambla y envía la trama de 9 valores al Uno.
 * Formato: "t1,t2,t3,t4,contadorCiclos,varSel,etapa,activo,tRestante\n"
 */
void enviarEstadoSerial() {
  
  // 1. Cálculo del tiempo restante (solo para las etapas de tiempo 4-7)
  int tiempoRestante_ms = 0;
  unsigned long elapsedTime = 0;
  
  if (sistemaActivo && (etapa >= 4 && etapa <= 7)) {
    elapsedTime = millis() - etapaStartTime;
    long tiempoTotalEtapa = 0;
    if (etapa == 4) tiempoTotalEtapa = tiempoPicos;
    else if (etapa == 5) tiempoTotalEtapa = tiempo1;
    else if (etapa == 6) tiempoTotalEtapa = tiempo2;
    else if (etapa == 7) tiempoTotalEtapa = tiempox;
    tiempoRestante_ms = (tiempoTotalEtapa > elapsedTime) ?
    (tiempoTotalEtapa - elapsedTime) : 0;
  }
  
  // Convertir a segundos para el display del Uno
  int tiempoRestante_s = tiempoRestante_ms / 1000;
  
  // 2. Envío de la trama con separadores de coma (,) y fin de línea (\n)
  total = tiempo1+tiempo2+contadorBotellas+contadorSalida+contadorCiclos+variableSeleccionada+etapa+tiempoRestante_s;
  if(total != subtotal){ subtotal=total;
  // [0] tiempo1
  Serial.print(tiempo1/1000);
  Serial.print(",");
  // [1] tiempo2
  Serial.print(tiempo2/1000);
  Serial.print(",");
  // [2] tiempoPicos
  Serial.print(contadorBotellas);
  Serial.print(",");
  // [3] tiempox
  Serial.print(contadorSalida);
  Serial.print(",");
  
  // [4] contadorCiclos (REEMPLAZA A cantidadMax)
  Serial.print(contadorCiclos); 
  Serial.print(",");
  
  // [5] variableSeleccionada
  Serial.print(variableSeleccionada);
  Serial.print(",");
  // [6] etapa
  Serial.print(etapa);
  Serial.print(",");
  // [7] sistemaActivo (1 ó 0)
  Serial.print(sistemaActivo ? 1 : 0);
  Serial.print(",");
  // [8] tiempoRestante (termina con println para enviar el '\n')
  Serial.println(tiempoRestante_s);
  }
}