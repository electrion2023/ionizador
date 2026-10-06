Máquina tapadora Automatizada (1 botella por ciclo)
Sistema automatizado de tapado unitario (una botella a la vez) controlado por dos microcontroladores: un Arduino Nano maneja la lógica física (sensores, relés y tiempos) y un Arduino Uno controla la interfaz gráfica en pantalla TFT.
Componentes: Sensor IR, motor o paso a paso, dos pistones neumáticos (entrada/salida), cinta transportadora y encoder rotativo con pulsador.
Ciclo de trabajo:
ESPERANDO: Cinta en marcha hasta detectar botella.
DETECTADO: Pistón de entrada bloquea y la cinta se detiene.
Tapando: rele se abre el tiempo configurado. La TFT anima el líquido subiendo en tiempo real.
SALIENDO: Se libera la botella llena, el contador suma +1 y vuelve a esperar.
Extras: Configuración de tiempos (Llenado y Despacho) mediante encoder, valores guardados en EEPROM, y animación TFT no bloqueante sincronizada con la máquina real vía serial.
