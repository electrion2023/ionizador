Aun en proceso 6/10/26-  falta qr real y direccionamiento

qwen "esp8266lavadodeBidones"
nueva Version 3.1

maquina Lavado de Bidones
Estado actual: Lógica y hardware funcionando.
⚠️ Pendiente: Probar generación de QR real de Mercado Pago en el HTML (actualmente se usa el simulador_pago.html).
🔌 Diagrama de Conexiones (ESP32)
B1 (Seleccionar / Llenado): GPIO 16 (INPUT_PULLUP)
B2 (Confirmar / Lavado): GPIO 17 (INPUT_PULLUP)
Relé Llenado: GPIO 25
Relé Lavado: GPIO 14
Servo: GPIO 26 (Requiere librería ESP32Servo)
Caudalímetro: GPIO 34 (Interrupción RISING)
⚡ Importante: Alimentar Servo y Relés con fuente externa (compartir GND con ESP32). Código incluye desactivación de Brownout.
🔄 Flujo de Trabajo (Lógica)
Estado 0 (Selección):
B1: Cicla bidones (6L ➔ 12L ➔ 20L).
B2: Confirma selección. ESP escribe en Firebase y pasa al Estado 1.
Estado 1 (Esperando Pago):
ESP consulta Firebase cada 500ms buscando pagoOk = 1.
HTML consulta API de Mercado Pago. Si detecta pago nuevo por el monto exacto escribe pagoOk = 1.
Estado 2 (Modo Libre / Operativo):
B1: Activa/Desactiva Llenado (cuenta pulsos).
B2: Activa/Desactiva Lavado.
Corte automático: Si Litros >= Capacidad Bidón ➔ Apaga Llenado, Mueve Servo a 90°, espera 5 segundos y Resetea a Estado 0.
☁️ Firebase (Realtime Database)
Ruta base: nuevaVersion/prueba1/
Nodos: paso, bidonSel, seleccionConfirmada, pagoOk, litrosActuales.
️ Ojo con los litros: El ESP envía litros * 10 (ej: 5.7L = 57). El HTML divide / 10 para mostrar decimales y mover la barra suave.
📁 Archivos del Proyecto
esp32_final.ino: Código del microcontrolador.
index.html: Interfaz para la tablet (Responsive, 3 pantallas).
simulador_pago.html: Botón gigante para emular el pago (Mantener presionado = pagoOk:1, Soltar = pagoOk:0). Usar este para debug rápido.