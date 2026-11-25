// Stumix Desktop: multipista (faixas, gravação por cima, plugins VST3)
// Copyright (C) 2026 Fbiol. Licença GPLv3 (ver LICENSE na raiz do repositório).
#include "Sessao.h"
#include "Lancador.h"
#include <optional>

using namespace juce;

namespace
{
    String t (const char* s) { return String::fromUTF8 (s); }

    struct Tarefa : Thread
    {
        std::function<void()> f;
        explicit Tarefa (std::function<void()> fn) : Thread ("Stumix-plugins"), f (std::move (fn)) {}
        void run() override { f(); }
    };

    struct JanelaPlugin : DocumentWindow
    {
        std::function<void()> aoFechar;
        JanelaPlugin (const String& nome) : DocumentWindow (nome, Colour (0xff16181a), DocumentWindow::closeButton) {}
        void closeButtonPressed() override { if (aoFechar) aoFechar(); }
    };

    String nomeSeguro (String s)
    {
        return File::createLegalFileName (s).trim();
    }
}

Sessao::Sessao()
{
    addDefaultFormatsToManager (formatos);   // VST3 (e o que mais o JUCE tiver ligado nesta plataforma)
}

Sessao::~Sessao()
{
    if (busca != nullptr) busca->stopThread (4000);
    janelas.clear();   // editores antes dos plugins
    const ScopedLock sl (trava);
    faixas.clear();
}

//==============================================================================
// Thread de áudio
void Sessao::preparar (double novaTaxa, int blocoMax)
{
    const ScopedLock sl (trava);
    taxa = novaTaxa > 0 ? novaTaxa : 48000.0;
    bloco = jmax (64, blocoMax);
    for (auto* f : faixas) prepararFaixa (*f);
}

