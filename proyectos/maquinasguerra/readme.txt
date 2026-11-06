C:\Users\electrion\Documents\Arduino\proyectos Guerra\Balanza_FondoNegro_conBoton\acctualizadoConQwen


README - Sistema de Balanza con Llenado Automático
📋 Descripción General
Sistema de dos Arduino que controla una balanza con llenado automático mediante dos relés (flujo grueso y goteo). El Nano controla la lógica de llenado y la celda de carga, mientras que el Uno muestra la información en una pantalla TFT.
🏗️ Arquitectura del Sistema
┌─────────────────┐                    ┌─────────────────┐
│  Arduino Nano   │   Serial (9600)    │  Arduino Uno    │
│  (Cerebro)      │───────────────────▶│  + TFT Shield   │
│                 │   D1(TX)→D0(RX)    │  (Visualización)│
│  - HX711        │                    │                 │
│  - 2 Relés      │                    │  - Pantalla TFT │
│  - Botones      │                    │  - Logo desde SD│
│  - Buzzer       │                    │                 │
│  - EEPROM       │                    │                 │
└─────────────────┘                    └─────────────────┘
📁 Archivos del Proyecto
balanzaIniciaConBotonSinPesoytapa.ino (Nano)
Función: Controlador principal del sistema de llenado
Responsabilidades:
Lee la celda de carga HX711
Controla los 2 relés según el peso
Gestiona los botones para configurar pesos objetivo
Guarda configuraciones en EEPROM
Envía datos al Uno por Serial
Emite sonidos con el buzzer
Pines utilizados:
D2: DT del HX711
D3: SCK del HX711
D4: Botón comodín (guardar EEPROM)
D6: Botón selección MEM/PRE
D7: Botón + (incrementar)
D8: Buzzer
D9: Botón inicio de carga
D10: Relé 1 (flujo grueso)
D12: Relé 2 (goteo)
A2: Botón - (decrementar)
Lógica de llenado:
Sistema en reposo (relés OFF) hasta presionar botón inicio (D9)
Peso < preMemo1 → ambos relés ON (llenado rápido)
preMemo1 ≤ peso < memo1 → solo relé2 ON (goteo)
Peso ≥ memo1 → ambos relés OFF (llenado completo)
Al retirar el recipiente (peso < 20g), vuelve a reposo
Protocolo de comunicación con Uno:
P<peso>\n → peso actual en gramos
a<memo1>\n → peso máximo objetivo
b<preMemo1>\n → peso de pre-cierre (cambio a goteo)
pantallatftbalanzafin_copy_20260911134030.ino (Uno)
Función: Interfaz visual del sistema
Responsabilidades:
Recibe datos del Nano por Serial
Muestra el peso actual en un círculo central
Cambia el color del círculo según el rango de peso
Muestra los valores de MEM y PRE configurados
Carga y muestra un logo desde tarjeta SD
Visualización:
Círculo central con el peso en grande
Colores del círculo:
Blanco: peso < 30g (vacío)
Amarillo: 30g ≤ peso ≤ preMemo1 (llenando)
Rojo: preMemo1 < peso ≤ memo1 (cerca del final)
Verde: peso > memo1 (llenado completo)
Textos estáticos: "MEM:", "PRE:", "MAQUINAS GUERRA"
Logo BMP desde SD en esquina superior derecha
Protocolo de recepción:
Lee mensajes del Nano con formato X<valor>\n
Parsea el tipo (primer carácter) y el valor (resto)
Actualiza solo los elementos que cambiaron (optimización)
🔌 Cableado
Nano ↔ Uno (Comunicación Serial)
Nano D1 (TX)  →  Uno D0 (RX)
Nano GND      →  Uno GND
Celda de Carga → HX711 → Nano
Celda Rojo (E+)  →  HX711 E+
Celda Negro (E-) →  HX711 E-
Celda Blanco (S+)→  HX711 A+
Celda Verde (S-) →  HX711 A-

