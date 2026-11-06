#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============================================
// CONFIGURACIÓN
// ============================================
#define WIFI_SSID "nuevaRoma"
#define WIFI_PASSWORD "010199roma"
#define API_KEY "fsItQV1U7FPegMQ0BihgG7Frj2rRcOiP7MZ4b7gE"
#define DATABASE_URL "https://pagos-58644-default-rtdb.firebaseio.com/"

#define PIN_B1            13
#define PIN_B2            14
#define PIN_RELE_LLENADO  32
#define PIN_RELE_LAVADO   33
#define PIN_RELE_BOMBA    25
#define PIN_SERVO         27
#define PIN_CAUDALIMETRO  34

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
Servo miServo;
FirebaseJson jsonReset;

int estadoSistema = 0;
int bidonSel = 0;
bool leyendoFirebase = false;
int ultimoPagoLeido = 0;

volatile long pulsos = 0;
float litrosActuales = 0.0;
const float PULSOS_POR_LITRO = 270.0;

bool llenadoActivo = false;
bool lavadoActivo = false;
volatile bool contandoPulsos = false;

bool lavadoYaUsado = false;
unsigned long tiempoInicioLavado = 0;
const unsigned long TIEMPO_MAX_LAVADO = 5000;

unsigned long ultimoB1 = 0, ultimoB2 = 0;
unsigned long ultimoTiempoLecturaFB = 0;
unsigned long ultimoDebug = 0;
unsigned long tiempoInicioServo = 0;

// TIMEOUT INTELIGENTE (60s solo si está pausado)
unsigned long ultimoBotonPresionado = 0;
const unsigned long TIMEOUT_INACTIVIDAD = 60000;
bool timeoutActivo = false;

// HEARTBEAT (cada 30s)
unsigned long ultimoHeartbeat = 0;
const unsigned long INTERVALO_HEARTBEAT = 30000;

// PING FIREBASE (cada 60s)
unsigned long ultimoPingFirebase = 0;
const unsigned long INTERVALO_PING = 60000;

// CONTROL DE ERRORES SSL
int erroresFirebaseConsecutivos = 0;
const int MAX_ERRORES_ANTES_RESTART = 15;

const unsigned long ANTIRREBOTE = 300;
const unsigned long INTERVALO_LECTURA_FB = 800;

// ============================================
// INTERRUPCIÓN CAUDALÍMETRO
// ============================================
void IRAM_ATTR contadorPulsos() {
  if (contandoPulsos) pulsos++;
}

// ============================================
// SEGURIDAD BOMBA
// ============================================
void actualizarEstadoBomba() {
  bool valvulaLlenadoAbierta = (digitalRead(PIN_RELE_LLENADO) == LOW);
  bool valvulaLavadoAbierta = (digitalRead(PIN_RELE_LAVADO) == LOW);
  if (valvulaLlenadoAbierta || valvulaLavadoAbierta) digitalWrite(PIN_RELE_BOMBA, LOW);
  else digitalWrite(PIN_RELE_BOMBA, HIGH);
}

// ============================================
// VERIFICAR FIREBASE
// ============================================
bool firebaseSeguro() {
  if (!Firebase.ready()) {
    Serial.println("⚠️ Firebase no está listo.");
    erroresFirebaseConsecutivos++;
    if (erroresFirebaseConsecutivos >= MAX_ERRORES_ANTES_RESTART) {
      Serial.println("🔄 Demasiados errores. Reiniciando ESP32...");
      delay(1000); ESP.restart();
    }
    return false;
  }
  erroresFirebaseConsecutivos = 0;
  return true;
}

// ============================================
// PING FIREBASE
// ============================================
void pingFirebase() {
  if (millis() - ultimoPingFirebase >= INTERVALO_PING) {
    ultimoPingFirebase = millis();
    if (firebaseSeguro()) {
      if (Firebase.RTDB.getInt(&fbdo, "nuevaVersion/prueba1/paso")) {
        Serial.println("💓 Ping Firebase OK");
      } else {
        Serial.print("⚠️ Ping falló: "); Serial.println(fbdo.errorReason());
        fbdo.clear();
      }
    }
  }
}

