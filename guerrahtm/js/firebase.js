/**
 * ============================================================================
 * MÓDULO: FIREBASE CONEXIÓN (SDK Modular v9+)
 * ============================================================================
 * Inicializa la conexión y expone funciones de lectura/escritura.
 * Diseñado para no bloquear el hilo principal y mantener listeners vivos.
 */
// Usamos el SDK v8 (más estable para tu configuración actual)
import * as firebase from "https://www.gstatic.com/firebasejs/8.10.0/firebase-app.js";
import "https://www.gstatic.com/firebasejs/8.10.0/firebase-database.js";

const firebaseConfig = {
    apiKey: "AIzaSyBTsbxqJ0mVAi4DVp_vqZmJehvQspBo-S8",
    authDomain: "pagos-58644.firebaseapp.com",
    databaseURL: "https://pagos-58644-default-rtdb.firebaseio.com",
    projectId: "pagos-58644",
    storageBucket: "pagos-58644.firebasestorage.app",
    messagingSenderId: "118987640941",
    appId: "1:118987640941:web:5ffc5c075d2051ef1bef64"
};

// Inicializar Firebase
firebase.initializeApp(firebaseConfig);
const db = firebase.database();

export function escucharNodo(path, callback) {
    const nodoRef = db.ref(path);
    nodoRef.on('value', (snapshot) => {
        const data = snapshot.val();
        console.log("Firebase Recibido:", data);
        callback(data);
    });
}

export function escribirNodo(path, valor) {
    db.ref(path).set(valor);
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
