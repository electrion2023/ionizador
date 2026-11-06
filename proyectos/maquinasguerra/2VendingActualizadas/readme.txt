RESUMEN DEL PROYECTO - MÁQUINA AUTOSERVICIO BIDONES
🎯 DESCRIPCIÓN GENERAL
Sistema de autoservicio para llenado de bidones de agua (5L, 10L, 20L) con pago mediante Mercado Pago, controlado por ESP32 y interfaz web en tiempo real.
ARCHIVOS PRINCIPALES
1. ESP32 (.ino) - maquina1.txt / vendingconping.ino
Función: Controla hardware y lógica de la máquina
Características:
Bidones: 5L → 10L → 20L (ciclo rotativo)
Servo: Solo se activa en 10L y 20L (5L resetea directo)
Lavado: Uso único de 5 segundos por ciclo
Timeout: 60s si el usuario está inactivo/pausado
Heartbeat: Cada 30s envía lastSeen a Firebase
Ping: Cada 60s verifica conexión Firebase
WiFi RSSI: Monitoreo de calidad de señal (opcional)
Pines:
B1 (LLENADO) = 13
B2 (LAVADO) = 14
Relé Llenado = 32
Relé Lavado = 33
Relé Bomba = 25
Servo = 27
Caudalímetro = 34
Lógica de seguridad:
La bomba SOLO se enciende si hay al menos una válvula abierta
Evita sobrepresión cerrando bomba si ambas válvulas están cerradas
2. HTML Interface - index.html / index1.html
Función: Interfaz de usuario en tablet/monitor
Características:
3 Pantallas: Selección → Pago → Operativa
Precios dinámicos: Se leen desde Firebase
QR dinámico: URL desde Firebase
Token MP: Se carga desde Firebase
Lista negra: Evita pagos duplicados (pagosProcesados)
Barra de progreso: Muestra litros en tiempo real
Responsive: Adaptable a celular/tablet/monitor
Flujo de pago:
Usuario selecciona bidón (ESP32 escribe bidonSel)
Presiona B2 para confirmar (ESP32 escribe seleccionConfirmada = 1)
HTML detecta confirmación y consulta Mercado Pago
HTML obtiene último ID de pago conocido
HTML consulta cada 3s pagos aprobados
Si detecta pago nuevo con monto exacto → aprueba
HTML escribe pagoOk = 1 en Firebase
ESP32 detecta pagoOk y pasa a modo libre
🔥 FIREBASE - ESTRUCTURA
Ruta Principal: nuevaVersion/prueba1/ o prueba2/
nuevaVersion/
└── prueba1/
    ├── bidonSel: 0              # Bidón seleccionado (5/10/20)
    ├── seleccionConfirmada: 0   # 1 cuando usuario confirma
    ├── pagoOk: 0                # 1 cuando HTML detecta pago
    ├── paso: 0                  # Estado: 0=Selección, 1=Pago, 2=Operativo
    ├── litrosActuales: 0        # Litros medidos (entero * 10)
    ├── lastSeen: 0              # Timestamp último heartbeat
    ├── heartbeat/               # (Opcional) Datos de WiFi
    │   ├── wifiRSSI: -58
    │   └── wifiCalidad: 60
    └── config/
        ├── mpToken: "APP_USR-..."    # Token Mercado Pago
        ├── qrUrl: "https://..."      # URL imagen QR
        └── precios/
            ├── 5: 1000
            ├── 10: 1800
            └── 20: 2800
🔄 FLUJO COMPLETO DEL SISTEMA
ESTADO 0: SELECCIÓN
1. Usuario presiona B1 → cicla bidones (5→10→20→5)
2. ESP32 escribe bidonSel en Firebase
3. HTML actualiza tarjeta activa y muestra precio
4. Usuario presiona B2 → confirma selección
5. ESP32 escribe seleccionConfirmada=1 y paso=1
ESTADO 1: ESPERANDO PAGO
1. HTML detecta seleccionConfirmada=1
2. Consulta API Mercado Pago por último ID
3. Inicia loop cada 3s buscando pagos aprobados
4. Usuario escanea QR y paga EXACTAMENTE el monto
5. HTML detecta pago nuevo (ID > último conocido)
6. HTML escribe pagoOk=1 en Firebase
7. HTML muestra "¡PAGO ACREDITADO!"
ESTADO 2: MODO LIBRE
1. ESP32 detecta pagoOk=1 → escribe paso=2
2. Usuario puede:
   - Presionar B1 (LLENADO): Activa/desactiva llenado
     * Abre válvula llenado
     * Enciende bomba si hay válvula abierta
     * Caudalímetro cuenta pulsos (solo en llenado)
   
   - Presionar B2 (LAVADO): Uso único 5 segundos
     * Abre válvula lavado (solo 1 vez por ciclo)
     * Enciende bomba
     * Corta automático a los 5s
   