// ============================================
// HEARTBEAT
// ============================================
// ============================================
// HEARTBEAT + CALIDAD DE WIFI
// ============================================
void enviarHeartbeat() {
  if (millis() - ultimoHeartbeat >= INTERVALO_HEARTBEAT) {
    ultimoHeartbeat = millis();
    
    if (firebaseSeguro()) {
      long timestamp = millis();
      int rssi = WiFi.RSSI(); // 🔑 Obtiene la calidad de señal en dBm
      
      // Calcular calidad en porcentaje (0-100%)
      // -30 dBm = 100%, -100 dBm = 0%
      int calidad = map(rssi, -100, -30, 0, 100);
      calidad = constrain(calidad, 0, 100); // Limitar entre 0 y 100
      
      // Interpretación textual
      String calidadTexto;
      if (rssi >= -50) calidadTexto = "Excelente";
      else if (rssi >= -65) calidadTexto = "Buena";
      else if (rssi >= -75) calidadTexto = "Regular";
      else calidadTexto = "Mala";
      
      // Escribir en Firebase
      jsonReset.clear();
      jsonReset.set("lastSeen", timestamp);
      jsonReset.set("wifiRSSI", rssi);
      jsonReset.set("wifiCalidad", calidad);
      
      if (Firebase.RTDB.updateNode(&fbdo, "nuevaVersion/prueba2/heartbeat", &jsonReset)) {
        Serial.print("💓 Heartbeat | RSSI: ");
        Serial.print(rssi);
        Serial.print(" dBm | Calidad: ");
        Serial.print(calidad);
        Serial.print("% (");
        Serial.print(calidadTexto);
        Serial.println(")");
      } else {
        Serial.print("⚠️ Fallo heartbeat: ");
        Serial.println(fbdo.errorReason());
        fbdo.clear();
      }
    }
  }
}

// ============================================
// ESCRITURA SEGURA
// ============================================
bool escribirFirebase(const char* path, int valor) {
  if (!firebaseSeguro()) return false;
  if (Firebase.RTDB.setInt(&fbdo, path, valor)) return true;
  else {
    Serial.print("❌ Error escribiendo "); Serial.print(path);
    Serial.print(": "); Serial.println(fbdo.errorReason());
    fbdo.clear(); erroresFirebaseConsecutivos++;
    return false;
  }
}

// ============================================
// SETUP
// ============================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  
  pinMode(PIN_B1, INPUT_PULLUP);
  pinMode(PIN_B2, INPUT_PULLUP);
  pinMode(PIN_RELE_LLENADO, OUTPUT);
  pinMode(PIN_RELE_LAVADO, OUTPUT);
  pinMode(PIN_RELE_BOMBA, OUTPUT);
  
  digitalWrite(PIN_RELE_LLENADO, HIGH);
  digitalWrite(PIN_RELE_LAVADO, HIGH);
  digitalWrite(PIN_RELE_BOMBA, HIGH);
  
  miServo.attach(PIN_SERVO);
  miServo.write(0); delay(500);
  miServo.write(180); delay(500);
  miServo.write(0);
  
  attachInterrupt(digitalPinToInterrupt(PIN_CAUDALIMETRO), contadorPulsos, RISING);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(500); Serial.print("."); intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi OK. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ ERROR WiFi.");
    delay(2000); ESP.restart();
  }

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = API_KEY;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  config.timeout.serverResponse = 10000;

  escribirFirebase("nuevaVersion/prueba1/bidonSel", 0);
  escribirFirebase("nuevaVersion/prueba1/seleccionConfirmada", 0);
  escribirFirebase("nuevaVersion/prueba1/pagoOk", 0);
  escribirFirebase("nuevaVersion/prueba1/paso", 0);
  escribirFirebase("nuevaVersion/prueba1/lastSeen", 0);

  ultimoPingFirebase = millis();
  ultimoHeartbeat = millis();
  
  Serial.println("🚀 Sistema Listo. Estado 0: Selección.");
}

