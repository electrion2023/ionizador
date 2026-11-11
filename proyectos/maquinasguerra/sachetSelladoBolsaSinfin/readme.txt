📦 Sachet Packer Pro - Documentación
Sistema de empaque automático de sachets controlado por dos Arduinos: Nano (cerebro y potencia) y Uno (interfaz HMI con pantalla TFT).
🛠️ Hardware y Pinout (Arduino Nano)
Componente
Pin Nano
Notas
Relé Pistón
4
Active LOW (LOW = ON)
Relé Bomba
5
Active LOW (LOW = ON)
Relé Sellador
6
Active LOW (LOW = ON)
Botón Inicio/Cancel
2
INPUT_PULLUP (LOW al presionar)
Encoder Botón (SW)
3
INPUT_PULLUP
Encoder CLK
8
Requiere capacitor 100nF a GND
Encoder DT
9
Requiere capacitor 100nF a GND
Serial TX
1
Va al RX del UNO
Serial RX
0
Va al TX del UNO
🔌 Conexiones entre Nano y UNO
TX (Nano) ➔ RX (UNO)
RX (Nano) ➔ TX (UNO)
GND (Nano) ➔ GND (UNO) (¡Crucial!)
Baudrate: 9600
⚙️ Lógica de la Máquina (Ciclo Infinito)
El botón de inicio funciona como Start/Cancel:
Estado 0 (Espera): Al presionar el botón, inicia el ciclo.
Estado 1 (Llenado): Pistón y Bomba ON. Espera T1.
Estado 2 (Sellado): Bomba y Pistón OFF, Sellador ON. Espera T2.
Estado 3 (Retracción): Sellador OFF. Espera T3.
Estado 4 (Reset): Pistón y Bomba ON. Vuelve automáticamente al Estado 1.
Cancelación: Si se presiona el botón durante cualquier estado (1 a 4), la máquina vuelve a Estado 0 y apaga todos los relés inmediatamente.
📡 Protocolo de Comunicación
El Nano envía datos al UNO cada 200ms en formato texto:
text
1
(S = Estado de máquina, P = Parámetro seleccionado por encoder)
⚠️ Notas Importantes / "Gotchas"
Relés Active LOW: Los módulos de relé chinos se activan con LOW. En el código se usa #define RELAY_ON LOW para no confundirse.
EEPROM: Los tiempos (T1, T2, T3) se guardan en la EEPROM (direcciones 0, 2, 4) para no perderlos al apagar.
Encoder Ruidoso: Si el encoder salta números, es falta de hardware. Agregar capacitores cerámicos de 100nF (104) entre los pines CLK, DT y SW a tierra (GND).
Pantalla TFT: Usa la librería MCUFRIEND_kbv. Si la pantalla no enciende o se ve blanca, verificar el parche de ID en el setup() (if (ID == 0xD3D3) ID = 0x9486;).
Depuración: Como se usa el puerto serie hardware (pines 0 y 1) para comunicar los Arduinos, no se puede usar el Monitor Serie a menos que se desconecte el cable TX/RX entre ellos.