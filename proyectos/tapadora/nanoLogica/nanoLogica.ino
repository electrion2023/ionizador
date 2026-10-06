#include <EEPROM.h>

// ================= PINES FÍSICOS =================
#define PIN_RELE_CINTA         4
#define PIN_RELE_PISTON1       5 // Delimitador (Entrada)
#define PIN_RELE_PISTON2       6 // Retenedor (Posición de tapado)
#define PIN_RELE_TAPADORA      7

#define PIN_SENSOR_IR          8

#define PIN_ENC_CLK            2 // Ya no requiere interrupción obligatoria
#define PIN_ENC_DT             3 
#define PIN_ENC_SW             9 

// ================= CONFIGURACIÓN DE LÓGICA =================
#define RELE_ON        LOW  
#define RELE_OFF       HIGH
#define IR_DETECTADO   LOW  

// ================= DIRECCIONES EEPROM =================
#define ADDR_T_TAPADO     0  
#define ADDR_T_DESPACHO   2  

// ================= VARIABLES GLOBALES =================
int tiempoTapado = 1500;    
int tiempoDespacho = 2000;  
int contador = 0;

// Variables del Encoder (Lectura por Estado Anterior)
int ultimoEstadoCLK;
unsigned long ultimoDebounceSW = 0;
int modoMenu = 0; 

// Variables de estado del proceso
enum EstadoMaquina { ESPERANDO, DETECTADO, TAPANDO, SALIENDO };
EstadoMaquina estadoActual = ESPERANDO;
const char* nombresEstados[] = {"ESPERANDO", "DETECTADO", "TAPANDO", "SALIENDO"};

// =====================================================
// ENVIAR DATOS SERIAL AL ARDUINO UNO (HMI)
// =====================================================
void enviarDatosHMI() {
  Serial.print(nombresEstados[estadoActual]);
  Serial.print(",");
  Serial.print(contador);
  Serial.print(",");
  Serial.print(tiempoTapado);
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
  pinMode(PIN_RELE_TAPADORA, OUTPUT);
  
  pinMode(PIN_SENSOR_IR, INPUT);
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  EEPROM.get(ADDR_T_TAPADO, tiempoTapado);
  EEPROM.get(ADDR_T_DESPACHO, tiempoDespacho);

  if (tiempoTapado < 100 || tiempoTapado > 10000) tiempoTapado = 1500;
  if (tiempoDespacho < 100 || tiempoDespacho > 10000) tiempoDespacho = 2000;

  // Estado Inicial Seguro
  digitalWrite(PIN_RELE_CINTA, RELE_ON);
  digitalWrite(PIN_RELE_PISTON1, RELE_OFF);
  digitalWrite(PIN_RELE_PISTON2, RELE_ON); 
  digitalWrite(PIN_RELE_TAPADORA, RELE_OFF);

  // Leer estado inicial del encoder
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
        EEPROM.put(ADDR_T_TAPADO, tiempoTapado);
        EEPROM.put(ADDR_T_DESPACHO, tiempoDespacho);
      }
      enviarDatosHMI();
      ultimoDebounceSW = millis();
    }
  }

  // ----- GESTIÓN DEL GIRO DEL ENCODER (MÉTODO DE POLLING ESTABLE) -----
  int estadoActualCLK = digitalRead(PIN_ENC_CLK);
  
  // Si el estado de CLK cambió, es porque hubo un movimiento físico
  if (estadoActualCLK != ultimoEstadoCLK && estadoActualCLK == LOW) {
    
    // Si el estado de DT es diferente al estado de CLK, gira a la derecha
    if (digitalRead(PIN_ENC_DT) != estadoActualCLK) {
      if (modoMenu == 1) tiempoTapado = constrain(tiempoTapado + 100, 200, 15000);
      if (modoMenu == 2) tiempoDespacho = constrain(tiempoDespacho + 100, 500, 18000);
    } 
    // De lo contrario, gira a la izquierda
    else {
      if (modoMenu == 1) tiempoTapado = constrain(tiempoTapado - 100, 200, 5000);
      if (modoMenu == 2) tiempoDespacho = constrain(tiempoDespacho - 100, 500, 8000);
    }
    
    enviarDatosHMI(); // Informar cambios al UNO al instante
  }
  ultimoEstadoCLK = estadoActualCLK; // Guardar el estado para la próxima vuelta


  // ----- MAQUINA DE ESTADOS DEL PROCESO -----
  if (modoMenu == 0) {
    switch (estadoActual) {
      
      case ESPERANDO:
        digitalWrite(PIN_RELE_CINTA, RELE_ON);
        digitalWrite(PIN_RELE_PISTON1, RELE_OFF);
        digitalWrite(PIN_RELE_PISTON2, RELE_ON);
        digitalWrite(PIN_RELE_TAPADORA, RELE_OFF);

        if (digitalRead(PIN_SENSOR_IR) == IR_DETECTADO) {
          estadoActual = DETECTADO;
          Serial.println("sensorir");
          enviarDatosHMI();
        }
        break;

      case DETECTADO:
        digitalWrite(PIN_RELE_PISTON1, RELE_ON); 
        delay(600); // Margen para que se acomode bajo la tapadora
        digitalWrite(PIN_RELE_CINTA, RELE_OFF);  
        
        estadoActual = TAPANDO;
        enviarDatosHMI();
        break;

      case TAPANDO:
        digitalWrite(PIN_RELE_TAPADORA, RELE_ON); 
        delay(tiempoTapado);                      
        digitalWrite(PIN_RELE_TAPADORA, RELE_OFF); 
        
        digitalWrite(PIN_RELE_PISTON2, RELE_OFF);  // Abre compuerta de salida
        digitalWrite(PIN_RELE_CINTA, RELE_ON);     
        
        estadoActual = SALIENDO;
        enviarDatosHMI();
        break;

      case SALIENDO:
        delay(tiempoDespacho); 
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