void Sessao::prepararFaixa (Faixa& f)
{
    int canais = 2;
    for (auto* p : f.plugins)
    {
        canais = jmax (canais, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
        p->releaseResources();
        p->prepareToPlay (taxa, bloco);
    }
    f.trab.setSize (canais, bloco, false, true, false);
}

void Sessao::comecarGravacaoNoBloco()
{
    inicioGrav = pos.load();
    tocandoFlag = true;
}

void Sessao::processarCadeia (Faixa& f, int n)
{
    for (int i = 0; i < f.plugins.size(); ++i)
    {
        if (! f.ligado[(size_t) i]) continue;
        auto* p = f.plugins[i];
        const int canais = jmax (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels(), 1);
        AudioBuffer<float> vista (f.trab.getArrayOfWritePointers(), jmin (canais, f.trab.getNumChannels()), n);
        midiVazio.clear();
        p->processBlock (vista, midiVazio);
        if (p->getTotalNumOutputChannels() == 1)          // plugin mono: a saída vai para os dois lados
            f.trab.copyFrom (1, 0, f.trab, 0, 0, n);
    }
}

void Sessao::processar (const float* inL, const float* inR, float* const* out, int nOut, int n)
{
    const ScopedTryLock tl (trava);
    if (! tl.isLocked() || nOut < 1) return;

    bool algumSolo = false;
    for (auto* f : faixas) algumSolo = algumSolo || f->solo;

    const bool toca = tocandoFlag.load();
    const int64 p = pos.load();

    for (auto* f : faixas)
    {
        const bool ouvirEntrada = f->armada && f->monitorar && inL != nullptr;
        const bool tocarAudio = toca && ! f->armada && f->audio.getNumSamples() > 0;
        if (! ouvirEntrada && ! tocarAudio) continue;
        if (n > f->trab.getNumSamples()) continue;   // bloco maior que o preparado: pula (não aloca aqui)

        f->trab.clear (0, n);
        if (ouvirEntrada)
        {
            const float* a = f->entrada == 1 ? (inR != nullptr ? inR : inL) : inL;
            const float* b = f->entrada == 2 ? (inR != nullptr ? inR : inL) : a;
            f->trab.copyFrom (0, 0, a, n);
            f->trab.copyFrom (1, 0, b, n);
        }
        else
        {
            const int64 ini = jmax (p, f->inicio), fim = jmin (p + n, f->fim());
            if (ini < fim)
            {
                const int destino = (int) (ini - p), origem = (int) (ini - f->inicio), qtd = (int) (fim - ini);
                const int c1 = jmin (1, f->audio.getNumChannels() - 1);
                f->trab.copyFrom (0, destino, f->audio, 0, origem, qtd);
                f->trab.copyFrom (1, destino, f->audio, c1, origem, qtd);
            }
        }

        processarCadeia (*f, n);

        if (f->mudo || (algumSolo && ! f->solo)) continue;
        const float gl = f->volume * jmin (1.0f, 1.0f - f->pan), gr = f->volume * jmin (1.0f, 1.0f + f->pan);
        if (nOut > 1)
        {
            FloatVectorOperations::addWithMultiply (out[0], f->trab.getReadPointer (0), gl, n);
            FloatVectorOperations::addWithMultiply (out[1], f->trab.getReadPointer (1), gr, n);
        }
        else
        {
            FloatVectorOperations::addWithMultiply (out[0], f->trab.getReadPointer (0), 0.5f * gl, n);
            FloatVectorOperations::addWithMultiply (out[0], f->trab.getReadPointer (1), 0.5f * gr, n);
        }
    }

    if (toca) pos = p + n;
}

//==============================================================================
void Sessao::tocar (double s)   { pos = (int64) (jmax (0.0, s) * taxa); tocandoFlag = true; }
void Sessao::parar()            { tocandoFlag = false; }
void Sessao::posicionar (double s) { if (! tocandoFlag.load()) pos = (int64) (jmax (0.0, s) * taxa); }

int Sessao::receberGravacao (const std::vector<float>& L, const std::vector<float>& R, int latencia)
{
    int recebidas = 0;
    const int total = (int) L.size();
    const int corte = jlimit (0, total, latencia);   // o começo atrasado sai: a faixa fica no tempo da base
    const int qtd = total - corte;
    if (qtd <= 0) return 0;

    const ScopedLock sl (trava);
    for (auto* f : faixas)
    {
        if (! f->armada) continue;
        const bool estereo = f->entrada == 2;
        AudioBuffer<float> b (estereo ? 2 : 1, qtd);
        const auto& fonte = f->entrada == 1 ? R : L;
        b.copyFrom (0, 0, fonte.data() + corte, qtd);
        if (estereo) b.copyFrom (1, 0, R.data() + corte, qtd);
        f->audio = std::move (b);
        f->inicio = inicioGrav.load();
        f->armada = false;
        gravarWav (*f);
        calcularPicos (*f);
        ++recebidas;
    }
    return recebidas;
}

//==============================================================================
Faixa* Sessao::achar (int id) const
{
    for (auto* f : faixas) if (f->id == id) return f;
    return nullptr;
}

int Sessao::novaFaixa (const String& nome, int entrada, bool armada)
{
    auto f = std::make_unique<Faixa>();
    f->entrada = jlimit (0, 2, entrada);
    f->armada = armada;
    const ScopedLock sl (trava);
    f->id = proximoId++;
    f->nome = nome.isNotEmpty() ? nome : t ("Faixa ") + String (f->id);
    prepararFaixa (*f);
    const int id = f->id;
    faixas.add (f.release());
    return id;
}

void Sessao::colocarAudio (int id, AudioBuffer<float>&& audio, int64 inicio)
{
    const ScopedLock sl (trava);
    if (auto* f = achar (id))
    {
        f->audio = std::move (audio);
        f->inicio = inicio;
        gravarWav (*f);
        calcularPicos (*f);
    }
}

String Sessao::importar (const File& arquivo)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (arquivo));
    if (r == nullptr) return t ("não consegui abrir este arquivo de áudio");
    if (r->lengthInSamples <= 0 || r->lengthInSamples > (int64) (r->sampleRate * 60 * 60)) return t ("arquivo vazio ou longo demais");

    const int canais = jlimit (1, 2, (int) r->numChannels);
    AudioBuffer<float> orig (canais, (int) r->lengthInSamples);
    r->read (&orig, 0, (int) r->lengthInSamples, 0, true, canais > 1);

    AudioBuffer<float> audio;
    if (std::abs (r->sampleRate - taxa) < 0.5)
        audio = std::move (orig);
    else
    {
        // outra taxa: converte para a da sessão (interpolação de alta qualidade)
        const double razao = r->sampleRate / taxa;
        const int saida = (int) std::floor (orig.getNumSamples() / razao);
        audio.setSize (canais, saida);
        for (int c = 0; c < canais; ++c)
        {
            Interpolators::WindowedSinc interp;
            interp.process (razao, orig.getReadPointer (c), audio.getWritePointer (c), saida);
        }
    }

    const int id = novaFaixa (arquivo.getFileNameWithoutExtension(), 0, false);
    {
        const ScopedLock sl (trava);
        if (auto* f = achar (id))
        {
            f->audio = std::move (audio);
            f->inicio = 0;
            gravarWav (*f);
            calcularPicos (*f);
        }
    }
    return {};
}

