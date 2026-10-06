#include <EEPROM.h>

// =====================================================
// PINES
// =====================================================

// --- NUEVOS PULSADORES DE CONFIGURACION ---
const int PIN_IZQ  = 2;     // Disminuir tiempo
const int PIN_DER  = 3;     // Aumentar tiempo
const int PIN_PUSH = 4;     // Cambiar HIDRO / BOMBA

// --- BOTONES DE ARRANQUE ---
const int PIN_BTN_HIDRO = 5;
const int PIN_BTN_BOMBA = 6;

// --- RELES ---
const int PIN_RELE_HIDRO = 7;
const int PIN_RELE_BOMBA = 8;

// --- BUZZER ---
const int PIN_BUZZER = 13;


// =====================================================
// EEPROM
// =====================================================

const int ADDR_HIDRO = 0;
const int ADDR_BOMBA = 4;


// =====================================================
// TIEMPOS
// =====================================================

unsigned long tiempoHidro = 5000UL;    // 5 segundos
unsigned long tiempoBomba = 10000UL;   // 10 segundos


// =====================================================
// MODO DE EDICION
// =====================================================

// 0 = HIDRO
// 1 = BOMBA

int modoEdicion = 0;


// =====================================================
// CONTROL DE CAMBIOS
// =====================================================

bool huboCambio = false;

unsigned long ultimoHidroGuardado = 0;
unsigned long ultimoBombaGuardado = 0;


// =====================================================
// ESTADO DE BOTONES
// =====================================================

bool lastIzq = HIGH;
bool lastDer = HIGH;
bool lastPush = HIGH;

bool lastBtnHidro = HIGH;
bool lastBtnBomba = HIGH;


// =====================================================
// DEBOUNCE
// =====================================================

unsigned long debounceIzq = 0;
unsigned long debounceDer = 0;
unsigned long debouncePush = 0;

unsigned long debounceHidro = 0;
unsigned long debounceBomba = 0;

const unsigned long DEBOUNCE_MS = 45;


// =====================================================
// ESTADO DE RELES
// =====================================================

bool releHidroActivo = false;
bool releBombaActivo = false;

unsigned long inicioHidro = 0;
unsigned long inicioBomba = 0;


// =====================================================
// ENVIO A TFT
// =====================================================

unsigned long ultimoEnvio = 0;
unsigned long ultimoHidroEnviado = 0;
unsigned long ultimoBombaEnviado = 0;