HX711 DT  →  Nano D2
HX711 SCK →  Nano D3
HX711 VCC →  Nano 5V
HX711 GND →  Nano GND
Relés
Relé 1 (flujo grueso) → Nano D10
Relé 2 (goteo)        → Nano D12
Nota: Los relés se activan con HIGH (lógica directa)
Botones (todos con INPUT_PULLUP, activo en LOW)
Botón inicio    → Nano D9
Botón +         → Nano D7
Botón -         → Nano A2
Botón selección → Nano D6
Botón guardar   → Nano D4
Buzzer
Buzzer (+) → Nano D8
Buzzer (-) → GND
⚙️ Configuración
Calibración de la celda
En el Nano, línea:
cpp
celda.set_scale(442.0);
Este valor es específico de tu celda. Para recalibrar:
Colocá un peso conocido (ej: 500g)
Leé el valor crudo con celda.get_units(1)
Calculá: factor = lectura_cruda / peso_conocido
Actualizá set_scale(factor)
Valores por defecto
memo1 (peso máximo): 300g
preMemo1 (pre-cierre): 200g
Rango válido: 50g - 5000g
Incremento/decremento: 25g
EEPROM
Dirección 130: guarda memo1
Dirección 160: guarda preMemo1
Se guardan al presionar el botón comodín (D4)
🎯 Flujo de Operación
Para el operador:
Encender el sistema
Ambos Arduino inician
TFT muestra "MAQUINAS GUERRA" y luego la interfaz
Buzzer emite beep de inicio
Celda se tara automáticamente (asegurar que esté vacía)
Configurar pesos objetivo (si es necesario)
Presionar botón selección (D6) para elegir entre MEM o PRE
Usar botones + (D7) y - (A2) para ajustar
Presionar botón guardar (D4) para persistir en EEPROM
Iniciar llenado
Colocar recipiente vacío sobre la balanza
Presionar botón inicio (D9)
El sistema arranca el llenado automático
Durante el llenado
Círculo amarillo: llenando rápido
Círculo rojo: goteo final
Círculo verde: llenado completo
Buzzer emite sonidos en cada transición
Retirar recipiente
Esperar a que el círculo esté verde
Retirar el recipiente
El sistema vuelve a reposo automáticamente
Colocar nuevo recipiente y repetir desde paso 3
🔧 Componentes Necesarios
Arduino Nano
Arduino Uno
TFT Shield 2.8" o 3.2" (MCUFRIEND compatible)
Módulo HX711
Celda de carga (la que sea, ajustar factor de escala)
2 Relés (5V, contactos adecuados para tu válvula/bomba)
Buzzer pasivo o activo
5 Botones momentáneos (push buttons)
Tarjeta microSD con archivo LOGO.BMP (100x100px recomendado)
Fuente de alimentación (5V, mínimo 1A)
📝 Notas Técnicas
Optimizaciones implementadas:
Nano: sin librerías pesadas (sin SoftwareSerial, sin OLED)
Uno: buffer char en vez de String (evita fragmentación RAM)
Uno: timeout Serial de 50ms (evita bloqueos)
Uno: solo redibuja el círculo cuando cambia de color (evita parpadeo)
Nano: detección de flanco en botón inicio (un solo pulso arranca el ciclo)
Posibles mejoras futuras:
Filtro de promedio móvil para el peso (suavizar lecturas)
Acknowledge en el protocolo (confirmación de recepción)
Watchdog: mostrar "SIN SEÑAL" si el Nano se desconecta
Menú de calibración desde la TFT
Histéresis en los umbrales para evitar oscilaciones
🐛 Solución de Problemas
La TFT no muestra el peso:
Verificar cable Nano D1 → Uno D0
Verificar GND común
Verificar baudios (ambos en 9600)
El peso oscila mucho:
Aumentar el promedio en get_units(2) a get_units(5)
Verificar que los cables de la celda estén cortos y alejados de fuentes de ruido
Verificar alimentación estable (5V regulado)
Los relés no se activan:
Verificar que la lógica sea correcta (HIGH = ON)
Medir con multímetro los pines D10 y D12
Verificar que el módulo de relés tenga alimentación adecuada
La EEPROM no guarda:
Verificar que se presione el botón comodín (D4)
Los valores se guardan en direcciones 130 y 160
Al primer uso, se cargan valores por defecto (300 y 200)
📅 Historial de Cambios
Versión actual (2026-11-06):
Eliminado Bluetooth y OLED del Nano
Eliminado servo y potenciómetro
Movido buzzer de D11 a D8 (evita conflicto Timer2 con HX711)
Movido botón inicio de A1 a D9
Corregida lógica de relés (ahora HIGH = ON)
Corregido bug: la máquina no arranca sola, espera botón inicio
Optimizada comunicación Serial (protocolo unificado)
Optimizada TFT (sin parpadeo, sin fragmentación RAM)
👤 Autor
Proyecto desarrollado para MAQUINAS GUERRA