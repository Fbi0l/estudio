# Stumix

App para gravar, editar e finalizar áudio: locução, podcast, voz ou instrumento, inclusive por placa de áudio ou pedaleira de guitarra. Tem assistente de gravação (teste da sala e do nível, avisos de estouro, regravar trecho), edição, efeitos, acabamento no volume das plataformas (LUFS) e exportação em MP3 ou WAV. Site estático (HTML, CSS e JavaScript, sem etapa de build), instalável como app (PWA) e funciona offline. Tudo roda no aparelho: nenhum áudio é enviado para a internet.

No ar em https://fbi0l.github.io/estudio/ (GitHub Pages). Como publicar e instalar: `LEIA-ME.md`.

**Toda versão nova:** suba `APP_VERSION` e `GUIA_REVISADO_NA_VERSAO` no `index.html`, a `VERSAO` do `sw.js` (para os apps instalados receberem a atualização), revise os passos do guia e registre aqui.

## Versão 1.14.0 — Várias faixas no celular (Stumix Desktop 0.7.2)

- A aba **Faixas** agora funciona no celular e no navegador: importe uma base ou back track, grave por cima, grave outras faixas e mixe. Tudo fica salvo no aparelho.
- A gravação começa na mesma amostra em que a base começa a tocar, e o atraso do aparelho é cortado do começo para a faixa ficar no tempo.
- Botão **Medir** (Faixas → Sessão): o alto-falante toca 6 estalos, o microfone escuta e o app descobre o atraso real do aparelho.
- "Ouvir a entrada no fone" por faixa armada.
- No celular, as faixas mostram nome e botões ● M S; volume e pan ficam no painel da faixa.
- Ao parar uma gravação, o cursor volta para onde ela começou, pronto para ouvir.
- PC: o botão tocar da aba Faixas funciona mesmo sem áudio aberto em Gravar.

## Versão 1.13.0 — Página de download para Windows

- Nova página https://fbi0l.github.io/estudio/baixar/ com o botão "Baixar para Windows" (sempre a última versão), a versão portátil e o que fazer nos avisos do Windows.
- No navegador do computador com Windows, o app mostra "Baixar" em ⓘ e um link no rodapé.
- O app instalado no celular não guarda mais outras páginas do site como se fossem a tela do app.

## Versão 1.12.0 — Auditoria: janela sem travar, proteção e acessibilidade (Stumix Desktop 0.7.1)

- PC: abrir sessão, importar base e mixar rodam em segundo plano, com aviso de progresso; a janela não trava.
- PC: funções usadas só pelo teste automático ficam desligadas no uso normal; "abrir link" só aceita https.
- Aba Faixas: o topo mostra a sessão, o botão do topo vira "Mixar", Desfazer/Refazer ficam inativos; setas do teclado escolhem a faixa e movem o cursor.
- Botão Gravar com contraste 4,7:1; título de nível 1 e áreas marcadas para leitores de tela (teste axe sem problemas).

## Versão 1.11.0 — Abertura rápida (Stumix Desktop 0.6.0)

- Animação de abertura completa só na primeira vez do dia; nas outras, a logo some em cerca de meio segundo.
- Fontes guardadas dentro do app (Manrope, JetBrains Mono, Unbounded; licença OFL em fonts/): a tela aparece sem esperar a internet.
- Com rede lenta, o app instalado abre a cópia guardada depois de 1,2 s, em vez de esperar a rede.
- PC: a placa abre uma vez só, já com a última configuração (placa.xml); drivers procurados só quando preciso; a última sessão da multipista carrega depois que a janela aparece.
- PC: o tempo de abertura aparece em ⓘ (Modo) e fica anotado em %APPDATA%\Estudio\abertura.log.
- PC 0.5.1: barra de título escura; botões das listas sem texto vazando.

## Versão 1.10.0 — Multipista no PC (Stumix Desktop 0.5.0)

