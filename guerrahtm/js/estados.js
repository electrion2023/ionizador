/**
 * ============================================================================
 * MÓDULO: ESTADOS (MÁQUINA DE ESTADOS FINITOS)
 * ============================================================================
 * Orquesta la lógica del negocio.
 * Reacciona a los cambios en Firebase y coordina UI, Videos y Temporizadores.
 */

import { mostrarPantalla, actualizarProgresoLlenado } from './ui.js';
import { reproducirVideo, detenerTodos } from './videos.js';
import { iniciarCuentaRegresiva } from './countdown.js';
import { escribirNodo } from './firebase.js';

// Memoria volátil de la transacción actual
let bidonSeleccionado = 0; // 6, 12 o 20
let procesoActivo = false;

// Rutas de Firebase (Constantes)
const PATH_PASO = 'dispositivo/estado/paso';
const PATH_PAGADO = 'dispositivo/estado/pagado';

/**
 * Procesa los cambios detectados en el nodo principal de estados (/paso).
 * El ESP8266 o la misma aplicación pueden disparar estos cambios.
 * * Estados esperados: 'ir', 'h', 'h1', 'b6', 'b12', 'b20', 'll', 'ok'
 * @param {string} nuevoPaso - Valor actual en Firebase.
 */
export function procesarCambioDePaso(nuevoPaso) {
    if (!nuevoPaso) return;
    
    // Sanitizar entrada para evitar problemas de formato al leer del NodeMCU
    const paso = nuevoPaso.toString().trim().toLowerCase();
    console.log(`[Estado] Transición a paso: ${paso}`);

    switch (paso) {
        case 'ir':
            // Señal para salir de StandBy y comenzar la experiencia
            if (!procesoActivo) {
                iniciarFlujoCompra();
            }
            break;

        case 'll':
            // El NodeMCU indica que empezó a llenar
            mostrarPantalla('llenando');
            break;

        case 'ok':
            // El NodeMCU indica que el llenado terminó exitosamente
            finalizarCompra();
            break;

        default:
            // Otros pasos (h, b6, etc.) son escritos por este frontend y leídos por el ESP8266.
            // No requieren acción visual directa desde este switch.
            break;
    }
}

/**
 * Inicia el flujo de la HMI: Sale de loop StandBy -> Intro -> Selección
 */
function iniciarFlujoCompra() {
    procesoActivo = true;
    mostrarPantalla('intro');
    
    reproducirVideo('intro', () => {
        // Al terminar el video intro, pasamos a seleccionar el bidón
        mostrarPantalla('seleccion');
    });
}

/**
 * Registra el tamaño del bidón elegido por el usuario y avanza al pago.
 * @param {number} litros - Capacidad (6, 12 o 20)
 */
export function setBidonSeleccionado(litros) {
    bidonSeleccionado = litros;
    console.log(`[Estado] Bidón seleccionado: ${bidonSeleccionado}L`);
    
    // Reseteamos el flag de pago por seguridad antes de mostrar el QR
    escribirNodo(PATH_PAGADO, false);
    mostrarPantalla('qr');
}

/**
 * Verifica si el pago entró (pagado=true) y lanza el proceso de lavado.
 * @param {boolean} estadoPago 
 */
export function procesarEstadoPago(estadoPago) {
    // Si la máquina no está en un proceso activo, ignoramos pagos huérfanos
    if (estadoPago === true && procesoActivo && bidonSeleccionado > 0) {
        iniciarLavado();
    }
}

/**
 * Secuencia 5: Video Lavado e instrucción al NodeMCU (h)
 */
function iniciarLavado() {
    mostrarPantalla('lavado');
    
    // Indicamos al controlador que active la bomba/válvula de lavado
    escribirNodo(PATH_PASO, 'h');

    // Iniciamos video y un temporizador visual de ejemplo (10 segundos)
    reproducirVideo('lavado');
    iniciarCuentaRegresiva(10, () => {
        // Al terminar el tiempo/video de lavado, notificamos al ESP8266 qué bidón es
        const comandoBidon = `b${bidonSeleccionado}`;
        escribirNodo(PATH_PASO, comandoBidon);
        
        // Automáticamente pasamos a la instrucción de girar el bidón
        iniciarInstruccionGirar();
    });
}

/**
 * Secuencia 6: Video para indicar al usuario que dé vuelta el bidón
 */
function iniciarInstruccionGirar() {
    mostrarPantalla('girar');
    // Este video puede estar en loop o detenerse al final. 
    // Queda en pantalla hasta que el NodeMCU escriba 'll' (detecta botón físico).
    reproducirVideo('girar');
}

/**
 * Actualiza la UI de llenado. Llamado desde el listener de Firebase.
 * @param {number} litrosActuales - Reporte en tiempo real del sensor de caudal.
 */
export function actualizarCaudal(litrosActuales) {
    // Solo actualizamos la UI si estamos en la fase donde importa
    if (procesoActivo && bidonSeleccionado > 0) {
        actualizarProgresoLlenado(litrosActuales, bidonSeleccionado);
    }
}

/**
 * Secuencia 8: Cierre exitoso y retorno a StandBy
 */
function finalizarCompra() {
    mostrarPantalla('gracias');
    reproducirVideo('gracias', () => {
        resetearSistema();
    });
    
    // Fallback por si el video no dispara el evento 'ended'
    setTimeout(() => {
        if (procesoActivo) resetearSistema();
    }, 8000); 
}

/**
 * Resetea las variables y pone la máquina en estado inicial, lista para otra venta.
 */
function resetearSistema() {
    procesoActivo = false;
    bidonSeleccionado = 0;
    detenerTodos();
    
    // Limpiamos Firebase para la próxima transacción
    escribirNodo(PATH_PASO, '');
    escribirNodo(PATH_PAGADO, false);
    escribirNodo('dispositivo/estado/litros', 0);
    
    // Volvemos a la pantalla de espera
    mostrarPantalla('standby');
    reproducirVideo('standby');
}
