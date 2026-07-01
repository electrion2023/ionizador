/**
 * ============================================================================
 * MÓDULO: COUNTDOWN (CUENTA REGRESIVA)
 * ============================================================================
 * Gestiona los temporizadores visuales en pantalla, específicamente
 * para procesos físicos como el lavado del bidón.
 */

const countdownEl = document.getElementById('countdown-lavado');
let intervalo = null;

/**
 * Inicia una cuenta regresiva visual en la pantalla actual.
 * @param {number} segundos - Segundos totales de la cuenta regresiva.
 * @param {Function} [onComplete] - Función opcional a ejecutar al llegar a cero.
 */
export function iniciarCuentaRegresiva(segundos, onComplete = null) {
    limpiarCuentaRegresiva(); // Asegurar que no hay intervalos superpuestos

    let tiempoRestante = segundos;
    countdownEl.textContent = tiempoRestante;

    intervalo = setInterval(() => {
        tiempoRestante--;
        countdownEl.textContent = tiempoRestante;

        if (tiempoRestante <= 0) {
            limpiarCuentaRegresiva();
            if (onComplete) onComplete();
        }
    }, 1000);
}

/**
 * Detiene y limpia la cuenta regresiva actual.
 */
export function limpiarCuentaRegresiva() {
    if (intervalo) {
        clearInterval(intervalo);
        intervalo = null;
    }
    countdownEl.textContent = "0";
}