- Aba **Faixas** (só no PC): várias faixas numa linha do tempo, cada uma com volume, pan, mudo e solo.
- **Gravar por cima**: arme (●) uma ou mais faixas e grave enquanto as outras tocam — a voz por cima da base que acabou de gravar ou de um back track importado (WAV, MP3, FLAC, OGG, AIFF; outra taxa é convertida).
- **Duas entradas ao mesmo tempo**: cada faixa armada escolhe a entrada 1, a 2 ou as duas em estéreo.
- **Alinhamento**: a gravação começa no mesmo bloco em que a reprodução começa e o atraso de ida e volta da placa é descontado; há ajuste fino em ms e "mover no tempo" por faixa.
- **Plugins VST3** por faixa, em cadeia: liga/desliga, janela do plugin, ajustes salvos com a sessão; com "ouvir a entrada pelos plugins", a faixa armada toca ao vivo pelo simulador de amp.
- **Sessões** salvas sozinhas no computador (WAV 32 bits por faixa + sessao.json).
- **Mixar e finalizar**: a mixagem (com plugins) é feita fora do tempo real e abre em Finalizar para o volume final e a exportação.
- Testes: motor (tocar, gravar por cima com atraso simulado, duas entradas, volume/pan, importar com outra taxa, mixagem, sessão, plugin VST3 de teste) e ponta a ponta na interface.

## Versão 1.9.1 — Ícone da tela inicial

- Arquivos de ícone com nome novo (-v2), para o Android e o PC perceberem a troca.
- Em ⓘ, no app instalado: como o ícone novo aparece em cada sistema (no iPhone, só removendo e adicionando de novo).

## Versão 1.9.0 — Animação de abertura

- Ao abrir, o cabo do S se desenha, o plugue encaixa e o nome sobe (cerca de 2 s); o app carrega por baixo.
- Um toque ou uma tecla pula a abertura; com "menos movimento" no sistema, só a logo aparece.
- O guia da primeira vez abre depois da abertura.

## Versão 1.8.0 — Identidade visual

- Logo nova: o S é um cabo de guitarra com o plugue na ponta, em verde-limão sobre fundo escuro; nome em Unbounded.
- Ícones novos do app (celular, PC e aba do navegador).
- Tema "palco": fundo #0E0F0C, destaque verde-limão #C8F169 (onda, botões, aba ativa), vermelho só na gravação.
- Barras e janelas de vidro fosco; abas com uma pílula que desliza até a aba tocada; no celular, a barra de abas flutua.
- Sem vidro quando o sistema pede menos transparência; sem animação quando pede menos movimento.

## Versão 1.7.0 — Novo nome: Stumix

- O app passa a se chamar **Stumix** (stu de estúdio, mix de mixagem). Nome conferido no INPI em 24/11/2026: sem registro ou pedido igual.
- Endereço, projetos salvos e preferências continuam os mesmos; nada precisa ser refeito.
- Interface pronta para o Stumix Desktop (motor nativo no Windows).

## Versão 1.6.3 — Diagnóstico de qualidade do sinal

- Teste do microfone mede até onde vão os agudos e avisa quando algo no caminho abafa o som (voz) ou informa o limite (guitarra com simulador de caixa).
- Depois de gravar, separa queda de conexão (som some por mais de 1,5 s) de gate (cortes curtos) e aponta a causa.
- Dica da Joyo Momix CAB: corta os agudos; para gravar, ligar a pedaleira direto na USB.

## Versão 1.6.2 — Novo motor de captura

- A gravação e o teste do microfone usam **AudioWorklet**, que roda numa linha de áudio própria, separada da tela. Com a tela sobrecarregada, o teste caiu de 35 cortes (motor antigo) para nenhum além do início.
- Navegadores sem AudioWorklet continuam gravando pelo motor antigo, automaticamente. A janela ⓘ mostra qual está em uso.

## Versão 1.6.1 — Gravação crua protegida

- O teste do microfone avisa quando um noise gate fora do app (Windows ou pedaleira) zera o som nas pausas; a gravação avisa quando houve cortes entre as notas ou palavras.
- Pede ao navegador para não aplicar isolação de voz (gravação crua).
- Efeitos → Utilidades: **Aparar silêncio das pontas**.
- Exportar avisa quando o volume está baixo para publicar e oferece o acabamento antes.
- Reconhece a Joyo Momix CAB com dicas próprias; esconde a entrada duplicada "Comunicações" do Windows.

## Versão 1.6.0 — Projetos salvos no aparelho

