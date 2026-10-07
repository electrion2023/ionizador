// ============================================================
//  ESP32_Control_Maquina.ino   v2
//  Cerebro autónomo de la llenadora.
//  Lógica de pistones coordinada:
//    PISTON1 = delimitador de ENTRADA
//    PISTON2 = delimitador de SALIDA
// ============================================================
#include <Preferences.h>

// ---------- UART hacia la HMI ----------
HardwareSerial hmi(2);
#define HMI_TX 17
#define HMI_RX 16
#define HMI_BAUD 9600

// ---------- Relés (lógica invertida: LOW = activo) ----------
#define RELE_CINTA      4   // R1
#define RELE_PISTON1    5   // R2  delimitador ENTRADA
#define RELE_PISTON2   13   // R3  delimitador SALIDA
#define RELE_PICOS     14   // R4
#define RELE_ANTIGOTEO 19   // R5
#define RELE_BOMBA     21   // R6

// ---------- Sensores IR ----------
#define SENSOR_IR1 35
#define SENSOR_IR2 34
#define IR_UMBRAL  2000

// ---------- Constantes mecánicas ----------
#define BIDONES_LOTE           4
#define TIEMPO_ALINEACION_MS   2000
#define TIEMPO_POSICIONAR_MS   500
#define TIEMPO_RETIRO_PICOS_MS 500
#define TIEMPO_CIERRE_PISTON_MS 500
#define DEBOUNCE_IR_MS         60

// ---------- Estados de la máquina ----------
enum Estado : uint8_t {
  E_DETENIDO = 0,
  E_INGRESO,         // 1
  E_ALINEACION,      // 2
  E_POSICIONAMIENTO, // 3
  E_LLENADO,         // 4
  E_DESCOMPRESION,   // 5
  E_RETIRO_PICOS,    // 6
  E_SALIDA,          // 7
  E_ESPERA_RETIRO,   // 8
  E_CIERRE           // 9
};

// ---------- Variables globales ----------
Estado estado = E_DETENIDO;
uint32_t tEtapaInicio = 0;
uint32_t tEtapaDuracion = 0;

uint16_t paramLlenadoSeg       = 25;
uint16_t paramDescompresionSeg = 10;
uint16_t paramRetiroMs         = 1500;

uint8_t  cntEntrada = 0;
uint8_t  cntSalida  = 0;
uint32_t cntCiclos  = 0;

bool ir1Estado = false, ir2Estado = false;
bool ir1Filtrado = false, ir2Filtrado = false;
uint32_t ir1UltimoFlanco = 0, ir2UltimoFlanco = 0;
bool ir1ContadoEsteBidon = false, ir2ContadoEsteBidon = false;

String bufHmi = "";
uint32_t tUltimoCharHmi = 0;
const uint32_t TIMEOUT_HMI_MS = 100;

uint32_t tUltimaTele = 0;
const uint32_t PERIODO_TELE_MS = 400;

Estado estadoAnterior = E_DETENIDO;
bool rEstado[6] = {false,false,false,false,false,false};
bool ir1Ant = false, ir2Ant = false;

Preferences prefs;

// ============================================================
//  Funciones de bajo nivel
// ============================================================
inline void setRele(uint8_t pin, bool activo) {
  digitalWrite(pin, activo ? LOW : HIGH);
}

void todosOff() {
  setRele(RELE_CINTA, 0);
  setRele(RELE_PISTON1, 0);
  setRele(RELE_PISTON2, 0);
  setRele(RELE_PICOS, 0);
  setRele(RELE_ANTIGOTEO, 0);
  setRele(RELE_BOMBA, 0);
}

void actualizarEstadosRele() {
  rEstado[0] = (digitalRead(RELE_CINTA)    == LOW);
  rEstado[1] = (digitalRead(RELE_PISTON1)  == LOW);
  rEstado[2] = (digitalRead(RELE_PISTON2)  == LOW);
  rEstado[3] = (digitalRead(RELE_PICOS)    == LOW);
  rEstado[4] = (digitalRead(RELE_ANTIGOTEO)== LOW);
  rEstado[5] = (digitalRead(RELE_BOMBA)    == LOW);
}