// ACELERADO A 250ms PARA UNA CUENTA REGRESIVA FLUIDA
const unsigned long INTERVALO_ENVIO = 250;


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(9600);


  // ---------------------------------------------------
  // PULSADORES
  // ---------------------------------------------------

  pinMode(PIN_IZQ, INPUT);
  pinMode(PIN_DER, INPUT);
  pinMode(PIN_PUSH, INPUT);

  pinMode(PIN_BTN_HIDRO, INPUT_PULLUP);
  pinMode(PIN_BTN_BOMBA, INPUT_PULLUP);


  // ---------------------------------------------------
  // RELES
  // ---------------------------------------------------

  pinMode(PIN_RELE_HIDRO, OUTPUT);
  pinMode(PIN_RELE_BOMBA, OUTPUT);

  digitalWrite(PIN_RELE_HIDRO, LOW);
  digitalWrite(PIN_RELE_BOMBA, LOW);


  // ---------------------------------------------------
  // BUZZER
  // ---------------------------------------------------

  pinMode(PIN_BUZZER, OUTPUT);


  // ---------------------------------------------------
  // ESTADOS INICIALES
  // ---------------------------------------------------

  lastIzq = digitalRead(PIN_IZQ);
  lastDer = digitalRead(PIN_DER);
  lastPush = digitalRead(PIN_PUSH);

  lastBtnHidro = digitalRead(PIN_BTN_HIDRO);
  lastBtnBomba = digitalRead(PIN_BTN_BOMBA);


  // ---------------------------------------------------
  // EEPROM
  // ---------------------------------------------------

  unsigned long tempH;
  unsigned long tempB;

  EEPROM.get(ADDR_HIDRO, tempH);
  EEPROM.get(ADDR_BOMBA, tempB);

  if (tempH >= 1000 && tempH <= 60000)
    tiempoHidro = tempH;

  if (tempB >= 1000 && tempB <= 60000)
    tiempoBomba = tempB;


  ultimoHidroGuardado = tiempoHidro;
  ultimoBombaGuardado = tiempoBomba;


  // ---------------------------------------------------
  // MENSAJE INICIAL
  // ---------------------------------------------------

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F(" SISTEMA INICIADO"));
  Serial.println(F("================================"));

  imprimirTiempos();

  enviarTiempos();


  // ---------------------------------------------------
  // BEEP DE BIENVENIDA
  // ---------------------------------------------------

  beep(800, 120);
  delay(10);
  beep(1200, 120);
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  unsigned long ahora = millis();


  // ===================================================
  // BOTON IZQUIERDA
  // DISMINUIR TIEMPO
  // ===================================================

  bool izq = digitalRead(PIN_IZQ);

  if (izq == LOW &&
      lastIzq == HIGH &&
      ahora - debounceIzq > DEBOUNCE_MS) {

    debounceIzq = ahora;

    if (modoEdicion == 0) {

      // HIDRO
      if (tiempoHidro >= 2000)
        tiempoHidro -= 1000;

    } else {

      // BOMBA
      if (tiempoBomba >= 2000)
        tiempoBomba -= 1000;
    }

    huboCambio = true;

    beep(1400, 35);

    Serial.println(F("IZQ -> disminuir"));

    imprimirTiempos();
  }

  lastIzq = izq;


  // ===================================================
  // BOTON DERECHA
  // AUMENTAR TIEMPO
  // ===================================================

  bool der = digitalRead(PIN_DER);

  if (der == LOW &&
      lastDer == HIGH &&
      ahora - debounceDer > DEBOUNCE_MS) {

    debounceDer = ahora;

    if (modoEdicion == 0) {

      // HIDRO
      if (tiempoHidro <= 59000)
        tiempoHidro += 1000;

    } else {

      // BOMBA
      if (tiempoBomba <= 59000)
        tiempoBomba += 1000;
    }

    huboCambio = true;

    beep(1900, 35);

    Serial.println(F("DER -> aumentar"));

    imprimirTiempos();
  }

  lastDer = der;


  // ===================================================
  // PUSH
  // CAMBIAR HIDRO / BOMBA
  // ===================================================

  bool push = digitalRead(PIN_PUSH);

  if (push == LOW &&
      lastPush == HIGH &&
      ahora - debouncePush > DEBOUNCE_MS) {

    debouncePush = ahora;

    // Guardar el valor antes de cambiar de modo
    guardarSiCambio();


    // Cambiar modo
    modoEdicion = !modoEdicion;


    if (modoEdicion == 0) {

      beep(700, 140);

      Serial.println();
      Serial.println(F("MODO -> HIDRO"));

    } else {

      beep(1100, 140);

      Serial.println();
      Serial.println(F("MODO -> BOMBA"));
    }

    imprimirTiempos();
  }

  lastPush = push;


  // ===================================================
  // BOTON HIDROLAVADORA
  // ===================================================

  bool bh = digitalRead(PIN_BTN_HIDRO);

  if (bh == LOW &&
      lastBtnHidro == HIGH &&
      ahora - debounceHidro > DEBOUNCE_MS) {

    debounceHidro = ahora;

    activarHidro();
  }

  lastBtnHidro = bh;


  // ===================================================
  // BOTON BOMBA
  // ===================================================

  bool bb = digitalRead(PIN_BTN_BOMBA);

  if (bb == LOW &&
      lastBtnBomba == HIGH &&
      ahora - debounceBomba > DEBOUNCE_MS) {

    debounceBomba = ahora;

    activarBomba();
  }

  lastBtnBomba = bb;


  // ===================================================
  // TEMPORIZADOR HIDRO
  // ===================================================

  if (releHidroActivo &&
      (ahora - inicioHidro >= tiempoHidro)) {

    digitalWrite(PIN_RELE_HIDRO, LOW);

    releHidroActivo = false;

    beep(350, 450);

    Serial.println(F("Hidro -> OFF"));
  }


  // ===================================================
  // TEMPORIZADOR BOMBA
  // ===================================================

  if (releBombaActivo &&
      (ahora - inicioBomba >= tiempoBomba)) {

    digitalWrite(PIN_RELE_BOMBA, LOW);

    releBombaActivo = false;

    beep(350, 450);

    Serial.println(F("Bomba -> OFF"));
  }


  // ===================================================
  // ENVIO A TFT
  // ===================================================

  if ((ahora - ultimoEnvio >= INTERVALO_ENVIO) ||
      tiempoHidro != ultimoHidroEnviado ||
      tiempoBomba != ultimoBombaEnviado) {

    enviarTiempos();

    ultimoEnvio = ahora;

    ultimoHidroEnviado = tiempoHidro;
    ultimoBombaEnviado = tiempoBomba;
  }
}


