// Stumix Desktop: motor de áudio nativo
// Copyright (C) 2026 Fbiol. Licença GPLv3 (ver LICENSE na raiz do repositório).
#include "AudioEngine.h"

using namespace juce;

AudioEngine::AudioEngine() : Thread ("Estudio-gravacao")
{
    acumFFT.assign ((size_t) tamFFT, 0.0f);
    blocoFFT.assign ((size_t) tamFFT * 2, 0.0f);
    espectro.assign ((size_t) tamFFT / 2, 0.0);
    startThread (Priority::high);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
    stopThread (2000);
}

void AudioEngine::iniciar (const File& configSalva)
{
    // 2 entradas e 2 saídas; abre direto com a última configuração (driver, placa, taxa, buffer)
    arquivoConfig = configSalva;
    std::unique_ptr<XmlElement> salvo;
    if (configSalva.existsAsFile()) salvo = parseXML (configSalva);
    auto erro = deviceManager.initialise (2, 2, salvo.get(), true);
    configRestaurada = salvo != nullptr && erro.isEmpty();
    tiposProcurados.insert (deviceManager.getCurrentAudioDeviceType());   // o initialise já procurou o driver atual
    if (erro.isNotEmpty())
        DBG ("Erro ao abrir o áudio: " << erro);

    // a placa escolhida é a da configuração salva, mesmo que ela não esteja ligada agora
    tipoDesejado = deviceManager.getCurrentAudioDeviceType();
    desejado = deviceManager.getAudioDeviceSetup();
    if (salvo != nullptr)
    {
        tipoDesejado = salvo->getStringAttribute ("deviceType", tipoDesejado);
        desejado.inputDeviceName  = salvo->getStringAttribute ("audioInputDeviceName", desejado.inputDeviceName);
        desejado.outputDeviceName = salvo->getStringAttribute ("audioOutputDeviceName", desejado.outputDeviceName);
        desejado.sampleRate = salvo->getDoubleAttribute ("audioDeviceRate", desejado.sampleRate);
        desejado.bufferSize = salvo->getIntAttribute ("audioDeviceBufferSize", desejado.bufferSize);
        fora = ! configRestaurada;
        if (fora) registrar ("placa escolhida não abriu ao iniciar: " + desejado.inputDeviceName + " / " + erro);
    }
    temDesejado = true;
    deviceManager.addChangeListener (this);
    deviceManager.addAudioCallback (this);
}

