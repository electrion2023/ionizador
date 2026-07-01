/**
 * ============================================================================
 * MÓDULO: UI (USER INTERFACE)
 * ============================================================================
 * Encapsula toda la manipulación del DOM. 
 * Expone métodos limpios para que la máquina de estados cambie la vista
 * sin acoplarse a la estructura HTML.
 */

// Cache de elementos del DOM para evitar consultas repetitivas y costosas (Performance)
const screens = document.querySelectorAll('.screen');
const litrosActualesEl = document.getElementById('litros-actuales');
const litrosObjetivoEl = document.getElementById('litros-objetivo');
const progressBar = document.getElementById('progress-bar');
const qrImage = document.getElementById('qr-image');
const cards = document.querySelectorAll('.card');

/**
 * Gestiona la transición entre pantallas (SPA).
 * Utiliza las clases CSS preparadas para animaciones aceleradas por GPU.
 * * @param {string} nombrePantalla - Sufijo del ID de la pantalla (ej: 'standby', 'lavado').
 */
export function mostrarPantalla(nombrePantalla) {
    // 1. Ocultar todas las pantallas
    screens.forEach(screen => {
        screen.classList.remove('active');
    });

    // 2. Mostrar la pantalla objetivo
    const targetScreen = document.getElementById(`screen-${nombrePantalla}`);
    if (targetScreen) {
        targetScreen.classList.add('active');
        console.log(`[UI] Pantalla activa: ${nombrePantalla}`);
    } else {
        console.error(`[UI Error] La pantalla screen-${nombrePantalla} no existe.`);
    }
}

/**
 * Actualiza la interfaz del proceso de llenado en tiempo real.
 * * @param {number} litrosActuales - Valor reportado por el NodeMCU vía Firebase.
 * @param {number} litrosObjetivo - Capacidad del bidón seleccionado (6, 12, 20).
 */
export function actualizarProgresoLlenado(litrosActuales, litrosObjetivo) {
    // Formatear a un decimal para evitar parpadeos visuales con números muy largos
    const litrosFormateados = parseFloat(litrosActuales).toFixed(1);
    
    litrosActualesEl.textContent = litrosFormateados;
    litrosObjetivoEl.textContent = litrosObjetivo;

    // Calcular porcentaje para la barra (Limitado al 100% por seguridad visual)
    let porcentaje = (litrosActuales / litrosObjetivo) * 100;
    if (porcentaje > 100) porcentaje = 100;
    
    progressBar.style.width = `${porcentaje}%`;
}

/**
 * Inyecta dinámicamente el código QR generado por la pasarela de pagos.
 * * @param {string} urlImagen - URL o Base64 del código QR.
 */
export function actualizarQR(urlImagen) {
    if (urlImagen) {
        qrImage.src = urlImagen;
    }
}

/**
 * Vincula el evento táctil/click de los bidones a la lógica principal.
 * * @param {Function} callbackSeleccion - Función a ejecutar cuando el usuario elige un bidón.
 */
export function inicializarEventosTarjetas(callbackSeleccion) {
    cards.forEach(card => {
        // En tablets, 'click' funciona perfectamente, pero podríamos usar 'touchend' si hay delay
        card.addEventListener('click', () => {
            const litrosSeleccionados = card.getAttribute('data-litros');
            callbackSeleccion(litrosSeleccionados);
        });
    });
}