// ============================================
// LOOP
// ============================================
void loop() {
  unsigned long ahora = millis();

  enviarHeartbeat();
  pingFirebase();

  bool b1Presionado = (digitalRead(PIN_B1) == LOW) && (ahora - ultimoB1 > ANTIRREBOTE);
  bool b2Presionado = (digitalRead(PIN_B2) == LOW) && (ahora - ultimoB2 > ANTIRREBOTE);

  if (b1Presionado) {
    ultimoB1 = ahora;
    ultimoBotonPresionado = ahora;
  }
  if (b2Presionado) {
    ultimoB2 = ahora;
    ultimoBotonPresionado = ahora;
  }

  switch (estadoSistema) {
    
    // --- ESTADO 0: SELECCIÓN ---
    case 0:
      timeoutActivo = false;
      if (b1Presionado) {
        if (bidonSel == 0) bidonSel = 5;
        else if (bidonSel == 5) bidonSel = 10;
        else if (bidonSel == 10) bidonSel = 20;
        else bidonSel = 5;
        Serial.print("🔘 B1: Bidón seleccionado: ");
        Serial.println(bidonSel);
        escribirFirebase("nuevaVersion/prueba1/bidonSel", bidonSel);
      }
      if (b2Presionado && bidonSel > 0) {
        Serial.println("✅ B2: Confirmado. Esperando pago...");
        estadoSistema = 1;
        leyendoFirebase = true;
        ultimoPagoLeido = 0;
        escribirFirebase("nuevaVersion/prueba1/seleccionConfirmada", 1);
        escribirFirebase("nuevaVersion/prueba1/paso", 1);
      }
      break;

    // --- ESTADO 1: ESPERANDO PAGO ---
    case 1:
      timeoutActivo = false;
      if (b1Presionado || b2Presionado) {
        Serial.println("❌ Cancelación por botón.");
        estadoSistema = 0;
        bidonSel = 0;
        leyendoFirebase = false;
        ultimoPagoLeido = 0;
        escribirFirebase("nuevaVersion/prueba1/bidonSel", 0);
        escribirFirebase("nuevaVersion/prueba1/seleccionConfirmada", 0);
        escribirFirebase("nuevaVersion/prueba1/pagoOk", 0);
        escribirFirebase("nuevaVersion/prueba1/paso", 0);
        break;
      }
      if (leyendoFirebase && (ahora - ultimoTiempoLecturaFB >= INTERVALO_LECTURA_FB)) {
        ultimoTiempoLecturaFB = ahora;
        if (firebaseSeguro()) {
          if (Firebase.RTDB.getInt(&fbdo, "nuevaVersion/prueba1/pagoOk")) {
            if (fbdo.intData() == 1 && ultimoPagoLeido == 0) {
              ultimoPagoLeido = 1;
              Serial.println("💰 Pago OK. Modo Libre.");
              leyendoFirebase = false;
              estadoSistema = 2;
              escribirFirebase("nuevaVersion/prueba1/paso", 2);
            }
          }
        }
      }
      break;

    // --- ESTADO 2: MODO LIBRE (SIN ESCRITURAS DURANTE LLENADO) ---
    case 2:
      if (!timeoutActivo) {
        timeoutActivo = true;
        ultimoBotonPresionado = ahora;
        Serial.println("⏱️ Timeout de 60s activado.");
      }
      
      // TIMEOUT INTELIGENTE: solo si está PAUSADO
      if (!llenadoActivo && !lavadoActivo && (ahora - ultimoBotonPresionado >= TIMEOUT_INACTIVIDAD)) {
        Serial.println("⚠️ USUARIO ABANDONÓ (60s pausado). Reseteando...");
        
        estadoSistema = 0;
        bidonSel = 0;
        pulsos = 0;
        llenadoActivo = false;
        lavadoActivo = false;
        lavadoYaUsado = false;
        contandoPulsos = false;
        timeoutActivo = false;
        
        digitalWrite(PIN_RELE_LAVADO, HIGH);
        digitalWrite(PIN_RELE_LLENADO, HIGH);
        actualizarEstadoBomba();
        
        jsonReset.clear();
        jsonReset.set("bidonSel", 0);
        jsonReset.set("seleccionConfirmada", 0);
        jsonReset.set("pagoOk", 0);
        jsonReset.set("paso", 0);

        if (firebaseSeguro()) {
          Firebase.RTDB.updateNode(&fbdo, "nuevaVersion/prueba1", &jsonReset);
          Serial.println("✅ Sistema reseteado por abandono.");
        }
        break;
      }

      // 🔑 SOLO CÁLCULO LOCAL, SIN ESCRIBIR EN FIREBASE
      litrosActuales = (float)pulsos / PULSOS_POR_LITRO;

      if (ahora - ultimoDebug >= 1000) {
        ultimoDebug = ahora;
        Serial.print("📊 [Pulsos: "); Serial.print(pulsos);
        Serial.print("] [Litros: "); Serial.print(litrosActuales, 2);
        Serial.print("L / BidonSel: "); Serial.print(bidonSel);
        Serial.print(" | Llenado: "); Serial.print(llenadoActivo ? "ON" : "OFF");
        Serial.print(" | Lavado: "); Serial.print(lavadoActivo ? "ON" : "OFF");
        Serial.print(" | Bomba: "); Serial.println(digitalRead(PIN_RELE_BOMBA) == LOW ? "ON" : "OFF");
      }

      // B1: Llenado (toggle)
      if (b1Presionado) {
        llenadoActivo = !llenadoActivo;
        digitalWrite(PIN_RELE_LLENADO, llenadoActivo ? LOW : HIGH);
        contandoPulsos = llenadoActivo;
        actualizarEstadoBomba();
        Serial.println(llenadoActivo ? "💧 Llenado ON" : "💧 Llenado OFF");
      }
      
      // B2: Lavado (uso único, 5 seg)
      if (b2Presionado) {
        if (!lavadoYaUsado && !lavadoActivo) {
          lavadoActivo = true;
          lavadoYaUsado = true;
          tiempoInicioLavado = ahora;
          digitalWrite(PIN_RELE_LAVADO, LOW);
          actualizarEstadoBomba();
          Serial.println("🚿 Lavado ON (5 seg - Uso único)");
        } else if (lavadoYaUsado) {
          Serial.println("⚠️ Lavado ya utilizado.");
        }
      }

      // Corte automático lavado a los 5s
      if (lavadoActivo && (ahora - tiempoInicioLavado >= TIEMPO_MAX_LAVADO)) {
        lavadoActivo = false;
        digitalWrite(PIN_RELE_LAVADO, HIGH);
        actualizarEstadoBomba();
        Serial.println("⏱️ Lavado finalizado.");
      }

      // 🔑 CORTE POR LITROS: SOLO ESCRIBE EN FIREBASE AL COMPLETAR
      if (litrosActuales >= bidonSel && bidonSel > 0) {
        Serial.println("🛑 ¡LÍMITE DE LITROS ALCANZADO! Cortando TODO.");
        
        llenadoActivo = false;
        lavadoActivo = false;
        contandoPulsos = false;
        digitalWrite(PIN_RELE_LLENADO, HIGH);
        digitalWrite(PIN_RELE_LAVADO, HIGH);
        actualizarEstadoBomba();
        
        if (bidonSel == 10 || bidonSel == 20) {
            Serial.println("🔄 Moviendo servo (10L o 20L)...");
            miServo.write(100);
            tiempoInicioServo = ahora;
            estadoSistema = 3;
        } else {
            Serial.println("✅ Bidón de 5L completado. Reseteando...");
            estadoSistema = 0;
            bidonSel = 0;
            pulsos = 0;
            lavadoYaUsado = false;
            timeoutActivo = false;
            
            // 🔑 SOLO ESCRIBE AL FINAL
            jsonReset.clear();
            jsonReset.set("bidonSel", 0);
            jsonReset.set("seleccionConfirmada", 0);
            jsonReset.set("pagoOk", 0);
            jsonReset.set("paso", 0);
            
            if (firebaseSeguro()) {
              Firebase.RTDB.updateNode(&fbdo, "nuevaVersion/prueba1", &jsonReset);
              Serial.println("✅ Reset exitoso (5L)");
            }
        }
      }
      break;

    // --- ESTADO 3: ESPERA SERVO ---
    case 3:
      timeoutActivo = false;
      if (ahora - tiempoInicioServo >= 5000) {
        Serial.println("🔄 Reseteando sistema completo.");
        miServo.write(0);
        
        estadoSistema = 0;
        bidonSel = 0;
        pulsos = 0;
        lavadoYaUsado = false;
        
        digitalWrite(PIN_RELE_LAVADO, HIGH);
        digitalWrite(PIN_RELE_LLENADO, HIGH);
        actualizarEstadoBomba();
        
        jsonReset.clear();
        jsonReset.set("bidonSel", 0);
        jsonReset.set("seleccionConfirmada", 0);
        jsonReset.set("pagoOk", 0);
        jsonReset.set("paso", 0);

        if (firebaseSeguro()) {
          if (Firebase.RTDB.updateNode(&fbdo, "nuevaVersion/prueba1", &jsonReset)) {
            Serial.println("✅ Reset exitoso en Firebase");
          } else {
            Serial.print("⚠️ Fallo Firebase: "); Serial.println(fbdo.errorReason());
            fbdo.clear();
          }
        }
      }
      break;
  }

  delay(10);
}