// ============================================================
//  Lectura de sensores con antirrebote y flanco
// ============================================================
void leerSensores() {
  uint32_t now = millis();

  int v1 = analogRead(SENSOR_IR1);
  bool nuevo1 = (v1 < IR_UMBRAL);
  if (nuevo1 != ir1Estado) { ir1Estado = nuevo1; ir1UltimoFlanco = now; }
  if (now - ir1UltimoFlanco > DEBOUNCE_IR_MS) ir1Filtrado = ir1Estado;

  int v2 = analogRead(SENSOR_IR2);
  bool nuevo2 = (v2 < IR_UMBRAL);
  if (nuevo2 != ir2Estado) { ir2Estado = nuevo2; ir2UltimoFlanco = now; }
  if (now - ir2UltimoFlanco > DEBOUNCE_IR_MS) ir2Filtrado = ir2Estado;

  static bool ir1Prev = false, ir2Prev = false;

  if (estado == E_INGRESO) {
    if (ir1Filtrado && !ir1Prev && !ir1ContadoEsteBidon) {
      if (cntEntrada < BIDONES_LOTE) cntEntrada++;
      ir1ContadoEsteBidon = true;
    }
    if (!ir1Filtrado) ir1ContadoEsteBidon = false;
  }
  if (estado == E_SALIDA || estado == E_ESPERA_RETIRO || estado == E_CIERRE) {
    if (ir2Filtrado && !ir2Prev && !ir2ContadoEsteBidon) {
      if (cntSalida < BIDONES_LOTE) cntSalida++;
      ir2ContadoEsteBidon = true;
    }
    if (!ir2Filtrado) ir2ContadoEsteBidon = false;
  }
  ir1Prev = ir1Filtrado;
  ir2Prev = ir2Filtrado;
}

// ============================================================
//  Máquina de estados
// ============================================================
void entrarEtapa(Estado e, uint32_t duracionMs = 0) {
  estado = e;
  tEtapaInicio = millis();
  tEtapaDuracion = duracionMs;
}

// -----------------------------------------------------------
//  INICIO DE CICLO: PISTON2 ON (bloquea salida),
//                   PISTON1 OFF (permite ingreso),
//                   CINTA ON
// -----------------------------------------------------------
void transicionIngreso() {
  setRele(RELE_PISTON2, 1);   // bloquea salida
  setRele(RELE_PISTON1, 0);   // permite ingreso
  setRele(RELE_CINTA, 1);     // cinta avanza
  entrarEtapa(E_INGRESO);
}

void tickMaquina() {
  uint32_t ahora = millis();
  uint32_t transcurrido = ahora - tEtapaInicio;

  switch (estado) {

    case E_DETENIDO:
      break;

    // -------------------------------------------------------
    //  E01 INGRESO: P2=ON, P1=OFF, CINTA=ON
    //  Espera 4 bidones + que IR1 quede libre
    // -------------------------------------------------------
    case E_INGRESO:
      if (cntEntrada >= BIDONES_LOTE && !ir1Filtrado) {
        setRele(RELE_PISTON1, 1);   // bloquea ingreso de nuevos
        // PISTON2 sigue ON, CINTA sigue ON
        entrarEtapa(E_ALINEACION, TIEMPO_ALINEACION_MS);
      }
      break;

    // -------------------------------------------------------
    //  E02 ALINEACIÓN: P2=ON, P1=ON, CINTA=ON
    // -------------------------------------------------------
    case E_ALINEACION:
      if (transcurrido >= TIEMPO_ALINEACION_MS) {
        setRele(RELE_CINTA, 0);
        setRele(RELE_PICOS, 1);
        setRele(RELE_ANTIGOTEO, 1);
        // P1=ON, P2=ON se mantienen
        entrarEtapa(E_POSICIONAMIENTO, TIEMPO_POSICIONAR_MS);
      }
      break;

    // -------------------------------------------------------
    //  E03 POSICIONAMIENTO: P2=ON, P1=ON, CINTA=OFF
    // -------------------------------------------------------
    case E_POSICIONAMIENTO:
      if (transcurrido >= TIEMPO_POSICIONAR_MS) {
        setRele(RELE_BOMBA, 1);
        entrarEtapa(E_LLENADO, (uint32_t)paramLlenadoSeg * 1000UL);
      }
      break;

    // -------------------------------------------------------
    //  E04 LLENADO: P2=ON, P1=ON, CINTA=OFF
    // -------------------------------------------------------
    case E_LLENADO:
      if (transcurrido >= (uint32_t)paramLlenadoSeg * 1000UL) {
        setRele(RELE_BOMBA, 0);
        entrarEtapa(E_DESCOMPRESION, (uint32_t)paramDescompresionSeg * 1000UL);
      }
      break;

    // -------------------------------------------------------
    //  E05 DESCOMPRESIÓN: P2=ON, P1=ON, CINTA=OFF
    // -------------------------------------------------------
    case E_DESCOMPRESION:
      if (transcurrido >= (uint32_t)paramDescompresionSeg * 1000UL) {
        setRele(RELE_PICOS, 0);
        setRele(RELE_ANTIGOTEO, 0);
        entrarEtapa(E_RETIRO_PICOS, TIEMPO_RETIRO_PICOS_MS);
      }
      break;

    // -------------------------------------------------------
    //  E06 RETIRO PICOS: P2=ON, P1=ON, CINTA=OFF
    // -------------------------------------------------------
    case E_RETIRO_PICOS:
      if (transcurrido >= TIEMPO_RETIRO_PICOS_MS) {
        // Prepara salida: libera PISTON2, mantiene PISTON1 ON
        setRele(RELE_PISTON2, 0);   // libera salida
        setRele(RELE_CINTA, 1);     // cinta avanza
        // PISTON1 sigue ON (bloquea nuevos ingresos)
        entrarEtapa(E_SALIDA);
      }
      break;

    // -------------------------------------------------------
    //  E07 SALIDA: P2=OFF, P1=ON, CINTA=ON
    // -------------------------------------------------------
    case E_SALIDA:
      if (cntSalida >= BIDONES_LOTE) {
        entrarEtapa(E_ESPERA_RETIRO, paramRetiroMs);
      }
      break;

    // -------------------------------------------------------
    //  E08 ESPERA RETIRO: P2=OFF, P1=ON, CINTA=ON
    //  Espera tiempoRetiro ms después del 4º bidón
    // -------------------------------------------------------
    case E_ESPERA_RETIRO:
      if (transcurrido >= paramRetiroMs) {
        setRele(RELE_CINTA, 0);
        setRele(RELE_PISTON1, 0);   // permite ingreso del siguiente lote
        setRele(RELE_PISTON2, 1);   // bloquea salida hasta próximo ciclo
        entrarEtapa(E_CIERRE, TIEMPO_CIERRE_PISTON_MS);
      }
      break;

    // -------------------------------------------------------
    //  E09 CIERRE: P2=ON, P1=OFF, CINTA=OFF
    //  Espera a que PISTON2 se estabilice y vuelve a INGRESO
    // -------------------------------------------------------
    case E_CIERRE:
      if (transcurrido >= TIEMPO_CIERRE_PISTON_MS) {
        cntCiclos++;
        cntEntrada = 0;
        cntSalida  = 0;
        ir1ContadoEsteBidon = false;
        ir2ContadoEsteBidon = false;
        transicionIngreso();   // arranca nuevo ciclo
      }
      break;
  }
}

