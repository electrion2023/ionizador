# Máquinas Guerra - Panel de Tiempos

Panel con Arduino Nano que controla 2 relés:
- HIDROLAVADORA
- BOMBA DE LLENADO

Una pantalla TFT muestra tiempos configurados, estado y cuenta regresiva.

---

## Flujo de uso

1. El Nano recibe botones para configurar tiempos:
   - IZQ = bajar tiempo
   - DER = subir tiempo
   - PUSH = cambia entre HIDRO / BOMBA y guarda cambios

2. Botones de arranque:
   - BTN_HIDRO activa relé hidrolavadora
   - BTN_BOMBA activa relé bomba

3. Cada relé permanece activo durante su tiempo configurado.

4. Mientras un relé está activo, el Nano envía por Serial el tiempo restante.

5. La TFT muestra:
   - Tiempo configurado
   - Estado LISTO / ACTIVO
   - Cuenta regresiva en segundos
   - Barra de progreso

6. Al terminar:
   - El Nano apaga el relé
   - Suena beep
   - La TFT vuelve a LISTO

---

## Conexiones rápidas

### Arduino Nano - pines usados

| Función              | Pin |
|----------------------|-----|
| Botón IZQ            | 2   |
| Botón DER            | 3   |
| Botón PUSH           | 4   |
| Botón arranque HIDRO | 5   |
| Botón arranque BOMBA | 6   |
| Relé HIDRO           | 7   |
| Relé BOMBA           | 8   |
| Buzzer               | 13  |

### Notas eléctricas

- Botones de arranque HIDRO/BOMBA:
  - Configurados como `INPUT_PULLUP`
  - Se activan a `LOW`

- Botones de configuración IZQ/DER/PUSH:
  - En el código están como `INPUT`
  - Si se usan hacia GND sin resistencia externa, cambiar a `INPUT_PULLUP`

- Relés:
  - Activan con `LOW`
  - Se apagan con `HIGH`

### Comunicación Nano -> TFT

- Serial a 9600 baudios
- Conexión mínima:
  - Nano TX -> TFT RX
  - GND común

Si se usa el mismo puerto Serial para cargar o monitor, puede convenir desconectar la línea RX/TX durante la carga.

---

## Protocolo Serial

Formato nuevo:

```text
<H5000B10000h5000b0>