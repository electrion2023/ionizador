// SW CORREGIDO - electron-fbelectrion-v4
const CACHE_NAME = 'electrion-fbelectrion-v4';

// RUTAS RELATIVAS CORRECTAS
const urlsToCache = [
  './',                    // index.html actual
  './index.html',
  './timer.html', 
  './tips.html',
  './settings.html',
  './fondo.jpg',
  './icon-192.png',
  './icon-512.png',
  './manifest.json'
];

self.addEventListener('install', (event) => {
  console.log('Service Worker de Electrion (v4) instalado - RUTAS CORREGIDAS');
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      return cache.addAll(urlsToCache).catch(err => {
        console.warn('⚠️ Error al cachear archivos:', err);
      });
    })
  );
});

self.addEventListener('activate', event => {
  const cacheWhitelist = [CACHE_NAME];
  event.waitUntil(
    caches.keys().then(cacheNames => {
      return Promise.all(
        cacheNames.map(cacheName => {
          if (cacheWhitelist.indexOf(cacheName) === -1) {
            console.log('Eliminando caché antiguo:', cacheName);
            return caches.delete(cacheName);
          }
        })
      );
    })
  );
  // Forzar control inmediato sobre los clients
  return self.clients.claim();
});

self.addEventListener('fetch', event => {
  // Solo manejar solicitudes de la misma origen
  if (event.request.url.startsWith(self.location.origin)) {
    event.respondWith(
      caches.match(event.request).then(response => {
        // Devuelve del cache o haz fetch
        return response || fetch(event.request);
      })
    );
  }
  // Para solicitudes de terceros (como Firebase), dejar pasar normalmente
});
