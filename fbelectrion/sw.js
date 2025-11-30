// CAMBIO 1: Nuevo nombre de caché para forzar la actualización
const CACHE_NAME = 'electrion-fbelectrion-v3'; // Renombrado para evitar conflictos

// CAMBIO 2: Rutas corregidas para cachear los archivos DENTRO de la subcarpeta
const urlsToCache = [
  '/fbelectrion/', // La URL de la carpeta
  '/fbelectrion/index.html',
  '/fbelectrion/fondo.jpg',
  '/fbelectrion/icon-192.png',
  '/fbelectrion/icon-512.png',
  '/fbelectrion/manifest.json',
  '/fbelectrion/timer.html', // Asumo que quieres cachear las otras páginas
  '/fbelectrion/tips.html',
  '/fbelectrion/settings.html'
];

self.addEventListener('install', (event) => {
  console.log('Service Worker de Electrion (v3) instalado');
  event.waitUntil(
    // Abrir el nuevo caché
    caches.open(CACHE_NAME).then((cache) => {
      // Intentar cachear las nuevas URLs
      return cache.addAll(urlsToCache).catch(err => {
        console.warn('⚠️ Error al cachear archivos:', err);
      });
    })
  );
});

// Listener de activación para limpiar cachés antiguos (IMPORTANTE)
self.addEventListener('activate', event => {
  const cacheWhitelist = [CACHE_NAME];
  event.waitUntil(
    caches.keys().then(cacheNames => {
      return Promise.all(
        cacheNames.map(cacheName => {
          if (cacheWhitelist.indexOf(cacheName) === -1) {
            // Eliminar cachés antiguos
            return caches.delete(cacheName);
          }
        })
      );
    })
  );
});


self.addEventListener('fetch', event => {
  event.respondWith(
    caches.match(event.request).then(response => {
      return response || fetch(event.request);
    })
  );
});
