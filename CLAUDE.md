# Stumix (antes "Estúdio")

App de gravação, edição e finalização de áudio. PWA em https://fbi0l.github.io/estudio/ (ramo `main`) e Stumix Desktop para Windows (JUCE, pasta `desktop/`, ramo `desktop`; versão em `desktop/CMakeLists.txt`; o workflow publica a versão nova em Releases sozinho).

## Combinados com o Fabio
- Pode publicar sempre que alterar algo, depois de testar (25/11/2026).
- Gravação sempre bruta, sem cortes; edição depois.
- Português do Brasil, explicações simples.

## Pendências
Ficam nas Issues do GitHub. Próxima: página de download para Windows sem passar pelo GitHub (issue #1).

## Visual
Tema "palco": fundo `#0E0F0C`, superfície `#16181A`, destaque verde-limão `#C8F169`, vermelho só na gravação, vidro nas barras (`--glass*` em `:root` do index.html). Fontes em `fonts/` (Manrope, JetBrains Mono, Unbounded só "stumix"). Logo: S de cabo de guitarra com plugue reto inclinado 15°.

## Testes
- Motor do PC: `EngineTest` (com o plugin VST3 de teste `StumixTesteGanho`); interface ponta a ponta: `desktop/autoteste.js`. Ambos rodam no Windows a cada envio.
- Erro conhecido: texto JS→nativo perde acento no Linux; nomes vão com `encodeURIComponent` e o C++ usa `URL::removeEscapeChars`; textos com acento no C++ sempre via `String::fromUTF8`.
