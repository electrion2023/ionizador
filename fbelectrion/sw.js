
const CACHE_NAME = 'reserva-oficinas-v2';
const urlsToCache = [
  '/',
  '/index.html',
  '/fondo.jpg',
  '/icon-192.png',
  '/icon-512.png'
];

self.addEventListener('install', (event) => {
  console.log('Service Worker instalado');
  event.waitUntil(
    caches.open('v1').then((cache) => {
      return cache.addAll([
        '/ios/index.html',
        '/ios/manifest.json',
        '/ios/icon-192.png',
        '/ios/icon-512.png',
      ]).catch(err => {
        console.warn('⚠️ Error al cachear archivos:', err);
      });
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
