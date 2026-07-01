/**
 * ============================================================================
 * MÓDULO: VIDEOS
 * ============================================================================
 * Centraliza el control de reproducción de los recursos multimedia.
 * Maneja eventos de finalización para informar a la máquina de estados
 * y previene bloqueos por políticas de autoplay del navegador.
 */

// Mapeo estático de los elementos de video en el DOM
const videoElements = {
    standby: document.getElementById('video-standby'),
    intro: document.getElementById('video-intro'),
    lavado: document.getElementById('video-lavado'),
    girar: document.getElementById('video-girar'),
    gracias: document.getElementById('video-gracias')
};

/**
 * Reproduce un video desde el principio y ejecuta una función al terminar.
 * @param {string} nombre - Identificador del video (coincide con la clave en videoElements).
 * @param {Function} [onEndedCallback] - Función a ejecutar cuando el video termine.
 */
export function reproducirVideo(nombre, onEndedCallback = null) {
    const video = videoElements[nombre];
    
    if (!video) {
        console.error(`[Videos Error] No se encontró el video: ${nombre}`);
        // Si el video no existe pero hay un callback, lo ejecutamos para no romper el flujo
        if (onEndedCallback) onEndedCallback();
        return;
    }

    // 1. Reiniciar el video al inicio (importante para usos recurrentes)
    video.currentTime = 0;

    // 2. Limpiar listeners previos para evitar fugas de memoria y ejecuciones múltiples
    video.onended = null;

    // 3. Asignar el nuevo evento de finalización si existe un callback
    if (onEndedCallback) {
        video.onended = () => {
            video.onended = null; // Limpiar inmediatamente después de disparar
            console.log(`[Videos] Video terminado: ${nombre}`);
            onEndedCallback();
        };
    }

    // 4. Iniciar reproducción manejando la Promesa asíncrona
    video.play().catch(error => {
        console.warn(`[Videos Warning] Autoplay bloqueado o error en '${nombre}':`, error);
        
        // Fallback de seguridad vital: Si el video falla al reproducir (ej. falta el archivo),
        // disparamos el callback de todas formas para que la máquina de estados avance al siguiente paso.
        if (onEndedCallback) {
            console.warn(`[Videos] Forzando avance automático por fallo de reproducción.`);
            onEndedCallback();
        }
    });
}

/**
 * Pausa todos los videos y los devuelve al inicio.
 * Útil cuando se cancela una operación o se fuerza un reseteo de la máquina.
 */
export function detenerTodos() {
    Object.values(videoElements).forEach(video => {
        if (video) {
            video.pause();
            video.currentTime = 0;
            video.onended = null;
        }
    });
}
