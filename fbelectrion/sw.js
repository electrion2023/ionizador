// Actualiza el nombre del caché para asegurar que los usuarios obtengan la nueva versión
const CACHE_NAME = 'electrion-v1'; 

const urlsToCache = [
  '/', // La raíz (index.html)
  '/index.html',
  '/fondo.jpg',
  '/icon-192.png',
  '/icon-512.png',
  '/manifest.json' // Añadir el manifest.json
];

self.addEventListener('install', (event) => {
  console.log('Service Worker de Electrion instalado');
  // Usamos el nuevo nombre de caché
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      // Usamos la lista de URLs corregida
      return cache.addAll(urlsToCache).catch(err => {
        console.warn('⚠️ Error al cachear archivos:', err);
      });
    })
  );
});

// La lógica 'fetch' se mantiene igual, ya que es genérica para servir archivos desde el caché o la red
self.addEventListener('fetch', event => {
  event.respondWith(
    caches.match(event.request).then(response => {
      return response || fetch(event.request);
    })
  );
});
