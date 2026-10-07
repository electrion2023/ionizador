#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>
// Desactivar el detector de Brownout (Evita reinicios por picos de energía de los relés/servo)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============================================
// CONFIGURACIÓN WIFI Y FIREBASE
// ============================================
#define WIFI_SSID "ElectrionM"
#define WIFI_PASSWORD "lean1234"
#define API_KEY "fsItQV1U7FPegMQ0BihgG7Frj2rRcOiP7MZ4b7gE"
#define DATABASE_URL "https://pagos-58644-default-rtdb.firebaseio.com/"

// ============================================
// PINES SEGUROS (Sin conflicto de Boot)
// ============================================
#define PIN_B1           16  // Botón 1: Seleccionar / Llenado
#define PIN_B2           17  // Botón 2: Confirmar / Lavado
#define PIN_RELE_LLENADO 25  // Relé bomba de llenado (Cambiado de 5 a 25)
#define PIN_RELE_LAVADO  14  // Relé hidrolavadora
#define PIN_SERVO        27  // Servo motor
#define PIN_CAUDALIMETRO 34  // Sensor de flujo (Cambiado de 4 a 34 - Solo entrada, muy seguro)

// ============================================
// OBJETOS Y VARIABLES GLOBALES
// ============================================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
Servo miServo;

// Variables de estado
int estadoSistema = 0; // 0: Selección, 1: Esperando Pago, 2: Modo Libre
int bidonSel = 0;      // 0, 6, 12, 20
bool leyendoFirebase = false;
int ultimoPagoLeido = 0;

// Variables de hardware
volatile long pulsos = 0;
float litrosActuales = 0.0;
const float PULSOS_POR_LITRO = 250.0; // ⚠️ AJUSTAR SEGÚN TU SENSOR

bool llenadoActivo = false;
bool lavadoActivo = false;

// Timers
unsigned long ultimoB1 = 0, ultimoB2 = 0;
unsigned long ultimoTiempoLecturaFB = 0;
unsigned long ultimoTiempoEscrituraFB = 0;
unsigned long tiempoInicioServo = 0;

const unsigned long ANTIRREBOTE = 300;
const unsigned long INTERVALO_LECTURA_FB = 500; 
const unsigned long INTERVALO_ESCRITURA_FB = 1000; 

void IRAM_ATTR contadorPulsos() {
  pulsos++;
}

void setup() {
  // 🔑 CLAVE: Desactivar Brownout Detector para evitar reinicios por relés/servo
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  
  // Pines
  pinMode(PIN_B1, INPUT_PULLUP);
  pinMode(PIN_B2, INPUT_PULLUP);
  pinMode(PIN_RELE_LLENADO, OUTPUT);
  pinMode(PIN_RELE_LAVADO, OUTPUT);
  digitalWrite(PIN_RELE_LLENADO, LOW);
  digitalWrite(PIN_RELE_LAVADO, LOW);
  
  miServo.attach(PIN_SERVO);
  miServo.write(0); 
  delay(500);  miServo.write(180); 
  delay(500);  miServo.write(0); 
  attachInterrupt(digitalPinToInterrupt(PIN_CAUDALIMETRO), contadorPulsos, RISING);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ WiFi OK");

  // Firebase
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = API_KEY;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Reset inicial en Firebase
  Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/bidonSel", 0);
  Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/seleccionConfirmada", 0);
  Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/pagoOk", 0);
  Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/paso", 0);
  Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/litrosActuales", 0);

  Serial.println("🚀 Sistema Listo y Estable. Estado 0: Selección.");
}