- Cada trabalho vira um **projeto salvo automaticamente** no próprio aparelho (IndexedDB), 1,2 s depois de cada mudança. O histórico de desfazer não é guardado.
- Ao abrir o app, o **último projeto volta sozinho**.
- Menu **☰** (canto superior esquerdo): novo projeto, abrir, apagar (com confirmação) e espaço usado no aparelho.
- Antes de reiniciar para atualizar, o app salva o projeto. Primeiro passo da multipista.
- Guia, Sobre o app e justificativa atualizados.

## Versão 1.5.0 — Guia de uso e Sobre o app

- **Como usar o app** (botão "?" no topo): guia em 9 passos com todas as funções. Abre sozinho no primeiro acesso; depois, pelo "?", pelo ⓘ ou pelo rodapé.
- **Sobre o app**: por que o app foi criado, para que serve, como funciona, os benefícios, a privacidade e os custos, no padrão de numeração da ABNT.
- Versão e "Sobre o app" no rodapé do painel Gravar e na janela ⓘ.

## Versão 1.4.0 — Atualização pelo app

- Janela ⓘ: versão em uso, **Buscar atualização** (baixa e oferece Reiniciar, com aviso se houver áudio aberto) e instalação. Bolinha no ⓘ quando há versão nova.
- Corrigido: o botão "Instalar app" aparecia mesmo com o app instalado.

## Versão 1.3.0 — Placas de áudio e pedaleiras

- Reconhece o equipamento pelo nome (Cuvave/M-Vave, Joyo, outras pedaleiras e interfaces) e mostra dicas para cada um.
- Fonte (voz, instrumento ou pedaleira), Canal (automático, entrada 1, entrada 2 ou estéreo), retorno no fone pelo canal certo com atraso estimado, alerta de Bluetooth.

## Versão 1.2.0 — Visual de app

- Tela cheia com barra de título, ferramentas, onda central, painel com abas e transporte embaixo (PC); abas na base e exportação em folha (celular).

## Versão 1.1.0 — Celular

- Layout para celular, transporte fixo ao alcance do polegar.

## Versão 1.0.0 — Primeira versão

- Gravação sem perdas com assistente, edição, efeitos, masterização por LUFS, exportação WAV/MP3, instalável e offline.

---

# Stumix Desktop (Windows) — em desenvolvimento

App instalável para Windows com a mesma interface do Stumix e um **motor de áudio nativo** (JUCE 8): drivers ASIO e WASAPI (compartilhado, baixa latência e exclusivo), escolha de buffer com o atraso real, retorno direto no fone e gravação em 32 bits sem perdas, sem passar pelo mixer do Windows. O celular continua no PWA.

- Código em `desktop/`: `Source/AudioEngine.*` (motor), `Source/Main.cpp` (janela, interface embutida e ponte JS ↔ C++), `Source/EngineTest.cpp` (teste do motor sem placa), `autoteste.js` (teste de ponta a ponta com placa simulada), `gerar_web.py` (cópia da interface com acentos em códigos ASCII).
- Compilação e instalador: GitHub Actions (`.github/workflows/desktop.yml`), que também roda os dois testes no Windows. Ao subir a versão em `desktop/CMakeLists.txt` no ramo `desktop`, o workflow publica sozinho a versão `desktop-vX.Y.Z` com o instalador em Releases.
- Atualização: ⓘ → Buscar atualização consulta a última versão `desktop-v*` em Releases e, se houver uma nova, já baixa o instalador e o roda em modo silencioso (um clique, sem abrir site); o app salva o projeto, fecha, atualiza e reabre. Desde a 0.2.1.
- Nome antigo (Estúdio): o instalador mantém o mesmo AppId, remove o `Estudio.exe` e os atalhos antigos, e a pasta de dados `%APPDATA%\Estudio` foi mantida para não perder projetos.
- Fases: **A** motor nativo (esta) · **B** plugins VST3 · **C** multipista no motor nativo.

**Licenças:** o Stumix é GPLv3 (`LICENSE`). Usa JUCE 8 (AGPLv3/licença Starter), VST3 SDK (MIT), ASIO SDK (GPLv3) e lamejs (LGPL-3.0).