bool Sessao::remover (int id)
{
    if (auto* f = achar (id))
        for (auto* p : f->plugins) janelas.erase (p);
    const ScopedLock sl (trava);
    for (int i = 0; i < faixas.size(); ++i)
        if (faixas[i]->id == id)
        {
            if (pastaSessao.isDirectory() && faixas[i]->arquivo.isNotEmpty())
                pastaSessao.getChildFile (faixas[i]->arquivo).deleteFile();
            faixas.remove (i);
            return true;
        }
    return false;
}

bool Sessao::ajustar (int id, const var& p)
{
    const ScopedLock sl (trava);
    auto* f = achar (id);
    if (f == nullptr) return false;
    if (p.hasProperty ("volume"))    f->volume = jlimit (0.0f, 2.0f, (float) p["volume"]);
    if (p.hasProperty ("pan"))       f->pan = jlimit (-1.0f, 1.0f, (float) p["pan"]);
    if (p.hasProperty ("mudo"))      f->mudo = (bool) p["mudo"];
    if (p.hasProperty ("solo"))      f->solo = (bool) p["solo"];
    if (p.hasProperty ("armada"))    f->armada = (bool) p["armada"];
    if (p.hasProperty ("monitorar")) f->monitorar = (bool) p["monitorar"];
    if (p.hasProperty ("entrada"))   f->entrada = jlimit (0, 2, (int) p["entrada"]);
    if (p.hasProperty ("nome"))      f->nome = URL::removeEscapeChars (p["nome"].toString()).substring (0, 60);   // chega codificado (acentos)
    if (p.hasProperty ("moverMs"))   f->inicio += (int64) std::llround ((double) p["moverMs"] * taxa / 1000.0);
    return true;
}