void loop() {
  unsigned long ahora = millis();

  // ==========================================
  // LECTURA DE BOTONES (Siempre activa)
  // ==========================================
  bool b1Presionado = (digitalRead(PIN_B1) == LOW) && (ahora - ultimoB1 > ANTIRREBOTE);
  bool b2Presionado = (digitalRead(PIN_B2) == LOW) && (ahora - ultimoB2 > ANTIRREBOTE);

  if (b1Presionado) { ultimoB1 = ahora; }
  if (b2Presionado) { ultimoB2 = ahora; }

  // ==========================================
  // MÁQUINA DE ESTADOS
  // ==========================================
  switch (estadoSistema) {
    
    // --- ESTADO 0: SELECCIÓN ---
    case 0:
      if (b1Presionado) {
        if (bidonSel == 0) bidonSel = 6;
        else if (bidonSel == 6) bidonSel = 12;
        else if (bidonSel == 12) bidonSel = 20;
        else bidonSel = 6;
        
        Serial.print("🔘 B1: Bidón seleccionado: ");
        Serial.println(bidonSel);
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/bidonSel", bidonSel);
      }
      
      if (b2Presionado && bidonSel > 0) {
        Serial.println("✅ B2: Confirmado. Esperando pago...");
        estadoSistema = 1;
        leyendoFirebase = true;
        ultimoPagoLeido = 0;
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/seleccionConfirmada", 1);
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/paso", 1); 
      }
      break;

    // --- ESTADO 1: ESPERANDO PAGO ---
    case 1:
      if (leyendoFirebase && (ahora - ultimoTiempoLecturaFB >= INTERVALO_LECTURA_FB)) {
        ultimoTiempoLecturaFB = ahora;
        
        if (Firebase.RTDB.getInt(&fbdo, "nuevaVersion/prueba1/pagoOk")) {
          int pago = fbdo.intData();
          if (pago == 1 && ultimoPagoLeido == 0) {
            ultimoPagoLeido = 1;
            Serial.println("💰 ¡PAGO RECIBIDO! Pasando a Modo Libre.");
            leyendoFirebase = false;
            estadoSistema = 2;Serial.println(litrosActuales);
            Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/paso", 2); 
          }
        }
      }
      break;

    // --- ESTADO 2: MODO LIBRE (LAVADO / LLENADO) ---
    case 2:
      litrosActuales = pulsos / PULSOS_POR_LITRO;
      
      if (b1Presionado) {
        llenadoActivo = !llenadoActivo; 
        digitalWrite(PIN_RELE_LLENADO, llenadoActivo ? HIGH : LOW);
        Serial.println(llenadoActivo ? "💧 Llenado ACTIVADO" : "💧 Llenado DESACTIVADO");
      }

      if (b2Presionado) {
        lavadoActivo = !lavadoActivo; 
        digitalWrite(PIN_RELE_LAVADO, lavadoActivo ? HIGH : LOW);
        Serial.println(lavadoActivo ? "🚿 Lavado ACTIVADO" : "🚿 Lavado DESACTIVADO");
      }

      if (llenadoActivo && litrosActuales >= bidonSel) {
        Serial.println("⚠️ Límite de bidón alcanzado. Deteniendo y moviendo servo.");
        llenadoActivo = false;
        digitalWrite(PIN_RELE_LLENADO, LOW);
        
        miServo.write(90);
        tiempoInicioServo = ahora;
        estadoSistema = 3; 
      }

      if (ahora - ultimoTiempoEscrituraFB >= INTERVALO_ESCRITURA_FB) {
        ultimoTiempoEscrituraFB = ahora;
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/litrosActuales", (int)(litrosActuales)); 
        Serial.println(litrosActuales);
      }
      break;

    // --- ESTADO 3: ESPERANDO FIN DEL SERVO ---
    case 3:
      if (ahora - tiempoInicioServo >= 5000) { 
        Serial.println("🔄 Reseteando sistema completo.");
        miServo.write(0); 
        
        estadoSistema = 0;
        bidonSel = 0;
        litrosActuales = 0;
        pulsos = 0;
        llenadoActivo = false;
        lavadoActivo = false;
        digitalWrite(PIN_RELE_LAVADO, LOW);
        
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/bidonSel", 0);
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/seleccionConfirmada", 0);
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/pagoOk", 0);
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/litrosActuales", 0);
        Firebase.RTDB.setInt(&fbdo, "nuevaVersion/prueba1/paso", 0);
      }
      break;
  }

  delay(10); 
}