void AudioEngine::registrar (const String& texto)
{
    if (arquivoConfig == File()) return;
    auto log = arquivoConfig.getSiblingFile ("placa.log");
    if (log.getSize() > 200000) log.deleteFile();
    log.appendText (Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S  ") + texto + "\n", false, false, "\r\n");
}

String AudioEngine::pegarEventoPlaca (String& nome)
{
    const ScopedLock sl (travaEvento);
    nome = desejado.inputDeviceName.isNotEmpty() ? desejado.inputDeviceName : desejado.outputDeviceName;
    auto e = eventoPlaca; eventoPlaca.clear(); return e;
}

void AudioEngine::changeListenerCallback (ChangeBroadcaster*)
{
    if (! temDesejado) return;
    const auto agora = deviceManager.getAudioDeviceSetup();
    const bool igual = deviceManager.getCurrentAudioDeviceType() == tipoDesejado
                    && agora.inputDeviceName == desejado.inputDeviceName
                    && agora.outputDeviceName == desejado.outputDeviceName
                    && deviceManager.getCurrentAudioDevice() != nullptr;
    if (igual)
    {
        if (fora) { fora = false; registrar ("placa voltou: " + desejado.inputDeviceName); const ScopedLock sl (travaEvento); eventoPlaca = "voltou"; }
        return;
    }

    // a placa escolhida está na lista de novo? volta para ela
    for (auto* t : deviceManager.getAvailableDeviceTypes())
    {
        if (t->getTypeName() != tipoDesejado) continue;
        const auto ent = t->getDeviceNames (true), sai = t->getDeviceNames (false);
        const bool tem = (desejado.inputDeviceName.isEmpty() || ent.contains (desejado.inputDeviceName))
                      && (desejado.outputDeviceName.isEmpty() || sai.contains (desejado.outputDeviceName));
        if (tem)
        {
            if (deviceManager.getCurrentAudioDeviceType() != tipoDesejado)
                deviceManager.setCurrentAudioDeviceType (tipoDesejado, true);
            auto s = desejado;
            s.useDefaultInputChannels = false; s.inputChannels.clear(); s.inputChannels.setRange (0, 2, true);
            s.useDefaultOutputChannels = true;
            const auto erro = deviceManager.setAudioDeviceSetup (s, true);   // gera outra mensagem: cai no "igual" acima
            if (erro.isNotEmpty()) registrar ("não consegui reabrir " + desejado.inputDeviceName + ": " + erro);
            return;
        }
    }
    if (! fora)
    {
        fora = true;
        registrar ("placa sumiu: " + desejado.inputDeviceName + " (agora: " + agora.inputDeviceName + ")");
        const ScopedLock sl (travaEvento); eventoPlaca = "caiu";
    }
}

//==============================================================================
static var nomes (const StringArray& s)
{
    Array<var> a;
    for (auto& x : s) a.add (x);
    return a;
}

var AudioEngine::listar (bool forcar)
{
    // procurar placas ASIO carrega o driver de cada uma: só o driver em uso é procurado na abertura
    Array<var> tipos;
    const auto atual = deviceManager.getCurrentAudioDeviceType();
    for (auto* tipo : deviceManager.getAvailableDeviceTypes())
    {
        const auto nome = tipo->getTypeName();
        if (forcar || (nome == atual && tiposProcurados.count (nome) == 0))
        {
            tipo->scanForDevices();
            tiposProcurados.insert (nome);
        }
        auto* o = new DynamicObject();
        o->setProperty ("nome", tipo->getTypeName());
        o->setProperty ("entradas", nomes (tipo->getDeviceNames (true)));
        o->setProperty ("saidas", nomes (tipo->getDeviceNames (false)));
        tipos.add (var (o));
    }
    return tipos;
}

var AudioEngine::estado()
{
    auto* o = new DynamicObject();
    auto setup = deviceManager.getAudioDeviceSetup();
    o->setProperty ("tipo", deviceManager.getCurrentAudioDeviceType());
    o->setProperty ("entrada", setup.inputDeviceName);
    o->setProperty ("saida", setup.outputDeviceName);

    if (auto* d = deviceManager.getCurrentAudioDevice())
    {
        const double sr = d->getCurrentSampleRate();
        const int buf = d->getCurrentBufferSizeSamples();
        const int latIn = d->getInputLatencyInSamples(), latOut = d->getOutputLatencyInSamples();
        o->setProperty ("aberto", true);
        o->setProperty ("taxa", sr);
        o->setProperty ("buffer", buf);
        o->setProperty ("bits", d->getCurrentBitDepth());
        Array<var> taxas, buffers;
        for (auto t : d->getAvailableSampleRates()) taxas.add (t);
        for (auto b : d->getAvailableBufferSizes()) buffers.add (b);
        o->setProperty ("taxas", taxas);
        o->setProperty ("buffers", buffers);
        o->setProperty ("canaisEntrada", d->getActiveInputChannels().countNumberOfSetBits());
        o->setProperty ("nomesEntradas", nomes (d->getInputChannelNames()));
        o->setProperty ("latEntradaMs", sr > 0 ? 1000.0 * latIn / sr : 0.0);
        o->setProperty ("latSaidaMs", sr > 0 ? 1000.0 * latOut / sr : 0.0);
        o->setProperty ("bufferMs", sr > 0 ? 1000.0 * buf / sr : 0.0);
        // ida e volta: entrada + saída (os drivers já somam o buffer nessas latências)
        o->setProperty ("idaVoltaMs", sr > 0 ? 1000.0 * (latIn + latOut) / sr : 0.0);
    }
    else
    {
        o->setProperty ("aberto", false);
        o->setProperty ("taxa", taxaAtual.load());
    }
    o->setProperty ("descartadas", descartadas.load());
    o->setProperty ("configSalva", configRestaurada);
    return var (o);
}

String AudioEngine::configurar (const var& cfg)
{
    // Os dispositivos chegam pela posição na lista (sem depender de acentos nos nomes)
    auto nomeDe = [] (const StringArray& lista, const var& v) { const int i = (int) v; return isPositiveAndBelow (i, lista.size()) ? lista[i] : String(); };
    String tipo = cfg.getProperty ("tipo", "").toString();
    if (cfg.hasProperty ("tipoIdx"))
    {
        auto& tipos = deviceManager.getAvailableDeviceTypes();
        const int i = (int) cfg["tipoIdx"];
        if (isPositiveAndBelow (i, tipos.size())) tipo = tipos[i]->getTypeName();
    }
    if (tipo.isNotEmpty() && tipo != deviceManager.getCurrentAudioDeviceType())
        deviceManager.setCurrentAudioDeviceType (tipo, true);

    auto setup = deviceManager.getAudioDeviceSetup();
    if (cfg.hasProperty ("entrada")) setup.inputDeviceName = cfg["entrada"].toString();
    if (cfg.hasProperty ("saida"))   setup.outputDeviceName = cfg["saida"].toString();
    if (auto* t = deviceManager.getCurrentDeviceTypeObject())
    {
        if (cfg.hasProperty ("entradaIdx")) setup.inputDeviceName  = nomeDe (t->getDeviceNames (true),  cfg["entradaIdx"]);
        if (cfg.hasProperty ("saidaIdx"))   setup.outputDeviceName = nomeDe (t->getDeviceNames (false), cfg["saidaIdx"]);
    }
    if (cfg.hasProperty ("taxa"))    setup.sampleRate = (double) cfg["taxa"];
    if (cfg.hasProperty ("buffer"))  setup.bufferSize = (int) cfg["buffer"];
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.inputChannels.setRange (0, 2, true);
    setup.useDefaultOutputChannels = true;
    const auto erro = deviceManager.setAudioDeviceSetup (setup, true);
    if (erro.isEmpty())
    {
        tipoDesejado = deviceManager.getCurrentAudioDeviceType();
        desejado = deviceManager.getAudioDeviceSetup();
        temDesejado = true; fora = false;
    }
    if (erro.isEmpty() && arquivoConfig != File())
        if (auto xml = deviceManager.createStateXml())
        {
            arquivoConfig.getParentDirectory().createDirectory();
            xml->writeTo (arquivoConfig);
        }
    return erro;
}

void AudioEngine::setMonitor (bool ligado, int canal, float ganho)
{
    monitorCanal = jlimit (0, 3, canal);
    monitorGanho = jlimit (0.0f, 4.0f, ganho);
    monitorLigado = ligado;
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (AudioIODevice* d)
{
    taxaAtual = d->getCurrentSampleRate();
    sessao.preparar (d->getCurrentSampleRate(), d->getCurrentBufferSizeSamples());
}

int AudioEngine::latenciaAmostras() const
{
    if (auto* d = deviceManager.getCurrentAudioDevice())
        return d->getInputLatencyInSamples() + d->getOutputLatencyInSamples();
    return 0;
}

void AudioEngine::audioDeviceStopped()
{
    if (gravandoFlag.load()) caiuGravando = true;   // a placa parou no meio da gravação
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int nIn, float* const* out,
                                                    int nOut, int n, const AudioIODeviceCallbackContext&)
{
    processar (in, nIn, out, nOut, n);
}

void AudioEngine::processar (const float* const* in, int nIn, float* const* out, int nOut, int n)
{
    const float* l = nIn > 0 ? in[0] : nullptr;
    const float* r = nIn > 1 ? in[1] : l;

    // multipista: a gravação começa exatamente no bloco em que a reprodução começa
    if (sessao.pedidoGravar.exchange (false))
    {
        sessao.comecarGravacaoNoBloco();
        captaGrav = true;
    }

    // medidor
    for (int c = 0; c < 2; ++c)
    {
        const float* s = c == 0 ? l : r;
        float p = 0.0f;
        if (s != nullptr) for (int i = 0; i < n; ++i) p = jmax (p, std::abs (s[i]));
        picoAtual[(size_t) c] = jmax (p, picoAtual[(size_t) c].load() * 0.9f);
    }

    // retorno direto no fone
    if (monitorLigado.load() && l != nullptr && nOut > 0)
    {
        const float g = monitorGanho.load();
        const int modo = monitorCanal.load();
        for (int i = 0; i < n; ++i)
        {
            float a, b;
            if (modo == 3)      { a = l[i]; b = r[i]; }
            else if (modo == 1) { a = b = l[i]; }
            else if (modo == 2) { a = b = r[i]; }
            else                { a = b = 0.5f * (l[i] + r[i]); }
            out[0][i] = a * g;
            if (nOut > 1) out[1][i] = b * g;
        }
        for (int c = 2; c < nOut; ++c) if (out[c] != nullptr) FloatVectorOperations::clear (out[c], n);
    }
    else
    {
        for (int c = 0; c < nOut; ++c) if (out[c] != nullptr) FloatVectorOperations::clear (out[c], n);
    }

    // faixas tocando (e entradas armadas pela cadeia de plugins) somam na saída
    sessao.processar (l, nIn > 1 ? in[1] : nullptr, out, nOut, n);

    // fila para a thread de gravação/medição (só copia)
    if ((captaGrav.load() || captaMed.load()) && l != nullptr)
    {
        int s1, n1, s2, n2;
        fila.prepareToWrite (n, s1, n1, s2, n2);
        if (n1 + n2 < n) descartadas += n - (n1 + n2);
        if (n1 > 0) { anel.copyFrom (0, s1, l, n1); anel.copyFrom (1, s1, r, n1); }
        if (n2 > 0) { anel.copyFrom (0, s2, l + n1, n2); anel.copyFrom (1, s2, r + n1, n2); }
        fila.finishedWrite (n1 + n2);
    }
}

//==============================================================================
void AudioEngine::run()
{
    while (! threadShouldExit())
    {
        drenar();
        wait (10);
    }
}

void AudioEngine::drenar()
{
    const ScopedLock sl0 (leitura);
    for (;;)
    {
        int s1, n1, s2, n2;
        fila.prepareToRead (jmin (fila.getNumReady(), tmp.getNumSamples()), s1, n1, s2, n2);
        const int total = n1 + n2;
        if (total == 0) break;
        tmp.copyFrom (0, 0, anel, 0, s1, n1); tmp.copyFrom (1, 0, anel, 1, s1, n1);
        if (n2 > 0) { tmp.copyFrom (0, n1, anel, 0, s2, n2); tmp.copyFrom (1, n1, anel, 1, s2, n2); }
        fila.finishedRead (total);

        const float* l = tmp.getReadPointer (0);
        const float* r = tmp.getReadPointer (1);
        const ScopedLock sl (trava);

        if (gravandoFlag.load())
        {
            gravL.insert (gravL.end(), l, l + total);
            gravR.insert (gravR.end(), r, r + total);
            frames += total;
            for (int i = 0; i < total; ++i)
            {
                picoBloco = jmax (picoBloco, std::abs (l[i]), std::abs (r[i]));
                if (++contaBloco == 512) { picosNovos.add (picoBloco); picoBloco = 0.0f; contaBloco = 0; }
            }
        }

        if (medindoFlag.load())
        {
            const int alvo = (int) (taxaAtual.load() * 0.05);
            for (int i = 0; i < total; ++i)
            {
                const float v[2] = { l[i], r[i] };
                for (int c = 0; c < 2; ++c)
                {
                    somaQ[c] += (double) v[c] * v[c];
                    picoMed[c] = jmax (picoMed[c], std::abs (v[c]));
                    if (v[c] == 0.0f) ++zerosMed[c];
                }
                if (++nMed >= alvo)
                {
                    auto* o = new DynamicObject();
                    o->setProperty ("rms", Array<var> { std::sqrt (somaQ[0] / nMed), std::sqrt (somaQ[1] / nMed) });
                    o->setProperty ("pico", Array<var> { picoMed[0], picoMed[1] });
                    o->setProperty ("zeros", Array<var> { zerosMed[0], zerosMed[1] });
                    o->setProperty ("n", nMed);
                    medidas.add (var (o));
                    somaQ[0] = somaQ[1] = 0; picoMed[0] = picoMed[1] = 0; zerosMed[0] = zerosMed[1] = 0; nMed = 0;
                }
                // espectro médio (mistura dos dois canais)
                acumFFT[(size_t) posFFT++] = 0.5f * (l[i] + r[i]);
                if (posFFT == tamFFT)
                {
                    std::copy (acumFFT.begin(), acumFFT.end(), blocoFFT.begin());
                    std::fill (blocoFFT.begin() + tamFFT, blocoFFT.end(), 0.0f);
                    janela.multiplyWithWindowingTable (blocoFFT.data(), (size_t) tamFFT);
                    fft.performFrequencyOnlyForwardTransform (blocoFFT.data());
                    for (int k = 0; k < tamFFT / 2; ++k) espectro[(size_t) k] += (double) blocoFFT[(size_t) k] * blocoFFT[(size_t) k];
                    ++nFFT;
                    // metade de sobreposição
                    std::copy (acumFFT.begin() + tamFFT / 2, acumFFT.end(), acumFFT.begin());
                    posFFT = tamFFT / 2;
                }
            }
        }
    }
}

//==============================================================================
void AudioEngine::iniciarGravacao (bool imediato)
{
    {
        const ScopedLock sl (trava);
        gravL.clear(); gravR.clear(); picosNovos.clear();
        gravL.reserve ((size_t) (taxaAtual.load() * 120)); gravR.reserve ((size_t) (taxaAtual.load() * 120));
        picoBloco = 0.0f; contaBloco = 0;
    }
    frames = 0; descartadas = 0;
    gravandoFlag = true;
    if (imediato) captaGrav = true;
}

void AudioEngine::tomarGravacao (std::vector<float>& L, std::vector<float>& R)
{
    captaGrav = false;
    drenar();
    gravandoFlag = false;
    const ScopedLock sl (trava);
    L.swap (gravL); R.swap (gravR);
    gravL.clear(); gravR.clear();
}

var AudioEngine::pararGravacao()
{
    captaGrav = false;   // para de captar…
    drenar();            // …grava o que ainda estava na fila…
    gravandoFlag = false; // …e só então encerra
    montarWav();
    auto* o = new DynamicObject();
    const ScopedLock sl (trava);
    o->setProperty ("ok", ! gravL.empty());
    o->setProperty ("frames", (int64) gravL.size());
    o->setProperty ("taxa", taxaAtual.load());
    o->setProperty ("duracao", taxaAtual.load() > 0 ? (double) gravL.size() / taxaAtual.load() : 0.0);
    o->setProperty ("descartadas", descartadas.load());
    o->setProperty ("bytes", (int64) wavGravado.getSize());
    return var (o);
}

void AudioEngine::montarWav()
{
    const ScopedLock sl (trava);
    wavGravado.reset();
    if (gravL.empty()) return;
    // WAV de 32 bits em ponto flutuante: sem perdas, guarda exatamente o que a placa entregou
    auto* mos = new MemoryOutputStream (wavGravado, false);
    WavAudioFormat formato;
    std::unique_ptr<AudioFormatWriter> w (formato.createWriterFor (mos, taxaAtual.load(), 2, 32, {}, 0));
    if (w == nullptr) { delete mos; return; }
    const float* canais[2] = { gravL.data(), gravR.data() };
    w->writeFromFloatArrays (canais, 2, (int) gravL.size());
    w.reset(); // fecha o cabeçalho
}

Array<var> AudioEngine::pegarPicosNovos()
{
    const ScopedLock sl (trava);
    auto a = picosNovos; picosNovos.clear(); return a;
}

Array<var> AudioEngine::pegarMedidas()
{
    const ScopedLock sl (trava);
    auto a = medidas; medidas.clear(); return a;
}

void AudioEngine::iniciarMedicao()
{
    {
        const ScopedLock sl (trava);
        medidas.clear();
        somaQ[0] = somaQ[1] = 0; picoMed[0] = picoMed[1] = 0; zerosMed[0] = zerosMed[1] = 0; nMed = 0;
        std::fill (espectro.begin(), espectro.end(), 0.0); nFFT = 0; posFFT = 0;
    }
    medindoFlag = true;
    captaMed = true;
}

var AudioEngine::pararMedicao()
{
    captaMed = false;
    drenar();
    medindoFlag = false;
    const ScopedLock sl (trava);
    Array<var> db;
    for (int k = 0; k < tamFFT / 2; ++k)
        db.add (nFFT > 0 ? 10.0 * std::log10 (espectro[(size_t) k] / nFFT + 1e-30) : -300.0);
    auto* o = new DynamicObject();
    o->setProperty ("espectro", db);
    o->setProperty ("hz", taxaAtual.load() / tamFFT);
    o->setProperty ("blocos", nFFT);
    return var (o);
}
