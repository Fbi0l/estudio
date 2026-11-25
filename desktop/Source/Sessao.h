// Stumix Desktop: multipista (faixas, gravação por cima, plugins VST3)
// Copyright (C) 2026 Fbiol. Licença GPLv3 (ver LICENSE na raiz do repositório).
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <map>
#include <vector>

/*  Uma sessão é um conjunto de faixas numa mesma linha do tempo.
    - Tocar: cada faixa passa pela sua cadeia de plugins, volume e pan, e soma na saída.
    - Gravar: as faixas armadas recebem a entrada da placa (entrada 1, 2 ou as duas em estéreo);
      enquanto armadas, tocam a entrada pela cadeia de plugins (ouvir o simulador de amp ao vivo).
    - Alinhamento: a gravação começa no mesmo bloco em que a reprodução começa, e o atraso de ida e
      volta da placa (entrada + saída + ajuste) é descontado, para a voz cair no tempo da base.
    A thread de áudio só tenta pegar a trava; se a tela estiver mexendo nas faixas, aquele bloco
    sai sem a sessão (silêncio), sem esperar. */
struct Faixa
{
    int id = 0;
    juce::String nome;
    juce::AudioBuffer<float> audio;          // 1 ou 2 canais, na taxa da sessão
    juce::int64 inicio = 0;                  // posição na linha do tempo, em amostras
    float volume = 1.0f, pan = 0.0f;
    bool mudo = false, solo = false, armada = false, monitorar = false;   // ouvir a entrada: só quando a pessoa liga (evita microfonia)
    int entrada = 0;                         // 0 = entrada 1, 1 = entrada 2, 2 = entradas 1+2 (estéreo)
    juce::OwnedArray<juce::AudioPluginInstance> plugins;
    std::vector<bool> ligado;
    juce::Array<juce::PluginDescription> descricoes;   // como cada plugin foi carregado (para reabrir a sessão)
    juce::AudioBuffer<float> trab;           // espaço de trabalho do bloco (alocado no preparo)
    std::vector<float> picos;                // pico de cada centésimo de segundo, para a tela
    juce::String arquivo;                    // nome do WAV na pasta da sessão
    juce::int64 fim() const { return inicio + audio.getNumSamples(); }
};

class Sessao
{
public:
    Sessao();
    ~Sessao();

    // ---- chamado pela thread de áudio (AudioEngine) ----
    void preparar (double taxa, int blocoMax);
    void processar (const float* inL, const float* inR, float* const* out, int nOut, int n);
    std::atomic<bool> pedidoGravar { false };          // a gravação começa no próximo bloco
    void comecarGravacaoNoBloco();                      // chamado dentro do bloco, antes de tocar

    // ---- linha do tempo ----
    void tocar (double segundos);
    void posicionar (double segundos);
    void parar();
    bool tocando() const { return tocandoFlag.load(); }
    double posicao() const { return taxa > 0 ? (double) pos.load() / taxa : 0.0; }
    juce::int64 inicioGravacao() const { return inicioGrav.load(); }

    // ---- gravação: as faixas armadas recebem o que a placa captou ----
    // latencia = atraso de ida e volta em amostras (já com o ajuste manual)
    int receberGravacao (const std::vector<float>& L, const std::vector<float>& R, int latencia);

    // ---- faixas ----
    int novaFaixa (const juce::String& nome, int entrada, bool armada);
    juce::String importar (const juce::File& arquivo);  // erro ou vazio
    bool remover (int id);
    bool ajustar (int id, const juce::var& props);       // volume, pan, mudo, solo, armada, entrada, monitorar, nome, moverMs
    juce::var estado() const;
    juce::var picos (int id) const;
    void colocarAudio (int id, juce::AudioBuffer<float>&& audio, juce::int64 inicio);   // para testes
    const juce::AudioBuffer<float>* audioDe (int id) const { auto* f = achar (id); return f ? &f->audio : nullptr; }
    juce::int64 inicioDe (int id) const { auto* f = achar (id); return f ? f->inicio : 0; }
    juce::AudioPluginInstance* pluginDe (int id, int slot) const { auto* f = achar (id); return f && juce::isPositiveAndBelow (slot, f->plugins.size()) ? f->plugins[slot] : nullptr; }
    int primeiraFaixa() const { return faixas.isEmpty() ? 0 : faixas[0]->id; }

    // ---- plugins ----
    void buscarPlugins (bool forcar);                    // em segundo plano
    juce::var listaPlugins() const;
    juce::String adicionarPlugin (int id, int indice);   // índice na lista de plugins
    juce::String adicionarPluginDeArquivo (int id, const juce::File& arquivo);   // usado nos testes
    bool removerPlugin (int id, int slot);
    bool ligarPlugin (int id, int slot, bool ligar);
    bool abrirPlugin (int id, int slot);

    // ---- mixagem (fora do tempo real) ----
    juce::String exportar (const juce::File& destino, double& duracao);

    // ---- sessões salvas ----
    void usarPasta (const juce::File& raiz);             // onde ficam as sessões
    juce::var listarSessoes() const;
    juce::String abrir (const juce::String& nome);
    juce::String nova();
    void salvar();                                        // grava sessao.json (os WAVs já ficam salvos)
    juce::String nomeAtual() const { return nomeSessao; }
    double taxaSessao() const { return taxa; }

private:
    void prepararFaixa (Faixa&);
    void calcularPicos (Faixa&);
    void gravarWav (Faixa&);
    Faixa* achar (int id) const;
    void processarCadeia (Faixa&, int n);
    juce::String criarPlugin (Faixa&, const juce::PluginDescription&, const juce::String& estado, bool ligar);

    mutable juce::CriticalSection trava;
    juce::OwnedArray<Faixa> faixas;
    int proximoId = 1;
    double taxa = 48000.0;
    int bloco = 512;
    std::atomic<juce::int64> pos { 0 }, inicioGrav { 0 };
    std::atomic<bool> tocandoFlag { false };

    juce::AudioPluginFormatManager formatos;
    juce::KnownPluginList conhecidos;
    std::atomic<bool> buscando { false };
    std::unique_ptr<juce::Thread> busca;
    juce::File pastaRaiz, pastaSessao;
    juce::String nomeSessao;
    std::map<juce::AudioProcessor*, std::unique_ptr<juce::DocumentWindow>> janelas;
    juce::MidiBuffer midiVazio;
};