var Sessao::estado() const
{
    const ScopedLock sl (trava);
    Array<var> lista;
    for (auto* f : faixas)
    {
        auto* o = new DynamicObject();
        o->setProperty ("id", f->id);
        o->setProperty ("nome", f->nome);
        o->setProperty ("inicio", (double) f->inicio / taxa);
        o->setProperty ("dur", (double) f->audio.getNumSamples() / taxa);
        o->setProperty ("canais", f->audio.getNumChannels());
        o->setProperty ("volume", f->volume);
        o->setProperty ("pan", f->pan);
        o->setProperty ("mudo", f->mudo);
        o->setProperty ("solo", f->solo);
        o->setProperty ("armada", f->armada);
        o->setProperty ("monitorar", f->monitorar);
        o->setProperty ("entrada", f->entrada);
        Array<var> pl;
        for (int i = 0; i < f->plugins.size(); ++i)
        {
            auto* q = new DynamicObject();
            q->setProperty ("nome", f->plugins[i]->getName());
            q->setProperty ("ligado", (bool) f->ligado[(size_t) i]);
            pl.add (var (q));
        }
        o->setProperty ("plugins", pl);
        lista.add (var (o));
    }
    auto* o = new DynamicObject();
    o->setProperty ("nome", nomeSessao);
    o->setProperty ("taxa", taxa);
    o->setProperty ("pos", posicao());
    o->setProperty ("tocando", tocandoFlag.load());
    o->setProperty ("faixas", lista);
    o->setProperty ("buscandoPlugins", buscando.load());
    o->setProperty ("numPlugins", conhecidos.getNumTypes());
    return var (o);
}

var Sessao::picos (int id) const
{
    const ScopedLock sl (trava);
    Array<var> a;
    if (auto* f = achar (id))
        for (auto v : f->picos) a.add ((int) std::lround (jmin (1.0f, v) * 1000.0f));
    return a;
}

void Sessao::calcularPicos (Faixa& f) const
{
    const int passo = jmax (1, (int) (taxa / 100.0));
    const int n = f.audio.getNumSamples();
    f.picos.assign ((size_t) ((n + passo - 1) / passo), 0.0f);
    for (int c = 0; c < f.audio.getNumChannels(); ++c)
    {
        const float* s = f.audio.getReadPointer (c);
        for (int i = 0; i < n; ++i)
        {
            auto& v = f.picos[(size_t) (i / passo)];
            v = jmax (v, std::abs (s[i]));
        }
    }
}

void Sessao::gravarWav (Faixa& f)
{
    if (! pastaSessao.isDirectory() || f.audio.getNumSamples() == 0) return;
    if (f.arquivo.isNotEmpty()) pastaSessao.getChildFile (f.arquivo).deleteFile();
    f.arquivo = "faixa-" + String (f.id) + "-" + String (Time::currentTimeMillis()) + ".wav";
    auto arq = pastaSessao.getChildFile (f.arquivo);
    std::unique_ptr<FileOutputStream> s (arq.createOutputStream());
    if (s == nullptr) return;
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (s.get(), taxa, (unsigned) f.audio.getNumChannels(), 32, {}, 0));
    if (w == nullptr) return;
    s.release();
    w->writeFromAudioSampleBuffer (f.audio, 0, f.audio.getNumSamples());
}

//==============================================================================
// Plugins
void Sessao::buscarPlugins (bool forcar)
{
    if (buscando.load()) return;
    const File lista = pastaRaiz.getChildFile ("plugins.xml");
    if (! forcar && lista.existsAsFile())
    {
        if (auto xml = parseXML (lista)) conhecidos.recreateFromXml (*xml);
        return;
    }
    if (busca != nullptr) busca->stopThread (1000);
    buscando = true;
    busca = std::make_unique<Tarefa> ([this, lista]
    {
        // um plugin que derruba a busca fica anotado no "pedal" e é pulado na próxima vez
        const File pedal = pastaRaiz.getChildFile ("plugins-busca.tmp");
        for (auto* formato : formatos.getFormats())
        {
            PluginDirectoryScanner scanner (conhecidos, *formato, formato->getDefaultLocationsToSearch(), true, pedal, false);
            String nome;
            while (! Thread::currentThreadShouldExit() && scanner.scanNextFile (true, nome)) {}
        }
        if (auto xml = conhecidos.createXml()) xml->writeTo (lista);
        buscando = false;
    });
    busca->startThread();
}