// ============================================================
//  STOP local (estado seguro: todo OFF)
// ============================================================
void stopMaquina() {
  todosOff();
  entrarEtapa(E_DETENIDO, 0);
  cntEntrada = 0;
  cntSalida  = 0;
  ir1ContadoEsteBidon = false;
  ir2ContadoEsteBidon = false;
  hmi.println("ST");
}

// ============================================================
//  Guardar / cargar parámetros
// ============================================================
void cargarParametros() {
  prefs.begin("llenadora", true);
  paramLlenadoSeg       = prefs.getUShort("llenado", 25);
  paramDescompresionSeg = prefs.getUShort("descomp", 10);
  paramRetiroMs         = prefs.getUShort("retiro", 1500);
  prefs.end();

  if (paramLlenadoSeg > 999) paramLlenadoSeg = 25;
  if (paramDescompresionSeg > 99) paramDescompresionSeg = 10;
  if (paramRetiroMs > 9999) paramRetiroMs = 1500;
}

bool guardarParametros(uint16_t L, uint16_t D, uint16_t R) {
  if (L > 999 || D > 99 || R > 9999) return false;
  prefs.begin("llenadora", false);
  prefs.putUShort("llenado", L);
  prefs.putUShort("descomp", D);
  prefs.putUShort("retiro",  R);
  prefs.end();
  paramLlenadoSeg       = L;
  paramDescompresionSeg = D;
  paramRetiroMs         = R;
  return true;
}

// ============================================================
//  UART HMI
// ============================================================
void enviarTeleSiCambio() {
  if (estado != estadoAnterior) {
    hmi.print("E"); hmi.printf("%02d\n", (int)estado);
    hmi.print(estado == E_DETENIDO ? "MB\n" : "MA\n");
    estadoAnterior = estado;
  }
  if (ir1Filtrado != ir1Ant) { hmi.println(ir1Filtrado ? "I1A" : "I1B"); ir1Ant = ir1Filtrado; }
  if (ir2Filtrado != ir2Ant) { hmi.println(ir2Filtrado ? "I2A" : "I2B"); ir2Ant = ir2Filtrado; }

  actualizarEstadosRele();
  static bool rPrev[6] = {false,false,false,false,false,false};
  for (uint8_t i = 0; i < 6; i++) {
    if (rEstado[i] != rPrev[i]) {
      hmi.print("R"); hmi.print(i+1); hmi.println(rEstado[i] ? "A" : "B");
      rPrev[i] = rEstado[i];
    }
  }
}

