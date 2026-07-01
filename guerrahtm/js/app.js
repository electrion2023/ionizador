/**
 * ============================================================================
 * MÓDULO: APP (ENTRY POINT)
 * ============================================================================
 * Punto de entrada único. Inicializa la conexión con Firebase y los 
 * eventos de la interfaz de usuario.
 */

import { escucharNodo } from './firebase.js';
import { procesarCambioDePaso, setBidonSeleccionado, procesarEstadoPago, actualizarCaudal } from './estados.js';
import { inicializarEventosTarjetas, mostrarPantalla } from './ui.js';
import { reproducirVideo } from './videos.js';

// Ejecutar cuando el DOM esté completamente listo
document.addEventListener('DOMContentLoaded', () => {
    console.log("[App] Sistema HMI iniciando...");

    // 1. Mostrar pantalla de StandBy y reproducir el video loop
    mostrarPantalla('standby');
    reproducirVideo('standby');

    // 2. Inicializar los eventos de los botones (Tarjetas de bidones)
    inicializarEventosTarjetas((litros) => {
        setBidonSeleccionado(parseInt(litros));
    });

    // 3. Listeners de Firebase (Time-Sensitive updates)
    
    // Escuchar el estado general (paso) del dispositivo
    escucharNodo('dispositivo/estado/paso', (nuevoPaso) => {
        procesarCambioDePaso(nuevoPaso);
    });

    // Escuchar cambios en el pago (procesado por el webhook externo)
    escucharNodo('dispositivo/estado/pagado', (esPagado) => {
        procesarEstadoPago(esPagado);
    });

    // Escuchar litros en tiempo real (para el llenado)
    escucharNodo('dispositivo/estado/litros', (litrosActuales) => {
        actualizarCaudal(litrosActuales);
    });

    // Deshabilitar menú contextual y gestos nativos (Protección extra para kiosco)
    window.oncontextmenu = (e) => e.preventDefault();
});