var Sessao::listaPlugins() const
{
    Array<var> a;
    auto tipos = conhecidos.getTypes();
    for (int i = 0; i < tipos.size(); ++i)
    {
        auto* o = new DynamicObject();
        o->setProperty ("indice", i);
        o->setProperty ("nome", tipos[i].name);
        o->setProperty ("fabricante", tipos[i].manufacturerName);
        o->setProperty ("categoria", tipos[i].category);
        o->setProperty ("instrumento", tipos[i].isInstrument);
        a.add (var (o));
    }
    auto* o = new DynamicObject();
    o->setProperty ("buscando", buscando.load());
    o->setProperty ("plugins", a);
    return var (o);
}

String Sessao::criarPlugin (Faixa& f, const PluginDescription& desc, const String& estadoB64, bool ligar)
{
    String erro;
    auto inst = formatos.createPluginInstance (desc, taxa, bloco, erro);
    if (inst == nullptr) return erro.isNotEmpty() ? erro : t ("não consegui carregar o plugin");

    inst->enableAllBuses();
    AudioProcessor::BusesLayout estereo;
    if (inst->getBusCount (true) > 0)  estereo.inputBuses.add (AudioChannelSet::stereo());
    if (inst->getBusCount (false) > 0) estereo.outputBuses.add (AudioChannelSet::stereo());
    for (int i = 1; i < inst->getBusCount (true); ++i)  estereo.inputBuses.add (AudioChannelSet::disabled());
    for (int i = 1; i < inst->getBusCount (false); ++i) estereo.outputBuses.add (AudioChannelSet::disabled());
    if (inst->checkBusesLayoutSupported (estereo)) inst->setBusesLayout (estereo);

    if (estadoB64.isNotEmpty())
    {
        MemoryBlock mb;
        if (mb.fromBase64Encoding (estadoB64)) inst->setStateInformation (mb.getData(), (int) mb.getSize());
    }
    inst->prepareToPlay (taxa, bloco);

    const ScopedLock sl (trava);
    f.plugins.add (inst.release());
    f.ligado.push_back (ligar);
    f.descricoes.add (desc);
    prepararFaixa (f);
    return {};
}

String Sessao::adicionarPlugin (int id, int indice)
{
    auto tipos = conhecidos.getTypes();
    if (! isPositiveAndBelow (indice, tipos.size())) return t ("plugin não encontrado");
    auto* f = achar (id);
    if (f == nullptr) return t ("faixa não encontrada");
    return criarPlugin (*f, tipos[indice], {}, true);
}

String Sessao::adicionarPluginDeArquivo (int id, const File& arquivo)
{
    auto* f = achar (id);
    if (f == nullptr) return t ("faixa não encontrada");
    for (auto* formato : formatos.getFormats())
    {
        OwnedArray<PluginDescription> tipos;
        formato->findAllTypesForFile (tipos, arquivo.getFullPathName());
        if (! tipos.isEmpty())
        {
            conhecidos.addType (*tipos[0]);
            return criarPlugin (*f, *tipos[0], {}, true);
        }
    }
    return t ("nenhum plugin neste arquivo");
}

bool Sessao::removerPlugin (int id, int slot)
{
    auto* f = achar (id);
    if (f == nullptr || ! isPositiveAndBelow (slot, f->plugins.size())) return false;
    janelas.erase (f->plugins[slot]);
    const ScopedLock sl (trava);
    f->plugins.remove (slot);
    f->ligado.erase (f->ligado.begin() + slot);
    f->descricoes.remove (slot);
    prepararFaixa (*f);
    return true;
}

bool Sessao::ligarPlugin (int id, int slot, bool ligar)
{
    const ScopedLock sl (trava);
    auto* f = achar (id);
    if (f == nullptr || ! isPositiveAndBelow (slot, f->plugins.size())) return false;
    f->ligado[(size_t) slot] = ligar;
    return true;
}