void enviarPeriodico() {
  uint32_t ahora = millis();
  if (ahora - tUltimaTele < PERIODO_TELE_MS) return;
  tUltimaTele = ahora;

  hmi.print("C"); hmi.print(cntEntrada); hmi.print("/");
  hmi.print(cntSalida); hmi.print("/"); hmi.printf("%04lu\n", cntCiclos);

  if (estado != E_DETENIDO && tEtapaDuracion > 0) {
    uint32_t trans = ahora - tEtapaInicio;
    int32_t restante = (int32_t)tEtapaDuracion - (int32_t)trans;
    if (restante < 0) restante = 0;
    hmi.print("T"); hmi.println(restante);
  } else {
    hmi.println("T0");
  }
}

void procesarComandoHmi(const String &cmd) {
  if (cmd.length() == 0) return;

  if (cmd == "OK") {
    if (estado == E_DETENIDO) {
      cntEntrada = 0; cntSalida = 0;
      ir1ContadoEsteBidon = false;
      ir2ContadoEsteBidon = false;
      transicionIngreso();
      hmi.println("OK");
    } else {
      hmi.println("GB");
    }
    return;
  }
  if (cmd == "ST") { stopMaquina(); return; }
  if (cmd == "QS") {
    hmi.print("E"); hmi.printf("%02d\n", (int)estado);
    hmi.println(estado == E_DETENIDO ? "MB" : "MA");
    hmi.println(ir1Filtrado ? "I1A" : "I1B");
    hmi.println(ir2Filtrado ? "I2A" : "I2B");
    actualizarEstadosRele();
    for (uint8_t i = 0; i < 6; i++) {
      hmi.print("R"); hmi.print(i+1); hmi.println(rEstado[i] ? "A" : "B");
    }
    hmi.print("C"); hmi.print(cntEntrada); hmi.print("/");
    hmi.print(cntSalida); hmi.print("/"); hmi.printf("%04lu\n", cntCiclos);
    hmi.print("P"); hmi.printf("%03d%02d%04d\n",
            paramLlenadoSeg, paramDescompresionSeg, paramRetiroMs);
    return;
  }

  if (cmd.startsWith("G") && cmd.length() >= 8) {
    uint16_t L = (cmd[1]-'0')*100 + (cmd[2]-'0')*10 + (cmd[3]-'0');
    uint16_t D = (cmd[4]-'0')*10  + (cmd[5]-'0');
    uint16_t R = 0;
    for (uint8_t i = 6; i < cmd.length(); i++) {
      if (cmd[i] >= '0' && cmd[i] <= '9') R = R*10 + (cmd[i]-'0');
    }
    if (guardarParametros(L, D, R)) hmi.println("GS");
    else                            hmi.println("GE");
    return;
  }

  if (cmd.length() == 3 && cmd[0] == 'R' &&
      cmd[1] >= '1' && cmd[1] <= '6' &&
      (cmd[2] == 'A' || cmd[2] == 'B')) {
    if (estado != E_DETENIDO) { hmi.println("GB"); return; }
    uint8_t idx = cmd[1] - '1';
    bool activo = (cmd[2] == 'A');
    const uint8_t pines[6] = {RELE_CINTA, RELE_PISTON1, RELE_PISTON2,
                              RELE_PICOS, RELE_ANTIGOTEO, RELE_BOMBA};
    setRele(pines[idx], activo);
    hmi.println("GA");
    return;
  }

  hmi.println("GE");
}

void leerHmi() {
  while (hmi.available()) {
    char c = hmi.read();
    tUltimoCharHmi = millis();
    if (c == '\n' || c == '\r') {
      if (bufHmi.length() > 0) {
        bufHmi.trim();
        procesarComandoHmi(bufHmi);
        bufHmi = "";
      }
    } else {
      bufHmi += c;
      if (bufHmi.length() > 32) bufHmi = "";
    }
  }
  if (bufHmi.length() > 0 && (millis() - tUltimoCharHmi > TIMEOUT_HMI_MS)) {
    bufHmi = "";
  }
}

// ============================================================
//  Setup / Loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("=== ESP32 Control Llenadora v2 ===");

  pinMode(RELE_CINTA,   OUTPUT);
  pinMode(RELE_PISTON1, OUTPUT);
  pinMode(RELE_PISTON2, OUTPUT);
  pinMode(RELE_PICOS,   OUTPUT);
  pinMode(RELE_ANTIGOTEO,OUTPUT);
  pinMode(RELE_BOMBA,   OUTPUT);
  todosOff();

  pinMode(SENSOR_IR1, INPUT);
  pinMode(SENSOR_IR2, INPUT);

  hmi.begin(HMI_BAUD, SERIAL_8N1, HMI_RX, HMI_TX);

  cargarParametros();
  entrarEtapa(E_DETENIDO, 0);

  Serial.printf("Params -> L:%u  D:%u  R:%u ms\n",
                paramLlenadoSeg, paramDescompresionSeg, paramRetiroMs);
}

void loop() {
  leerSensores();
  tickMaquina();
  leerHmi();
  enviarTeleSiCambio();
  enviarPeriodico();
  delay(2);
}