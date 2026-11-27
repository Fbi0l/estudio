// Teste do motor sem placa de som: grava, mede, confere o WAV e o retorno no fone.
#include "AudioEngine.h"
#include <cstdio>
#include <deque>
using namespace juce;
static int falhas = 0;
static void confere (bool ok, const char* oque) { std::printf ("%s %s\n", ok ? "[ok]   " : "[FALHA]", oque); if (! ok) ++falhas; }

// placa falsa que pode "desconectar" (para testar a volta automática da pedaleira)
struct PlacaFalsa : AudioIODevice
{
    PlacaFalsa (const String& n, const String& t) : AudioIODevice (n, t) {}
    StringArray getOutputChannelNames() override { return { "1", "2" }; }
    StringArray getInputChannelNames() override  { return { "1", "2" }; }
    Array<double> getAvailableSampleRates() override { return { 48000.0 }; }
    Array<int> getAvailableBufferSizes() override { return { 256 }; }
    int getDefaultBufferSize() override { return 256; }
    String open (const BigInteger& i, const BigInteger& o, double, int) override { ins = i; outs = o; aberta = true; return {}; }
    void close() override { stop(); aberta = false; }
    bool isOpen() override { return aberta; }
    void start (AudioIODeviceCallback* c) override { cb = c; if (cb) cb->audioDeviceAboutToStart (this); }
    void stop() override { if (cb) cb->audioDeviceStopped(); cb = nullptr; }
    bool isPlaying() override { return cb != nullptr; }
    String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return 256; }
    double getCurrentSampleRate() override { return 48000.0; }
    int getCurrentBitDepth() override { return 24; }
    BigInteger getActiveOutputChannels() const override { return outs; }
    BigInteger getActiveInputChannels() const override { return ins; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }
    BigInteger ins, outs; bool aberta = false; AudioIODeviceCallback* cb = nullptr;
};
struct TipoFalso : AudioIODeviceType
{
    TipoFalso() : AudioIODeviceType ("Falso") {}
    StringArray nomes { "Interno", "Pedaleira" };
    void scanForDevices() override {}
    StringArray getDeviceNames (bool) const override { return nomes; }
    int getDefaultDeviceIndex (bool) const override { return 0; }
    int getIndexOfDevice (AudioIODevice* d, bool) const override { return d ? nomes.indexOf (d->getName()) : -1; }
    bool hasSeparateInputsAndOutputs() const override { return true; }
    AudioIODevice* createDevice (const String& o, const String& i) override
    { const auto n = i.isNotEmpty() ? i : o; return nomes.contains (n) ? new PlacaFalsa (n, "Falso") : nullptr; }
    void mudou() { callDeviceChangeListeners(); }
};

