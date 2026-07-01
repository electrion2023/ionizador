import "https://www.gstatic.com/firebasejs/8.10.0/firebase-app.js";
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

// Inicializar Firebase (usando el namespace global firebase)
firebase.initializeApp(firebaseConfig);
const db = firebase.database();

export function escucharNodo(path, callback) {
    const nodoRef = db.ref(path);
    nodoRef.on('value', (snapshot) => {
        const data = snapshot.val();
        console.log("Firebase Recibido:", data);
        callback(data);
    }, (error) => {
        console.error("Error Firebase:", error);
    });
}

export function escribirNodo(path, valor) {
    db.ref(path).set(valor);
}

// Corregido: Ahora usa la sintaxis v8 (.once)
export async function leerNodoUnaVez(path) {
    try {
        const snapshot = await db.ref(path).once('value');
        return snapshot.exists() ? snapshot.val() : null;
    } catch (error) {
        console.error(`Error leyendo ${path}:`, error);
        return null;
    }
}
