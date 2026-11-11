#include <EEPROM.h>

// --- DEFINICIÓN DE LÓGICA DE RELÉS (Active LOW) ---
#define RELAY_ON  LOW   // El relé se activa
#define RELAY_OFF HIGH  // El relé se desactiva

// --- PINES ---
const int PIN_PISTON = 4;
const int PIN_BOMBA = 5;
const int PIN_SELLADOR = 6;
const int PIN_BOTON_INICIO = 2;
const int PIN_ENCODER_SW = 3;
const int PIN_ENCODER_CLK = 8;
const int PIN_ENCODER_DT = 9;

// --- TIEMPOS ---
int t1 = 1000, t2 = 500, t3 = 800;

// --- ESTADO ---
byte estadoMaquina = 0; // 0:Idle, 1:Llenando, 2:Sellando, 3:Retrayendo, 4:Reset
unsigned long tiempoInicioEstado = 0;

// --- ENCODER ---
int ultimoEstadoCLK = HIGH;
int parametroActual = 0;
bool necesitaGuardar = false;
unsigned long ultimoEnvioSerial = 0;
unsigned long ultimoCambioEncoder = 0;
unsigned long ultimoCambioBoton = 0;

void setup() {
  Serial.begin(9600);

  pinMode(PIN_PISTON, OUTPUT);
  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_SELLADOR, OUTPUT);
  
  pinMode(PIN_BOTON_INICIO, INPUT_PULLUP);
  pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);

  ultimoEstadoCLK = digitalRead(PIN_ENCODER_CLK);

  EEPROM.get(0, t1); EEPROM.get(2, t2); EEPROM.get(4, t3);
  if(t1 < 100 || t1 > 10000) t1 = 1000;
  if(t2 < 100 || t2 > 10000) t2 = 500;
  if(t3 < 100 || t3 > 10000) t3 = 800;

  apagarTodo();
  enviarDatosAlUno();
}

void loop() {
  leerEncoder();
  leerBotones();
  
  // La máquina de estados siempre corre si no está en estado 0
  if (estadoMaquina != 0) {
    maquinaDeEstados();
  }
  
  if (millis() - ultimoEnvioSerial > 200 || necesitaGuardar) {
    enviarDatosAlUno();
    ultimoEnvioSerial = millis();
  }

  if (necesitaGuardar) {
    EEPROM.put(0, t1); EEPROM.put(2, t2); EEPROM.put(4, t3);
    necesitaGuardar = false;
  }
}

void leerEncoder() {
  int estadoCLK = digitalRead(PIN_ENCODER_CLK);
  
  if (estadoCLK != ultimoEstadoCLK && (millis() - ultimoCambioEncoder > 5)) {
    ultimoCambioEncoder = millis();
    
    if (digitalRead(PIN_ENCODER_DT) != estadoCLK) {
      ajustarTiempo(50);
    } else {
      ajustarTiempo(-50);
    }
  }
  ultimoEstadoCLK = estadoCLK;
}

void leerBotones() {
  // Botón de inicio / cancelar
  if (digitalRead(PIN_BOTON_INICIO) == LOW && (millis() - ultimoCambioBoton > 400)) {
    ultimoCambioBoton = millis();
    
    if (estadoMaquina == 0) {
      // INICIAR: Si está en espera, arranca el ciclo
      estadoMaquina = 1;
      tiempoInicioEstado = millis();
      digitalWrite(PIN_PISTON, RELAY_ON);  // Pistón baja
      digitalWrite(PIN_BOMBA, RELAY_ON);   // Bomba prende
    } else {
      // CANCELAR: Si está corriendo, apaga todo y vuelve a espera
      estadoMaquina = 0;
      apagarTodo();
    }
  }

  // Botón del encoder (cambiar parámetro)
  if (digitalRead(PIN_ENCODER_SW) == LOW && (millis() - ultimoCambioBoton > 200)) {
    ultimoCambioBoton = millis();
    parametroActual = (parametroActual + 1) % 3;
  }
}

void ajustarTiempo(int cambio) {
  if (parametroActual == 0) t1 = constrain(t1 + cambio, 100, 50000);
  if (parametroActual == 1) t2 = constrain(t2 + cambio, 100, 20000);
  if (parametroActual == 2) t3 = constrain(t3 + cambio, 100, 10000);
  necesitaGuardar = true;
}

// ==========================================
// LÓGICA DE LA MÁQUINA (NO MODIFICADA)
// ==========================================
void maquinaDeEstados() {
  unsigned long tiempoTranscurrido = millis() - tiempoInicioEstado;

  switch (estadoMaquina) {
    case 1: // Llenando
      if (tiempoTranscurrido >= t1) {
        estadoMaquina = 2;
        tiempoInicioEstado = millis();
        digitalWrite(PIN_BOMBA, RELAY_OFF);    // Bomba apaga
        digitalWrite(PIN_SELLADOR, RELAY_ON);  // Sellador prende
        digitalWrite(PIN_PISTON, RELAY_OFF);   // Pistón baja
      }
      break;
      
    case 2: // Sellando
      if (tiempoTranscurrido >= t2) {
        estadoMaquina = 3;
        tiempoInicioEstado = millis();
        digitalWrite(PIN_SELLADOR, RELAY_OFF); // Sellador apaga
      }
      break;
      
    case 3: // Retrayendo
      if (tiempoTranscurrido >= t3) {
        estadoMaquina = 4;
      }
      break;
      
    case 4: // Reset - Vuelve al estado 1 automáticamente (ciclo infinito)
      digitalWrite(PIN_PISTON, RELAY_ON);  // Pistón baja para el nuevo ciclo
      digitalWrite(PIN_BOMBA, RELAY_ON);   // Bomba prende
      estadoMaquina = 1;
      tiempoInicioEstado = millis();
      break;
  }
}

void apagarTodo() {
  digitalWrite(PIN_PISTON, RELAY_OFF);
  digitalWrite(PIN_BOMBA, RELAY_OFF);
  digitalWrite(PIN_SELLADOR, RELAY_OFF);
}

void enviarDatosAlUno() {
  // Ya no enviamos el +10 de pausa, enviamos el estado real (0 a 4)
  Serial.print("T1:"); Serial.print(t1);
  Serial.print(",T2:"); Serial.print(t2);
  Serial.print(",T3:"); Serial.print(t3);
  Serial.print(",S:"); Serial.print(estadoMaquina);
  Serial.print(",P:"); Serial.println(parametroActual);
}