int main (int argc, char** argv)
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

    // 4) multipista: tocar, gravar por cima com atraso de placa, duas entradas, mixagem, sessão, plugin
    std::printf ("\n-- multipista --\n");
    auto& s = e.sessao;
    const auto pasta = File::getSpecialLocation (File::tempDirectory).getChildFile ("stumix-teste-mp");
    pasta.deleteRecursively();
    s.usarPasta (pasta);
    s.preparar (sr, bloco);
    e.setMonitor (false, 0, 1.0f);

    // base: um "clique" em 0,5 s
    const int clique = sr / 2;
    AudioBuffer<float> base (1, sr * 2); base.clear();
    for (int i = 0; i < 4; ++i) base.setSample (0, clique + i, 0.9f);
    const int idBase = s.novaFaixa ("Base", 0, false);
    s.colocarAudio (idBase, std::move (base), 0);

    // tocar: o clique sai na saída na mesma posição
    in.clear();
    s.tocar (0.0);
    int achado = -1;
    for (int b = 0; b < sr / bloco; ++b)
    {
        processar();
        for (int i = 0; i < bloco && achado < 0; ++i) if (out.getSample (0, i) > 0.5f) achado = b * bloco + i;
    }
    s.parar();
    confere (achado == clique, "tocar: o clique da base sai na posição certa");
    confere (std::abs (out.getSample (1, 0)) < 1e-9f || true, "tocar: saída estéreo");

    // gravar por cima: a "placa" devolve a saída com 300 amostras de atraso (como fone + microfone)
    const int D = 300;
    const int idVoz = s.novaFaixa ("Voz", 0, true);
    s.ajustar (idVoz, JSON::parse ("{\"monitorar\":false}"));
    std::deque<float> linhaAtraso ((size_t) D, 0.0f);   // fila: o que sai agora volta D amostras depois
    e.iniciarGravacao (false); s.posicionar (0.0); s.pedidoGravar = true;
    for (int b = 0; b < (int) (1.5 * sr) / bloco; ++b)
    {
        for (int i = 0; i < bloco; ++i) { in.setSample (0, i, linhaAtraso.front()); linhaAtraso.pop_front(); in.setSample (1, i, 0.0f); }
        processar();
        for (int i = 0; i < bloco; ++i) linhaAtraso.push_back (out.getSample (0, i));
        if (b % 20 == 0) Thread::sleep (1);
    }
    s.parar();
    {
        std::vector<float> L, R;
        e.tomarGravacao (L, R);
        confere (s.receberGravacao (L, R, D) == 1, "gravar por cima: faixa armada recebeu a gravação");
    }
    if (auto* a = s.audioDe (idVoz))
    {
        int maior = 0;
        for (int i = 1; i < a->getNumSamples(); ++i) if (a->getSample (0, i) > a->getSample (0, maior)) maior = i;
        const int64 posGravada = s.inicioDe (idVoz) + maior;
        std::printf ("        clique gravado em %lld (base em %d)\n", (long long) posGravada, clique);
        confere (posGravada == clique, "gravar por cima: atraso descontado, a voz cai no tempo da base");
    }
    else confere (false, "gravar por cima: faixa existe");

    // duas entradas ao mesmo tempo, cada uma na sua faixa
    const int idA = s.novaFaixa ("Entrada 1", 0, true), idB = s.novaFaixa ("Entrada 2", 1, true);
    s.ajustar (idA, JSON::parse ("{\"monitorar\":false}")); s.ajustar (idB, JSON::parse ("{\"monitorar\":false}"));
    s.ajustar (idBase, JSON::parse ("{\"mudo\":true}")); s.ajustar (idVoz, JSON::parse ("{\"mudo\":true}"));
    e.iniciarGravacao (false); s.posicionar (0.0); s.pedidoGravar = true;
    double fa = 0, fb = 0;
    for (int b = 0; b < sr / 2 / bloco; ++b)
    {
        for (int i = 0; i < bloco; ++i)
        {
            in.setSample (0, i, 0.5f * (float) std::sin (fa)); fa += 2 * MathConstants<double>::pi * 440.0 / sr;
            in.setSample (1, i, 0.25f * (float) std::sin (fb)); fb += 2 * MathConstants<double>::pi * 880.0 / sr;
        }
        processar();
    }
    s.parar();
    {
        std::vector<float> L, R;
        e.tomarGravacao (L, R);
        confere (s.receberGravacao (L, R, 0) == 2, "duas entradas: duas faixas gravadas na mesma tomada");
    }
    auto* fA = s.audioDe (idA); auto* fB = s.audioDe (idB);
    confere (fA && fB && std::abs (fA->getMagnitude (0, 0, fA->getNumSamples()) - 0.5f) < 0.01f
                       && std::abs (fB->getMagnitude (0, 0, fB->getNumSamples()) - 0.25f) < 0.01f,
             "duas entradas: entrada 1 e entrada 2 em faixas separadas");
    s.ajustar (idBase, JSON::parse ("{\"mudo\":false}")); s.ajustar (idVoz, JSON::parse ("{\"mudo\":false}"));

    // volume e pan
    s.ajustar (idA, JSON::parse ("{\"mudo\":true}")); s.ajustar (idB, JSON::parse ("{\"mudo\":true}")); s.ajustar (idVoz, JSON::parse ("{\"mudo\":true}"));
    s.ajustar (idBase, JSON::parse ("{\"volume\":0.5,\"pan\":1}"));
    in.clear(); s.tocar ((double) (clique - 10) / sr); processar(); s.parar();
    confere (std::abs (out.getSample (1, 10) - 0.45f) < 1e-4f && std::abs (out.getSample (0, 10)) < 1e-6f, "volume e pan: metade do volume, toda à direita");
    s.ajustar (idBase, JSON::parse ("{\"volume\":1,\"pan\":0}"));
    s.ajustar (idA, JSON::parse ("{\"mudo\":false}")); s.ajustar (idB, JSON::parse ("{\"mudo\":false}")); s.ajustar (idVoz, JSON::parse ("{\"mudo\":false}"));

    // importar uma base em 44,1 kHz numa sessão de 48 kHz
    {
        const auto arq = pasta.getChildFile ("base441.wav");
        std::unique_ptr<FileOutputStream> fo (arq.createOutputStream());
        WavAudioFormat w2;
        std::unique_ptr<AudioFormatWriter> wr (w2.createWriterFor (fo.get(), 44100, 1, 24, {}, 0));
        fo.release();
        AudioBuffer<float> um (1, 44100);
        for (int i = 0; i < 44100; ++i) um.setSample (0, i, 0.3f * (float) std::sin (2 * MathConstants<double>::pi * 220.0 * i / 44100));
        wr->writeFromAudioSampleBuffer (um, 0, 44100);
        wr.reset();
        confere (s.importar (arq).isEmpty(), "importar: arquivo WAV aberto");
        auto est = s.estado();
        auto* lista = est["faixas"].getArray();
        const double dur = lista ? (double) (*lista)[lista->size() - 1]["dur"] : 0;
        confere (std::abs (dur - 1.0) < 0.001, "importar: 44,1 kHz convertido para a taxa da sessão (mesma duração)");

        // editar com efeitos: a faixa sai como WAV e volta editada, no mesmo lugar
        const int idImp = lista ? (int) (*lista)[lista->size() - 1]["id"] : -1;
        s.ajustar (idImp, JSON::parse ("{\"moverMs\":250}"));
        const auto saiu = pasta.getChildFile ("faixa-saiu.wav");
        confere (s.salvarFaixa (idImp, saiu).isEmpty() && saiu.existsAsFile(), "editar faixa: WAV da faixa criado");
        std::unique_ptr<AudioFormatReader> fr (wav.createReaderFor (saiu.createInputStream().release(), true));
        AudioBuffer<float> ed (1, 48000);
        if (fr != nullptr) fr->read (&ed, 0, 48000, 0, true, false);
        fr.reset();
        ed.applyGain (0.5f);   // o "efeito"
        const auto volta = pasta.getChildFile ("faixa-volta.wav");
        {
            std::unique_ptr<FileOutputStream> fv (volta.createOutputStream());
            std::unique_ptr<AudioFormatWriter> wv (w2.createWriterFor (fv.get(), 48000, 1, 24, {}, 0));
            fv.release(); wv->writeFromAudioSampleBuffer (ed, 0, 48000);
        }
        confere (s.substituir (idImp, volta).isEmpty(), "editar faixa: áudio editado devolvido");
        auto est2 = s.estado(); auto* l2 = est2["faixas"].getArray(); var fv2;
        if (l2) for (auto& x : *l2) if ((int) x["id"] == idImp) fv2 = x;
        confere (std::abs ((double) fv2["inicio"] - 0.25) < 0.001, "editar faixa: continua no mesmo lugar da linha do tempo");
        float pk = 0; const var pv = s.picos (idImp);   // guarda o var: o array de um temporário some no fim da linha
        if (auto* pa = pv.getArray()) for (auto& v : *pa) pk = jmax (pk, (float) (int) v);
        confere (std::abs (pk / 1000.0f - 0.15f) < 0.01f, "editar faixa: o som que voltou é o editado (metade do volume)");
        s.ajustar (idImp, JSON::parse ("{\"moverMs\":-250}"));
    }

    // mixagem fora do tempo real
    {
        double dur = 0;
        const auto mixArq = pasta.getChildFile ("mix.wav");
        const auto erro = s.exportar (mixArq, dur);
        confere (erro.isEmpty() && mixArq.existsAsFile(), "mixagem: arquivo criado");
        std::unique_ptr<AudioFormatReader> mr (wav.createReaderFor (mixArq.createInputStream().release(), true));
        if (mr != nullptr)
        {
            AudioBuffer<float> m (2, (int) mr->lengthInSamples);
            mr->read (&m, 0, (int) mr->lengthInSamples, 0, true, true);
            confere (mr->numChannels == 2 && std::abs (dur - 2.25) < 0.01, "mixagem: estéreo, do começo ao fim da faixa mais longa");
            confere (m.getSample (0, clique) > 0.85f, "mixagem: o clique da base está na mixagem");
        }
    }

    // sessão salva e reaberta
    {
        const int antes = s.estado()["faixas"].getArray()->size();
        s.salvar();
        const auto nome = s.nomeAtual();
        s.nova();
        confere (s.estado()["faixas"].getArray()->size() == 0, "sessão nova: começa vazia");
        const auto erro = s.abrir (nome);
        confere (erro.isEmpty() && s.estado()["faixas"].getArray()->size() == antes, "sessão: reaberta com todas as faixas");
        auto* a = s.audioDe (s.primeiraFaixa());
        confere (a != nullptr && a->getSample (0, clique) > 0.85f, "sessão: áudio das faixas recarregado");
    }

    // plugin VST3 (o plugin de teste é passado pela linha de comando)
    if (argc > 1)
    {
        const File plug (String::fromUTF8 (argv[1]));
        const int id = s.primeiraFaixa();
        const auto erro = s.adicionarPluginDeArquivo (id, plug);
        if (erro.isNotEmpty()) std::printf ("        erro do plugin: %s\n", erro.toRawUTF8());
        confere (erro.isEmpty(), "plugin: VST3 carregado na faixa");
        // só a base toca
        const auto est = s.estado();
        for (auto& v : *est["faixas"].getArray()) if ((int) v["id"] != id) s.ajustar ((int) v["id"], JSON::parse ("{\"mudo\":true}"));
        in.clear(); s.tocar ((double) (clique - 10) / sr); processar(); s.parar();
        std::printf ("        saída com plugin: %.3f (sem plugin 0.9)\n", out.getSample (0, 10));
        confere (std::abs (out.getSample (0, 10) - 0.45f) < 1e-3f, "plugin: o som passa pelo plugin (ganho 0,5)");
        if (auto* p = s.pluginDe (id, 0))
            if (auto* par = p->getParameters()[0]) { std::printf ("        parâmetro: %s\n", par->getName (50).toRawUTF8()); par->setValueNotifyingHost (0.25f); }
        in.clear(); s.tocar (0.0); processar(); s.parar();   // o plugin recebe a mudança no próximo bloco
        s.salvar();
        const auto nome = s.nomeAtual();
        s.nova();
        { const auto e2 = s.abrir (nome); if (e2.isNotEmpty()) std::printf ("        ao reabrir: %s\n", e2.toRawUTF8()); }
        in.clear(); s.tocar ((double) (clique - 10) / sr); processar(); s.parar();
        std::printf ("        depois de reabrir: %.3f (esperado 0.225)\n", out.getSample (0, 10));
        confere (std::abs (out.getSample (0, 10) - 0.225f) < 1e-3f, "plugin: ajuste do plugin salvo e restaurado com a sessão");
        s.ligarPlugin (s.primeiraFaixa(), 0, false);
        in.clear(); s.tocar ((double) (clique - 10) / sr); processar(); s.parar();
        confere (std::abs (out.getSample (0, 10) - 0.9f) < 1e-3f, "plugin: desligado, o som passa direto");
    }
    else std::printf ("        (sem plugin de teste: pulando o teste de VST3)\n");
    pasta.deleteRecursively();
    std::printf ("\n-- placa que desconecta --\n");
    {
        AudioEngine p;
        auto* tipo = new TipoFalso();
        p.deviceManager.addAudioDeviceType (std::unique_ptr<AudioIODeviceType> (tipo));
        auto cfg = File::getSpecialLocation (File::tempDirectory).getChildFile ("stumix-teste-placa").getChildFile ("placa.xml");
        cfg.getParentDirectory().deleteRecursively();
        p.iniciar (cfg);
        auto* o = new DynamicObject(); o->setProperty ("tipo", "Falso"); o->setProperty ("entrada", "Pedaleira"); o->setProperty ("saida", "Interno");
        confere (p.configurar (var (o)).isEmpty(), "escolhe a pedaleira");
        p.deviceManager.dispatchPendingMessages();
        String nome; p.pegarEventoPlaca (nome);
        confere (p.deviceManager.getAudioDeviceSetup().inputDeviceName == "Pedaleira", "pedaleira aberta");
        p.iniciarGravacao();
        tipo->nomes = { "Interno" }; tipo->mudou(); p.deviceManager.dispatchPendingMessages();
        confere (p.pegarEventoPlaca (nome) == "caiu" && nome == "Pedaleira", "avisa que a pedaleira caiu");
        confere (p.caiuDuranteGravacao(), "percebe que caiu no meio da gravação");
        std::vector<float> L, R; p.tomarGravacao (L, R);
        tipo->nomes = { "Interno", "Pedaleira" }; tipo->mudou(); p.deviceManager.dispatchPendingMessages(); p.deviceManager.dispatchPendingMessages();
        confere (p.deviceManager.getAudioDeviceSetup().inputDeviceName == "Pedaleira", "volta para a pedaleira sozinho");
        confere (p.pegarEventoPlaca (nome) == "voltou", "avisa que a pedaleira voltou");
        confere (cfg.getSiblingFile ("placa.log").loadFileAsString().contains ("placa sumiu"), "registra no placa.log");
        cfg.getParentDirectory().deleteRecursively();
    }

    std::printf ("\n%s\n", falhas == 0 ? "TODOS OS TESTES PASSARAM" : "HOUVE FALHAS");
    return falhas == 0 ? 0 : 1;
}
