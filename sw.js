// Service worker do Stumix: guarda o app para funcionar offline.
// Ao publicar uma versão nova, troque VERSAO para os usuários receberem a atualização.
const VERSAO = 'estudio-v24';
const APP = [
  './',
  './index.html',
  './manifest.webmanifest',
  './lame.min.js',
  './icons/icon-192-v2.png',
  './icons/icon-512-v2.png',
  './icons/maskable-512-v2.png',
  './icons/apple-touch-icon-v2.png',
  './icons/favicon-32-v2.png',
  './fonts/manrope.woff2',
  './fonts/jetbrains-mono-500.woff2',
  './fonts/unbounded-800-stumix.woff2'
];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(VERSAO).then(c => c.addAll(APP)));
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys()
      .then(ks => Promise.all(ks.filter(k => k !== VERSAO).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', e => {
  const req = e.request;
  if (req.method !== 'GET') return;
  const url = new URL(req.url);

  if (url.origin !== self.location.origin) return;

  // Página: tenta a rede (pega versão nova), mas não espera mais que 1,2 s: com rede lenta abre a cópia guardada
  // só a página do app usa a cópia guardada; outras páginas do site (ex.: /baixar/) vão direto à rede
  const ehApp = url.pathname.endsWith('/estudio/') || url.pathname.endsWith('/estudio/index.html');
  if (req.mode === 'navigate' && ehApp) {
    const rede = fetch(req).then(r => { if (r.ok) { const cp = r.clone(); caches.open(VERSAO).then(c => c.put('./index.html', cp)); } return r; });
    e.respondWith(new Promise(resolve => {
      let feito = false;
      const usarCache = () => caches.match('./index.html').then(h => { if (h && !feito) { feito = true; resolve(h); } });
      const t = setTimeout(usarCache, 1200);
      rede.then(r => { clearTimeout(t); if (!feito) { feito = true; resolve(r); } })
          .catch(() => { clearTimeout(t); caches.match('./index.html').then(h => { if (!feito) { feito = true; resolve(h || Response.error()); } }); });
    }));
    return;
  }
  // Demais arquivos do app: cache primeiro
  e.respondWith(caches.match(req, { ignoreSearch: true }).then(hit => hit || fetch(req)));
});
