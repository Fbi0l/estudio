// Teste do motor sem placa de som: grava, mede, confere o WAV e o retorno no fone.
#include "AudioEngine.h"
#include <cstdio>
using namespace juce;
static int falhas = 0;
static void confere (bool ok, const char* oque) { std::printf ("%s %s\n", ok ? "[ok]   " : "[FALHA]", oque); if (! ok) ++falhas; }

int main()
{
    ScopedJuceInitialiser_GUI init;  // só para o sistema de mensagens; não abre placa
    AudioEngine e;
    e.taxaParaTeste (48000.0);
    const int bloco = 256, sr = 48000;
    AudioBuffer<float> in (2, bloco), out (2, bloco);
    double fase = 0;
    auto gerar = [&] (float amp) {
        for (int i = 0; i < bloco; ++i) { in.setSample (0, i, amp * (float) std::sin (fase)); in.setSample (1, i, 0.0f); fase += 2 * MathConstants<double>::pi * 440.0 / sr; }
    };
    auto processar = [&] { e.processar (in.getArrayOfReadPointers(), 2, out.getArrayOfWritePointers(), 2, bloco); };

    // 1) retorno no fone: entrada 1 nos dois ouvidos
    e.setMonitor (true, 1, 1.0f); gerar (0.5f); processar();
    confere (std::abs (out.getSample (0, 10) - in.getSample (0, 10)) < 1e-6f && std::abs (out.getSample (1, 10) - in.getSample (0, 10)) < 1e-6f, "retorno: entrada 1 sai nos dois ouvidos");
    e.setMonitor (false, 0, 1.0f); processar();
    confere (out.getMagnitude (0, bloco) == 0.0f, "retorno desligado: saída em silêncio");

    // 2) gravação de 2 s, com a thread de gravação rodando junto
    fase = 0;
    e.iniciarGravacao();
    const int blocos = 2 * sr / bloco;
    for (int b = 0; b < blocos; ++b) { gerar (0.5f); processar(); if (b % 20 == 0) Thread::sleep (1); }
    auto res = e.pararGravacao();
    std::printf ("        frames gravados %lld, esperados %lld\n", (long long) (int64) res["frames"], (long long) blocos * bloco);
    confere ((int64) res["frames"] == (int64) blocos * bloco, "gravação: nenhuma amostra perdida");
    confere ((int) res["descartadas"] == 0, "gravação: fila sem descarte");

    WavAudioFormat wav;
    std::unique_ptr<AudioFormatReader> r (wav.createReaderFor (new MemoryInputStream (e.wav(), false), true));
    confere (r != nullptr && r->bitsPerSample == 32 && r->usesFloatingPointData && r->numChannels == 2, "WAV: 32 bits ponto flutuante, 2 canais");
    if (r != nullptr)
    {
        AudioBuffer<float> lido (2, (int) r->lengthInSamples);
        r->read (&lido, 0, (int) r->lengthInSamples, 0, true, true);
        // compara com a senoide original recalculada
        double f2 = 0, erro = 0;
        for (int i = 0; i < lido.getNumSamples(); ++i) { erro = jmax (erro, std::abs ((double) lido.getSample (0, i) - 0.5 * std::sin (f2))); f2 += 2 * MathConstants<double>::pi * 440.0 / sr; }
        confere (erro < 1e-6, "WAV: amostras idênticas ao que entrou (sem perdas)");
        confere (lido.getMagnitude (1, 0, lido.getNumSamples()) == 0.0f, "WAV: canal 2 vazio, como entrou");
    }
    confere (e.pegarPicosNovos().size() > 0, "picos para a onda ao vivo");

    // 3) medição do teste: 1 s de silêncio absoluto e 1 s de tom
    e.iniciarMedicao();
    for (int b = 0; b < sr / bloco; ++b) { gerar (0.0f); processar(); }
    for (int b = 0; b < sr / bloco; ++b) { gerar (0.5f); processar(); }
    Thread::sleep (50);
    auto esp = e.pararMedicao();
    auto med = e.pegarMedidas();
    confere (med.size() >= 38 && med.size() <= 40, "medição: blocos de 50 ms");
    if (med.size() > 30)
    {
        confere ((int) med[0]["zeros"][0] == (int) med[0]["n"], "medição: silêncio absoluto detectado (zeros)");
        const double rms = (double) med[med.size() - 2]["rms"][0];
        confere (std::abs (rms - 0.5 / std::sqrt (2.0)) < 0.01, "medição: RMS do tom correto");
    }
    auto* arr = esp["espectro"].getArray();
    if (arr != nullptr)
    {
        int melhor = 0; double maior = -1e9;
        for (int k = 1; k < arr->size(); ++k) if ((double) (*arr)[k] > maior) { maior = (*arr)[k]; melhor = k; }
        const double hz = (double) esp["hz"] * melhor;
        confere (std::abs (hz - 440.0) < 10.0, "espectro: pico em 440 Hz");
    }
    std::printf ("\n%s\n", falhas == 0 ? "TODOS OS TESTES PASSARAM" : "HOUVE FALHAS");
    return falhas == 0 ? 0 : 1;
}
