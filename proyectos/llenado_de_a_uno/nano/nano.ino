#include <EEPROM.h>

// ================= PINES FÍSICOS =================
#define PIN_RELE_CINTA         4
#define PIN_RELE_PISTON1       5 // Delimitador (Entrada)
#define PIN_RELE_PISTON2       6 // Retenedor (Posición de llenado)
#define PIN_RELE_LLENADO       7 // === CAMBIO: Antes era PIN_RELE_TAPADORA, ahora activa la electroválvula de llenado ===

#define PIN_SENSOR_IR          8

#define PIN_ENC_CLK            2 
#define PIN_ENC_DT             3 
#define PIN_ENC_SW             9 

// ================= CONFIGURACIÓN DE LÓGICA =================
#define RELE_ON        LOW  
#define RELE_OFF       HIGH
#define IR_DETECTADO   LOW  

// ================= DIRECCIONES EEPROM =================
// Se mantienen las mismas direcciones para no perder la configuración guardada
#define ADDR_TIEMPO_1  0  // Antes ADDR_T_TAPADO, ahora es Tiempo de Llenado
#define ADDR_TIEMPO_2  2  // Antes ADDR_T_DESPACHO

// ================= VARIABLES GLOBALES =================
int tiempoLlenado = 1500;    // === CAMBIO: Nombre más descriptivo (antes tiempoTapado) ===
int tiempoDespacho = 2000;  
int contador = 0;

// Variables del Encoder
int ultimoEstadoCLK;
unsigned long ultimoDebounceSW = 0;
int modoMenu = 0; 

// Variables de estado del proceso
// === CAMBIO: Se reemplaza TAPANDO por LLENANDO ===
enum EstadoMaquina { ESPERANDO, DETECTADO, LLENANDO, SALIENDO };
EstadoMaquina estadoActual = ESPERANDO;
const char* nombresEstados[] = {"ESPERANDO", "DETECTADO", "LLENANDO", "SALIENDO"};