// =====================================================
// ACTIVAR HIDROLAVADORA
// =====================================================

void activarHidro() {

  if (releHidroActivo)
    return;

  digitalWrite(PIN_RELE_HIDRO, HIGH);

  inicioHidro = millis();

  releHidroActivo = true;

  beep(1550, 90);

  Serial.print(F("Hidro ON -> "));
  Serial.print(tiempoHidro / 1000);
  Serial.println(F(" segundos"));
}


// =====================================================
// ACTIVAR BOMBA
// =====================================================

void activarBomba() {

  if (releBombaActivo)
    return;

  digitalWrite(PIN_RELE_BOMBA, HIGH);

  inicioBomba = millis();

  releBombaActivo = true;

  beep(1550, 90);

  Serial.print(F("Bomba ON -> "));
  Serial.print(tiempoBomba / 1000);
  Serial.println(F(" segundos"));
}


// =====================================================
// MOSTRAR TIEMPOS
// =====================================================

void imprimirTiempos() {

  Serial.print(F("Hidro: "));
  Serial.print(tiempoHidro / 1000);

  Serial.print(F(" s  |  Bomba: "));
  Serial.print(tiempoBomba / 1000);

  Serial.print(F(" s  |  Editando: "));

  if (modoEdicion == 0)
    Serial.println(F("HIDRO"));
  else
    Serial.println(F("BOMBA"));
}


// =====================================================
// GUARDAR EEPROM
// =====================================================

void guardarSiCambio() {

  if (!huboCambio)
    return;


  if (modoEdicion == 0 &&
      tiempoHidro != ultimoHidroGuardado) {

    EEPROM.put(ADDR_HIDRO, tiempoHidro);

    ultimoHidroGuardado = tiempoHidro;

    Serial.println(F("EEPROM -> Hidro guardado"));

    beep(2600, 70);
  }


  else if (modoEdicion == 1 &&
           tiempoBomba != ultimoBombaGuardado) {

    EEPROM.put(ADDR_BOMBA, tiempoBomba);

    ultimoBombaGuardado = tiempoBomba;

    Serial.println(F("EEPROM -> Bomba guardado"));

    beep(2600, 70);
  }


  huboCambio = false;
}


// =====================================================
// ENVIAR TIEMPOS (ACTUALIZADO CON RECUENTO RESTANTE)
// =====================================================

void enviarTiempos() {
  unsigned long ahora = millis();
  
  long restH = 0;
  if (releHidroActivo) {
    restH = tiempoHidro - (ahora - inicioHidro);
    if (restH < 0) restH = 0;
  }
  
  long restB = 0;
  if (releBombaActivo) {
    restB = tiempoBomba - (ahora - inicioBomba);
    if (restB < 0) restB = 0;
  }

  // Formato: <H(configurado)B(configurado)X(restanteHidro)Y(restanteBomba)>
  Serial.print(F("<H"));
  Serial.print(tiempoHidro);
  Serial.print(F("B"));
  Serial.print(tiempoBomba);
  Serial.print(F("X"));
  Serial.print(restH);
  Serial.print(F("Y"));
  Serial.print(restB);
  Serial.println(F(">"));
}


// =====================================================
// BUZZER
// =====================================================

void beep(int freq, int duracion) {

  tone(PIN_BUZZER, freq, duracion);
}