bool Sessao::abrirPlugin (int id, int slot)
{
    auto* f = achar (id);
    if (f == nullptr || ! isPositiveAndBelow (slot, f->plugins.size())) return false;
    auto* p = f->plugins[slot];
    if (auto it = janelas.find (p); it != janelas.end()) { it->second->toFront (true); return true; }

    auto* editor = p->hasEditor() ? p->createEditorIfNeeded() : nullptr;
    if (editor == nullptr) editor = new GenericAudioProcessorEditor (*p);
    auto janela = std::make_unique<JanelaPlugin> (p->getName() + " - " + f->nome);
    janela->setUsingNativeTitleBar (true);
    janela->setContentOwned (editor, true);
    janela->setResizable (editor->isResizable(), false);
    janela->aoFechar = [this, p] { MessageManager::callAsync ([this, p] { janelas.erase (p); }); };
    janela->centreWithSize (janela->getWidth(), janela->getHeight());
    janela->setVisible (true);
    if (auto* peer = janela->getPeer()) barraEscura (peer->getNativeHandle());
    janelas[p] = std::move (janela);
    return true;
}

//==============================================================================
// Mixagem fora do tempo real: mesma soma da reprodução, do começo ao fim da última faixa
String Sessao::exportar (const File& destino, double& duracao)
{
    parar();
    const ScopedLock sl (trava);
    int64 fim = 0;
    bool algumSolo = false, temPlugin = false;
    for (auto* f : faixas) { algumSolo = algumSolo || f->solo; temPlugin = temPlugin || ! f->plugins.isEmpty(); }
    for (auto* f : faixas)
        if (! f->mudo && (! algumSolo || f->solo) && f->audio.getNumSamples() > 0) fim = jmax (fim, f->fim());
    if (fim <= 0) return t ("nenhuma faixa com áudio para mixar");
    const int64 total = fim + (int64) (taxa * (temPlugin ? 2.0 : 0.25));   // sobra para o fim dos efeitos

    destino.deleteFile();
    std::unique_ptr<FileOutputStream> s (destino.createOutputStream());
    if (s == nullptr) return t ("não consegui criar o arquivo da mixagem");
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (s.get(), taxa, 2, 32, {}, 0));
    if (w == nullptr) return t ("não consegui criar o arquivo da mixagem");
    s.release();

    for (auto* f : faixas) for (auto* p : f->plugins) { p->setNonRealtime (true); p->reset(); }
    AudioBuffer<float> mix (2, bloco);
    for (int64 p = 0; p < total; p += bloco)
    {
        const int n = (int) jmin ((int64) bloco, total - p);
        mix.clear();
        for (auto* f : faixas)
        {
            if (f->audio.getNumSamples() == 0 || f->mudo || (algumSolo && ! f->solo)) continue;
            f->trab.clear (0, n);
            const int64 ini = jmax (p, f->inicio), fimF = jmin (p + n, f->fim());
            if (ini < fimF)
            {
                const int c1 = jmin (1, f->audio.getNumChannels() - 1);
                f->trab.copyFrom (0, (int) (ini - p), f->audio, 0, (int) (ini - f->inicio), (int) (fimF - ini));
                f->trab.copyFrom (1, (int) (ini - p), f->audio, c1, (int) (ini - f->inicio), (int) (fimF - ini));
            }
            processarCadeia (*f, n);
            const float gl = f->volume * jmin (1.0f, 1.0f - f->pan), gr = f->volume * jmin (1.0f, 1.0f + f->pan);
            mix.addFrom (0, 0, f->trab, 0, 0, n, gl);
            mix.addFrom (1, 0, f->trab, 1, 0, n, gr);
        }
        w->writeFromAudioSampleBuffer (mix, 0, n);
    }
    for (auto* f : faixas) for (auto* p : f->plugins) { p->setNonRealtime (false); p->reset(); }
    duracao = (double) total / taxa;
    return {};
}

//==============================================================================
// Sessões salvas: uma pasta por sessão, com os WAVs das faixas e o sessao.json
void Sessao::usarPasta (const File& raiz)
{
    const auto ultima = prepararPasta (raiz);
    if (ultima.isNotEmpty()) abrir (ultima);
    else nova();
}