// =====================================================
// ENVIAR DATOS SERIAL AL ARDUINO UNO (HMI)
// =====================================================
void enviarDatosHMI() {
  Serial.print(nombresEstados[estadoActual]);
  Serial.print(",");
  Serial.print(contador);
  Serial.print(",");
  Serial.print(tiempoLlenado); // Envía el tiempo de llenado
  Serial.print(",");
  Serial.print(tiempoDespacho);
  Serial.print(",");
  Serial.println(modoMenu); 
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(9600);

  pinMode(PIN_RELE_CINTA, OUTPUT);
  pinMode(PIN_RELE_PISTON1, OUTPUT);
  pinMode(PIN_RELE_PISTON2, OUTPUT);
  pinMode(PIN_RELE_LLENADO, OUTPUT); // === CAMBIO ===
  
  pinMode(PIN_SENSOR_IR, INPUT);
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  // Carga desde EEPROM (usa las mismas direcciones, así que lee los valores guardados)
  EEPROM.get(ADDR_TIEMPO_1, tiempoLlenado);
  EEPROM.get(ADDR_TIEMPO_2, tiempoDespacho);

  // Validación de rangos seguros
  if (tiempoLlenado < 100 || tiempoLlenado > 15000) tiempoLlenado = 1500;
  if (tiempoDespacho < 100 || tiempoDespacho > 18000) tiempoDespacho = 2000;

  // Estado Inicial Seguro
  digitalWrite(PIN_RELE_CINTA, RELE_ON);
  digitalWrite(PIN_RELE_PISTON1, RELE_OFF);
  digitalWrite(PIN_RELE_PISTON2, RELE_ON); 
  digitalWrite(PIN_RELE_LLENADO, RELE_OFF); // === CAMBIO ===

  ultimoEstadoCLK = digitalRead(PIN_ENC_CLK);

  enviarDatosHMI();
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {
  
  // ----- GESTIÓN DEL PULSADOR DEL ENCODER -----
  if (digitalRead(PIN_ENC_SW) == LOW) {
    if (millis() - ultimoDebounceSW > 350) { 
      modoMenu++;
      if (modoMenu > 2) {
        modoMenu = 0;
        // Guarda los valores actualizados en EEPROM
        EEPROM.put(ADDR_TIEMPO_1, tiempoLlenado);
        EEPROM.put(ADDR_TIEMPO_2, tiempoDespacho);
      }
      enviarDatosHMI();
      ultimoDebounceSW = millis();
    }
  }

    // ----- GESTIÓN DEL GIRO DEL ENCODER (CON ANTIREBOTE REAL) -----
  static int ultimoEstadoCLK = digitalRead(PIN_ENC_CLK);
  static unsigned long ultimoCambioEncoder = 0;
  const unsigned long debounceEncoder = 10; // 10ms de antirebote (ajustable)

  int estadoActualCLK = digitalRead(PIN_ENC_CLK);
  
  // Solo actuamos si el estado del pin CLK cambió
  if (estadoActualCLK != ultimoEstadoCLK) {
    
    // Verificamos que haya pasado el tiempo de antirebote
    if (millis() - ultimoCambioEncoder > debounceEncoder) {
      
      int estadoActualDT = digitalRead(PIN_ENC_DT);
      bool huboCambioReal = false;

      // Lógica robusta de detección de dirección
      if (estadoActualCLK == HIGH && estadoActualDT == LOW) {
        // Giró a la DERECHA (Sentido Horario) -> SUMAR
        if (modoMenu == 1) {
          tiempoLlenado = constrain(tiempoLlenado + 100, 200, 15000);
          huboCambioReal = true;
        }
        if (modoMenu == 2) {
          tiempoDespacho = constrain(tiempoDespacho + 100, 500, 18000);
          huboCambioReal = true;
        }
      } 
      else if (estadoActualCLK == HIGH && estadoActualDT == HIGH) {
        // Giró a la IZQUIERDA (Sentido Antihorario) -> RESTAR
        if (modoMenu == 1) {
          tiempoLlenado = constrain(tiempoLlenado - 100, 200, 15000);
          huboCambioReal = true;
        }
        if (modoMenu == 2) {
          tiempoDespacho = constrain(tiempoDespacho - 100, 500, 18000);
          huboCambioReal = true;
        }
      }

      // SOLO enviamos datos al HMI si el valor realmente cambió
      if (huboCambioReal) {
        enviarDatosHMI();
      }
      
      // Reiniciamos el timer de antirebote y guardamos el estado
      ultimoCambioEncoder = millis();
    }
    ultimoEstadoCLK = estadoActualCLK;
  }


  // ----- MAQUINA DE ESTADOS DEL PROCESO -----
  if (modoMenu == 0) {
    switch (estadoActual) {
      
      case ESPERANDO:
        digitalWrite(PIN_RELE_CINTA, RELE_ON);
        digitalWrite(PIN_RELE_PISTON1, RELE_OFF);
        digitalWrite(PIN_RELE_PISTON2, RELE_ON);
        digitalWrite(PIN_RELE_LLENADO, RELE_OFF);

        if (digitalRead(PIN_SENSOR_IR) == IR_DETECTADO) {
          estadoActual = DETECTADO;Serial.println("sensorir");
          enviarDatosHMI();
        }
        break;

      case DETECTADO:
        digitalWrite(PIN_RELE_PISTON1, RELE_ON); // Bloquea entrada
        delay(600); // Margen para que la botella se acomode bajo la boquilla
        digitalWrite(PIN_RELE_CINTA, RELE_OFF);  // Detiene la cinta
        
        estadoActual = LLENANDO; // === CAMBIO ===
        enviarDatosHMI();
        break;

      case LLENANDO: // === CAMBIO: Antes era TAPANDO ===
        digitalWrite(PIN_RELE_LLENADO, RELE_ON); // Abre la electroválvula de llenado
        delay(tiempoLlenado);                    // Espera el tiempo configurado para llenar
        digitalWrite(PIN_RELE_LLENADO, RELE_OFF); // Cierra la electroválvula
        
        digitalWrite(PIN_RELE_PISTON2, RELE_OFF); // Abre compuerta de salida
        digitalWrite(PIN_RELE_CINTA, RELE_ON);    // Reactiva la cinta
        
        estadoActual = SALIENDO;
        enviarDatosHMI();
        break;

      case SALIENDO:
        delay(tiempoDespacho); // Tiempo para que la botella salga completamente
        contador++; 
        
        // Cierra compuerta de salida y abre la de entrada para recibir otra botella
        digitalWrite(PIN_RELE_PISTON2, RELE_ON);  
        digitalWrite(PIN_RELE_PISTON1, RELE_OFF); 
        
        estadoActual = ESPERANDO;
        enviarDatosHMI();
        break;
    }
  }
}
