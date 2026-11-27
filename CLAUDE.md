# Stumix (antes "Estúdio")

App de gravação, edição e finalização de áudio. PWA em https://fbi0l.github.io/estudio/ (ramo `main`) e Stumix Desktop para Windows (JUCE, pasta `desktop/`, ramo `desktop`; versão em `desktop/CMakeLists.txt`; o workflow publica a versão nova em Releases sozinho).

## Combinados com o Fabio
- Pode publicar sempre que alterar algo, depois de testar (25/11/2026).
- Gravação sempre bruta, sem cortes; edição depois.
- Português do Brasil, explicações simples.

## Pendências
Ficam nas Issues do GitHub. Página de download pronta em baixar/ (issue #1 fechada). Multipista no celular pronta (1.14.0, motor `MPW` no index.html com as mesmas funções `mp*` do PC; `mpc()` escolhe o motor). Efeitos por faixa pela opção A ("Editar com efeitos" → Devolver à faixa; nativo: `mpFaixaWav`, `mpReceber` em pedaços base64, `mpSubstituir`). Próximas: guia e "Sobre" com a aba Faixas; opção B (efeitos ao vivo por faixa) se o Fabio pedir.

## Visual
Tema "palco": fundo `#0E0F0C`, superfície `#16181A`, destaque verde-limão `#C8F169`, vermelho só na gravação, vidro nas barras (`--glass*` em `:root` do index.html). Fontes em `fonts/` (Manrope, JetBrains Mono, Unbounded só "stumix"). Logo: S de cabo de guitarra com plugue reto inclinado 15°.

## Testes
- Motor do PC: `EngineTest` (com o plugin VST3 de teste `StumixTesteGanho`); interface ponta a ponta: `desktop/autoteste.js`. Ambos rodam no Windows a cada envio.
- Faixas no navegador: `/home/claude/t/mpweb.py` (fluxo) e `mploop.py` (cabo de retorno falso de 80 ms: mede o atraso e confere que a faixa gravada cai em cima da base, 0 ms de diferença).
- Placa que desconecta: teste "placa que desconecta" no EngineTest (placa falsa `TipoFalso`); `mp_mock.py` confere o aviso e a parada da gravação.
- Erro repetido (2x): nunca percorrer `x().getArray()` de um `var` temporário (o array morre no fim da linha; no Linux passa por sorte, no Windows dá zero). Guardar antes: `const var v = x();`.
- Erro conhecido: texto JS→nativo perde acento no Linux; nomes vão com `encodeURIComponent` e o C++ usa `URL::removeEscapeChars`; textos com acento no C++ sempre via `String::fromUTF8`.