var Sessao::listarSessoes() const
{
    Array<File> pastas = pastaRaiz.getChildFile ("sessoes").findChildFiles (File::findDirectories, false);
    std::sort (pastas.begin(), pastas.end(), [] (const File& a, const File& b)
               { return a.getChildFile ("sessao.json").getLastModificationTime() > b.getChildFile ("sessao.json").getLastModificationTime(); });
    Array<var> a;
    for (auto& p : pastas)
    {
        auto* o = new DynamicObject();
        o->setProperty ("nome", p.getFileName());
        o->setProperty ("atual", p == pastaSessao);
        a.add (var (o));
    }
    return a;
}

String Sessao::nova()
{
    parar();
    janelas.clear();
    {
        const ScopedLock sl (trava);
        faixas.clear();
        proximoId = 1;
    }
    String nome = nomeSeguro (t ("Sessão ") + Time::getCurrentTime().formatted ("%d-%m %Hh%M"));
    auto pasta = pastaRaiz.getChildFile ("sessoes").getChildFile (nome);
    for (int i = 2; pasta.exists(); ++i) pasta = pastaRaiz.getChildFile ("sessoes").getChildFile (nome + " (" + String (i) + ")");
    pasta.createDirectory();
    pastaSessao = pasta;
    nomeSessao = pasta.getFileName();
    salvar();
    return {};
}

void Sessao::salvar()
{
    if (! pastaSessao.isDirectory()) return;
    auto* raiz = new DynamicObject();
    Array<var> lista;
    {
        const ScopedLock sl (trava);
        raiz->setProperty ("versao", 1);
        raiz->setProperty ("taxa", taxa);
        raiz->setProperty ("proximoId", proximoId);
        for (auto* f : faixas)
        {
            auto* o = new DynamicObject();
            o->setProperty ("id", f->id);
            o->setProperty ("nome", f->nome);
            o->setProperty ("arquivo", f->arquivo);
            o->setProperty ("inicio", (int64) f->inicio);
            o->setProperty ("volume", f->volume);
            o->setProperty ("pan", f->pan);
            o->setProperty ("mudo", f->mudo);
            o->setProperty ("solo", f->solo);
            o->setProperty ("entrada", f->entrada);
            o->setProperty ("monitorar", f->monitorar);
            Array<var> pl;
            for (int i = 0; i < f->plugins.size(); ++i)
            {
                auto* q = new DynamicObject();
                MemoryBlock mb;
                f->plugins[i]->getStateInformation (mb);
                const auto& d = f->descricoes.getReference (i);
                q->setProperty ("id", d.createIdentifierString());
                if (auto xml = d.createXml()) q->setProperty ("descricao", xml->toString (XmlElement::TextFormat().singleLine().withoutHeader()));
                q->setProperty ("nome", f->plugins[i]->getName());
                q->setProperty ("ligado", (bool) f->ligado[(size_t) i]);
                q->setProperty ("estado", mb.toBase64Encoding());
                pl.add (var (q));
            }
            o->setProperty ("plugins", pl);
            lista.add (var (o));
        }
    }
    raiz->setProperty ("faixas", lista);
    pastaSessao.getChildFile ("sessao.json").replaceWithText (JSON::toString (var (raiz)), false, false, "\n");
    pastaRaiz.getChildFile ("ultima-sessao.txt").replaceWithText (pastaSessao.getFileName());
}

