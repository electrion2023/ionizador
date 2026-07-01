/**
 * ============================================================================
 * MÓDULO: FIREBASE CONEXIÓN (SDK Modular v9+)
 * ============================================================================
 * Inicializa la conexión y expone funciones de lectura/escritura.
 * Diseñado para no bloquear el hilo principal y mantener listeners vivos.
 */

import { initializeApp } from "https://www.gstatic.com/firebasejs/10.8.1/firebase-app.js";
import { getDatabase, ref, onValue, set, get } from "https://www.gstatic.com/firebasejs/10.8.1/firebase-database.js";
// Preparado para Auth en el futuro según requerimiento
import { getAuth } from "https://www.gstatic.com/firebasejs/10.8.1/firebase-auth.js";

// TODO: Reemplazar con las credenciales reales del proyecto
const firebaseConfig = {
  apiKey: "AIzaSyBTsbxqJ0mVAi4DVp_vqZmJehvQspBo-S8",
  authDomain: "pagos-58644.firebaseapp.com",
  databaseURL: "https://pagos-58644-default-rtdb.firebaseio.com",
  projectId: "pagos-58644",
  storageBucket: "pagos-58644.firebasestorage.app",
  messagingSenderId: "118987640941",
  appId: "1:118987640941:web:5ffc5c075d2051ef1bef64"
};

// Inicialización
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);
const auth = getAuth(app); // Listo para implementar validación de técnicos/admin

/**
 * Establece un listener en tiempo real sobre un nodo específico.
 * @param {string} path - Ruta del nodo en Realtime Database (ej: 'dispositivo/estado/paso').
 * @param {Function} callback - Función que se ejecuta cada vez que el valor cambia.
 */
export function escucharNodo(path, callback) {
    const nodoRef = ref(db, path);
    onValue(nodoRef, (snapshot) => {
        const data = snapshot.val();
        callback(data);
    }, (error) => {
        console.error(`[Firebase Error] Fallo al escuchar el nodo ${path}:`, error);
    });
}

/**
 * Escribe un valor en un nodo específico de forma asíncrona.
 * @param {string} path - Ruta del nodo a sobreescribir.
 * @param {any} valor - El dato a escribir (string, número, booleano u objeto).
 */
export async function escribirNodo(path, valor) {
    const nodoRef = ref(db, path);
    try {
        await set(nodoRef, valor);
        console.log(`[Firebase Success] Nodo ${path} actualizado a:`, valor);
    } catch (error) {
        console.error(`[Firebase Error] Fallo al escribir en ${path}:`, error);
    }
}

/**
 * Lee un nodo una sola vez (Ideal para configuraciones iniciales o precios).
 * @param {string} path - Ruta del nodo a leer.
 * @returns {Promise<any>} - El valor contenido en el nodo.
 */
export async function leerNodoUnaVez(path) {
    const nodoRef = ref(db, path);
    try {
        const snapshot = await get(nodoRef);
        return snapshot.exists() ? snapshot.val() : null;
    } catch (error) {
        console.error(`[Firebase Error] Fallo lectura única en ${path}:`, error);
        return null;
    }
}
