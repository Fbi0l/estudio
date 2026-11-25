// Stumix Desktop: motor de áudio nativo
// Copyright (C) 2026 Fbiol. Licença GPLv3 (ver LICENSE na raiz do repositório).
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Sessao.h"
#include <atomic>
#include <vector>

/*  O áudio entra pela placa (thread de áudio), passa por uma fila sem travas e é
    consumido por uma thread própria, que grava, mede e calcula os picos para a tela.
    A thread de áudio só copia amostras e faz o retorno direto no fone: nada de
    alocação nem de espera ali dentro. */
class AudioEngine : public juce::AudioIODeviceCallback, private juce::Thread
{
public:
    AudioEngine();
    ~AudioEngine() override;

    juce::AudioDeviceManager deviceManager;

    void iniciar();                                   // abre o dispositivo padrão
    juce::var listar();                               // drivers e dispositivos
    juce::var estado();                               // configuração atual + atraso
    juce::String configurar (const juce::var& cfg);   // muda driver/entrada/saída/taxa/buffer

    // Retorno no fone: canal 0 = mistura das entradas 1 e 2, 1 = entrada 1, 2 = entrada 2, 3 = estéreo
    void setMonitor (bool ligado, int canal, float ganho);

    void iniciarGravacao (bool imediato = true);    // false: a multipista liga a captação no bloco certo
    void tomarGravacao (std::vector<float>& L, std::vector<float>& R);   // encerra e entrega o que foi captado
    int latenciaAmostras() const;                     // ida e volta da placa (entrada + saída)
    Sessao sessao;                                    // multipista
    juce::var pararGravacao();                        // { ok, frames, canais, taxa, duracao }
    const juce::MemoryBlock& wav() const              { return wavGravado; }

    void iniciarMedicao();                            // para o teste do microfone
    juce::var pararMedicao();                         // { espectro:[dB], hz }

    // Dados para a tela, lidos pelo temporizador da interface
    float pico (int canal) const                      { return picoAtual[(size_t) juce::jlimit (0, 1, canal)].load(); }
    bool gravando() const                             { return gravandoFlag.load(); }
    juce::int64 framesGravados() const                { return frames.load(); }
    juce::Array<juce::var> pegarPicosNovos();         // picos de 512 amostras desde a última chamada
    juce::Array<juce::var> pegarMedidas();            // blocos de 50 ms do teste: {rms, pico, zeros, n}

    // Usado pelo teste automático, sem placa
    void processar (const float* const* in, int nIn, float* const* out, int nOut, int n);
    double taxa() const                               { return taxaAtual.load(); }
    void taxaParaTeste (double t)                     { taxaAtual = t; }
    void esvaziarFila()                               { drenar(); }

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* in, int nIn, float* const* out, int nOut,
                                           int n, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

private:
    void run() override;
    void drenar();
    void montarWav();

    static constexpr int capacidade = 48000 * 4;      // 4 s de folga por canal na fila
    juce::AbstractFifo fila { capacidade };
    juce::AudioBuffer<float> anel { 2, capacidade };

    std::atomic<bool> monitorLigado { false }, gravandoFlag { false }, medindoFlag { false };
    std::atomic<bool> captaGrav { false }, captaMed { false };   // a thread de áudio só escreve na fila enquanto estes estão ligados
    std::atomic<int> monitorCanal { 0 };
    std::atomic<float> monitorGanho { 1.0f };
    std::atomic<float> picoAtual[2] { { 0.0f }, { 0.0f } };
    std::atomic<double> taxaAtual { 48000.0 };
    std::atomic<juce::int64> frames { 0 };
    std::atomic<int> descartadas { 0 };

    juce::CriticalSection leitura;                    // só um leitor da fila por vez (thread própria ou parada)
    juce::CriticalSection trava;                      // protege os vetores abaixo (fora da thread de áudio)
    std::vector<float> gravL, gravR;
    juce::Array<juce::var> picosNovos, medidas;
    float picoBloco = 0.0f; int contaBloco = 0;
    // medição
    double somaQ[2] {}; float picoMed[2] {}; int zerosMed[2] {}; int nMed = 0;
    static constexpr int ordemFFT = 13, tamFFT = 1 << ordemFFT;
    juce::dsp::FFT fft { ordemFFT };
    juce::dsp::WindowingFunction<float> janela { (size_t) tamFFT, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> acumFFT, blocoFFT; int posFFT = 0; int nFFT = 0; std::vector<double> espectro;

    juce::MemoryBlock wavGravado;
    juce::AudioBuffer<float> tmp { 2, 4096 };
};
