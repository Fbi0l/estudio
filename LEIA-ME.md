# Stumix (PWA)

App de gravação, edição e finalização de áudio que roda no navegador e pode ser instalado como aplicativo. Funciona offline e nada sai do computador.

## Arquivos
- `index.html`: o app inteiro
- `manifest.webmanifest`: nome, ícones e tipos de arquivo que o app abre
- `sw.js`: service worker, que guarda o app para uso offline
- `lame.min.js`: codificador MP3 (lamejs 1.2.1, licença LGPL-3.0)
- `icons/`: ícones
- `_headers`: regras de cache para o Cloudflare Pages

## Publicar no Cloudflare Pages (sem Git)
1. dash.cloudflare.com → Workers & Pages → Create → Pages → **Upload assets**.
2. Dê um nome ao projeto (ex.: `estudio`) e arraste **o conteúdo desta pasta**.
3. Deploy. O app fica em `https://estudio.pages.dev` (ou no nome que você escolheu).

Também funciona em GitHub Pages, Netlify ou qualquer hospedagem com **https**.

## Instalar
- **Chrome/Edge no computador:** botão "Instalar app" no topo do Stumix ou ícone de instalar na barra de endereço. Depois de instalado, arquivos de áudio podem ser abertos pelo sistema com "Abrir com > Stumix".
- **Android (Chrome):** menu ⋮ → Instalar app.
- **iPhone/iPad (Safari):** Compartilhar → Adicionar à Tela de Início.

## Testar no seu computador antes de publicar
Na pasta do app: `python3 -m http.server 8765` e abra `http://localhost:8765`.
(Aberto direto como arquivo, o app funciona, mas não instala nem fica offline.)

## Publicar uma versão nova
Altere `const VERSAO = 'estudio-v1'` em `sw.js` (ex.: `estudio-v2`) e publique de novo. Os usuários recebem a atualização na próxima vez que abrirem o app.