// Abrir uma sessão em duas partes: ler os WAVs (pode ser em segundo plano, sem travar a janela)
// e depois trocar as faixas e carregar os plugins (na linha da janela, como os plugins exigem).
std::unique_ptr<Sessao::Carga> Sessao::carregar (const String& nome) const
{
    auto c = std::make_unique<Carga>();
    c->pasta = pastaRaiz.getChildFile ("sessoes").getChildFile (nomeSeguro (nome));
    c->json = JSON::parse (c->pasta.getChildFile ("sessao.json"));
    if (! c->json.isObject()) return nullptr;
    AudioFormatManager fm;
    fm.registerBasicFormats();
    if (auto* lista = c->json["faixas"].getArray())
        for (auto& v : *lista)
        {
            auto f = std::make_unique<Faixa>();
            f->id = (int) v["id"];
            f->nome = v["nome"].toString();
            f->arquivo = v["arquivo"].toString();
            f->inicio = (int64) v["inicio"];
            f->volume = (float) v["volume"];
            f->pan = (float) v["pan"];
            f->mudo = (bool) v["mudo"];
            f->solo = (bool) v["solo"];
            f->entrada = (int) v["entrada"];
            f->monitorar = (bool) v["monitorar"];
            if (f->arquivo.isNotEmpty())
                if (std::unique_ptr<AudioFormatReader> r { fm.createReaderFor (c->pasta.getChildFile (f->arquivo)) })
                {
                    f->audio.setSize ((int) r->numChannels, (int) r->lengthInSamples);
                    r->read (&f->audio, 0, (int) r->lengthInSamples, 0, true, true);
                }
            calcularPicos (*f);
            c->faixas.add (f.release());
        }
    return c;
}

String Sessao::aplicar (std::unique_ptr<Carga> c)
{
    if (c == nullptr) return t ("sessão não encontrada");
    parar();
    janelas.clear();
    {
        const ScopedLock sl (trava);
        faixas.clear();
    }
    pastaSessao = c->pasta;
    nomeSessao = c->pasta.getFileName();

    StringArray faltando;
    auto* lista = c->json["faixas"].getArray();
    for (int i = 0; i < c->faixas.size(); ++i)
    {
        auto* bruto = c->faixas[i];
        {
            const ScopedLock sl (trava);
            prepararFaixa (*bruto);
            faixas.add (c->faixas.removeAndReturn (i--));
        }
        const var v = lista != nullptr && isPositiveAndBelow (faixas.size() - 1, lista->size()) ? (*lista)[faixas.size() - 1] : var();
        if (auto* pl = v["plugins"].getArray())
            for (auto& q : *pl)
            {
                // a descrição salva diz exatamente qual plugin era; o identificador é o plano B
                std::optional<PluginDescription> desc;
                if (auto xml = parseXML (q["descricao"].toString()))
                {
                    PluginDescription d;
                    if (d.loadFromXml (*xml)) desc = d;
                }
                if (! desc)
                    for (auto& d : conhecidos.getTypes())
                        if (d.createIdentifierString() == q["id"].toString()) { desc = d; break; }
                const bool achou = desc && criarPlugin (*bruto, *desc, q["estado"].toString(), (bool) q["ligado"]).isEmpty();
                if (! achou) faltando.add (q["nome"].toString());
            }
    }
    {
        const ScopedLock sl (trava);
        proximoId = jmax ((int) c->json["proximoId"], 1);
        for (auto* f : faixas) proximoId = jmax (proximoId, f->id + 1);
    }
    pastaRaiz.getChildFile ("ultima-sessao.txt").replaceWithText (pastaSessao.getFileName());
    return faltando.isEmpty() ? String() : t ("plugins não encontrados: ") + faltando.joinIntoString (", ");
}

String Sessao::abrir (const String& nome) { return aplicar (carregar (nome)); }

String Sessao::prepararPasta (const File& raiz)
{
    pastaRaiz = raiz;
    pastaRaiz.createDirectory();
    raiz.getChildFile ("sessoes").createDirectory();
    buscarPlugins (false);
    const String ultima = raiz.getChildFile ("ultima-sessao.txt").loadFileAsString().trim();
    return ultima.isNotEmpty() && raiz.getChildFile ("sessoes").getChildFile (ultima).getChildFile ("sessao.json").existsAsFile()
               ? ultima : String();
}