3. Sistema monitorea:
   - Timeout 60s si está pausado (sin llenar/lavar)
   - Límite de litros alcanzado
   
4. Al completar litros:
   - Si es 10L o 20L: 
     * Mueve servo a 100°
     * Espera 5s
     * Vuelve servo a 0°
     * Resetea sistema
   
   - Si es 5L:
     * Resetea directo (sin servo)
ESTADO 3: ESPERA SERVO (solo 10L/20L)
1. Servo en 100° durante 5 segundos
2. Vuelve servo a 0°
3. Resetea todas las variables
4. Escribe todo en 0 en Firebase
5. Vuelve a ESTADO 0
🔧 CONFIGURACIÓN INICIAL
En Firebase Console:
Crear estructura nuevaVersion/prueba1/config/
Agregar:
mpToken: Token de acceso Mercado Pago
qrUrl: URL de imagen QR de pago
precios/5, precios/10, precios/20: Precios en pesos
En ESP32:
Modificar WiFi SSID/PASSWORD
Verificar ruta Firebase (prueba1 o prueba2)
Ajustar PULSOS_POR_LITRO (calibrar caudalímetro)
Subir código
En HTML:
Verificar firebaseConfig (apiKey, databaseURL)
Ajustar ruta para que coincida con ESP32
Subir a GitHub Pages o servidor web
SOLUCIÓN DE PROBLEMAS COMUNES
Problema
Causa
Solución
No detecta pago
Token MP vencido o mal configurado
Verificar mpToken en Firebase
Pago se aprueba solo
No usa lista negra
Implementar pagosProcesados en HTML
Servo no se mueve
Bidón de 5L seleccionado
Normal: solo 10L/20L mueven servo
Bomba no enciende
Válvulas no abren
Verificar relés y lógica invertida (LOW=ON)
WiFi se desconecta
Mala señal RSSI
Agregar repetidor o mover router
Firebase no escribe
Errores SSL consecutivos
ESP32 hace restart automático (15 errores)
📊 MONITOREO REMOTO
Heartbeat (cada 30s):
1
En Firebase:
lastSeen: Timestamp último contacto
Si no se actualiza en 2min → máquina offline
WiFi RSSI:
-30 a -50: Excelente 🟢
-51 a -65: Buena 🟡
-66 a -75: Regular 🟠
-76 a -100: Mala
🔐 SEGURIDAD
Bomba: Nunca enciende sin válvula abierta
Timeout: Reset automático si usuario abandona (60s)
Lavado: Solo 5 segundos, uso único
Firebase: Escrituras atómicas con FirebaseJson
Pagos: Lista negra evita duplicados
📝 NOTAS IMPORTANTES
Caudalímetro: Solo cuenta cuando llenadoActivo = true (no cuenta en lavado)
Lógica invertida: Relés funcionan con LOW = ENCENDIDO
PULSOS_POR_LITRO: Calibrar según caudalímetro (valor default: 270.0)
Ruta Firebase: Debe coincidir EXACTAMENTE entre ESP32 y HTML
Precios: Deben ser exactos para que MP apruebe (sin centavos)
PRÓXIMAS MEJORAS POSIBLES
Panel de estadísticas en tiempo real
Historial de ventas por día/semana
Notificaciones push cuando se complete un ciclo
Modo administrador con PIN
Detección de bidón vacío (sensor adicional)
Reporte de mantenimiento preventivo
Última actualización: Noviembre 2024
Versión: 2.0 con heartbeat y WiFi RSSI
Ubicaciones activas: Carlos Paz (prueba1), Hugo (prueba2)