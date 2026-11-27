// Stumix Desktop: janela, interface web embutida e ponte com o motor nativo
// Copyright (C) 2026 Fbiol. Licença GPLv3 (ver LICENSE na raiz do repositório).
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "AudioEngine.h"
#include "BinaryData.h"
#include "Lancador.h"

using namespace juce;
using Opcoes = WebBrowserComponent::Options;

//==============================================================================
/** Atualização pelo próprio app: consulta a última versão no GitHub, baixa o instalador
    e o executa em modo silencioso; o instalador fecha o app, troca os arquivos e o reabre. */
// Tempo de cada etapa da abertura, em ms desde que o app começou (aparece em ⓘ e em abertura.log)
namespace Abertura
{
    static double inicio = 0;
    static bool modoTeste = false;   // funções só para o teste automático ficam desligadas no uso normal
    static NamedValueSet marcas;
    static void marcar (const char* etapa) { marcas.set (etapa, std::round (Time::getMillisecondCounterHiRes() - inicio)); }
    static File pasta() { return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Estudio"); }
}

namespace Atualizador
{
    // textos com acento: sempre como UTF-8 (sem isso a interface mostra "nÃ£o")
    static String t (const char* s) { return String::fromUTF8 (s); }

    static const char* api = "https://api.github.com/repos/Fbi0l/estudio/releases/latest";

    static int comparar (const String& a, const String& b)   // -1, 0, 1 (versões x.y.z)
    {
        auto pa = StringArray::fromTokens (a, ".", ""), pb = StringArray::fromTokens (b, ".", "");
        for (int i = 0; i < 3; ++i)
        {
            const int x = pa[i].getIntValue(), y = pb[i].getIntValue();
            if (x != y) return x < y ? -1 : 1;
        }
        return 0;
    }

    static var verificar()
    {
        auto* r = new DynamicObject();
        r->setProperty ("atual", JUCE_APPLICATION_VERSION_STRING);
        if (empacotadoNaLoja()) { r->setProperty ("loja", true); return var (r); }   // a Store atualiza
        auto opcoes = URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                          .withExtraHeaders ("User-Agent: Stumix-Desktop\r\nAccept: application/vnd.github+json")
                          .withConnectionTimeoutMs (15000);
        std::unique_ptr<InputStream> in (URL (api).createInputStream (opcoes));
        if (in == nullptr) { r->setProperty ("erro", t ("sem conexão com o GitHub")); return var (r); }
        auto json = JSON::parse (in->readEntireStreamAsString());
        const String tag = json["tag_name"].toString();
        if (! tag.startsWith ("desktop-v")) { r->setProperty ("erro", t ("nenhuma versão publicada ainda")); return var (r); }
        const String versao = tag.fromFirstOccurrenceOf ("desktop-v", false, false);
        // prefere o pacote portátil: a troca do próprio .exe não passa pelo instalador, que o
        // Controle Inteligente de Aplicativos do Windows 11 bloqueia (arquivo .tmp sem assinatura)
        String url, urlInstalador;
        if (auto* assets = json["assets"].getArray())
            for (auto& a : *assets)
            {
                const String nome = a["name"].toString();
                if (nome.endsWith ("portatil.zip")) url = a["browser_download_url"].toString();
                if (nome.endsWith ("Instalador.exe")) urlInstalador = a["browser_download_url"].toString();
            }
        if (url.isEmpty()) url = urlInstalador;
        r->setProperty ("versao", versao);
        r->setProperty ("url", url);
        r->setProperty ("notas", json["body"].toString());
        r->setProperty ("nova", url.isNotEmpty() && comparar (JUCE_APPLICATION_VERSION_STRING, versao) < 0);
        return var (r);
    }

    // Atualização sem instalador: tira o Stumix.exe do pacote, troca o arquivo do app (o Windows
    // deixa renomear um .exe em uso) e abre a versão nova, que espera esta fechar.
    static var trocarExecutavel (const File& pacote, DynamicObject* r)
    {
        auto falha = [r] (const String& e) { r->setProperty ("ok", false); r->setProperty ("erro", e); return var (r); };
        ZipFile zip (pacote);
        const int i = zip.getIndexOfFileName ("Stumix.exe", true);
        if (i < 0) return falha (t ("o pacote baixado não tem o Stumix.exe"));
        const File exe = File::getSpecialLocation (File::currentExecutableFile);
        const File novo = exe.getSiblingFile ("Stumix.novo.exe"), antigo = exe.getSiblingFile ("Stumix.antigo.exe");
        novo.deleteFile();
        {
            std::unique_ptr<InputStream> in (zip.createStreamForEntry (i));
            FileOutputStream out (novo);
            if (in == nullptr || ! out.openedOk() || out.writeFromInputStream (*in, -1) < 100000)
                return falha (t ("não consegui gravar a versão nova na pasta do app"));
        }
        antigo.deleteFile();
        if (! exe.moveFileTo (antigo)) { novo.deleteFile(); return falha (t ("não consegui substituir o app (pasta protegida?)")); }
        if (! novo.moveFileTo (exe)) { antigo.moveFileTo (exe); return falha (t ("não consegui substituir o app")); }
        unsigned long codigo = 0;
        const String args = "--esperar=" + String ((int64) idDoProcesso());
        if (! lancarInstalador (exe.getFullPathName().toWideCharPointer(), args.toWideCharPointer(), codigo))
        {
            exe.deleteFile(); antigo.moveFileTo (exe);   // volta para a versão que funcionava
            return falha (t ("não consegui abrir a versão nova (código ") + String ((int) codigo) + ")");
        }
        r->setProperty ("ok", true);
        return var (r);
    }

    static var instalar (const String& url)
    {
        auto* r = new DynamicObject();
        if (! url.startsWith ("https://github.com/Fbi0l/estudio/releases/download/"))
        { r->setProperty ("ok", false); r->setProperty ("erro", t ("endereço inesperado")); return var (r); }
        auto destino = File::getSpecialLocation (File::tempDirectory)
                           .getChildFile (url.endsWithIgnoreCase (".zip") ? "Stumix-Atualizacao.zip" : "Stumix-Atualizacao.exe");
        destino.deleteFile();
        auto opcoes = URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                          .withExtraHeaders ("User-Agent: Stumix-Desktop").withConnectionTimeoutMs (30000);
        std::unique_ptr<InputStream> in (URL (url).createInputStream (opcoes));
        bool ok = false;
        if (in != nullptr)
        {
            FileOutputStream out (destino);
            ok = out.openedOk() && out.writeFromInputStream (*in, -1) > 100000;
            out.flush();
        }
        if (! ok) { r->setProperty ("ok", false); r->setProperty ("erro", t ("o download não terminou")); return var (r); }
        if (url.endsWithIgnoreCase (".zip"))
            return trocarExecutavel (destino, r);
        unsigned long codigo = 0;
        ok = lancarInstalador (destino.getFullPathName().toWideCharPointer(),
                               L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS", codigo);
        r->setProperty ("ok", ok);
        if (! ok)
            r->setProperty ("erro", codigo == 1223 ? t ("a instalação foi cancelada")
                                                   : t ("não consegui abrir o instalador (código ") + String ((int) codigo) + ")");
        return var (r);
    }
}

//==============================================================================
/** A interface do Stumix (a mesma do PWA) rodando dentro do app, falando com o motor nativo. */
/** Só para o teste automático: faz o papel da placa, mandando um tom ao motor em tempo real. */
class PlacaDeTeste : public HighResolutionTimer
{
public:
    explicit PlacaDeTeste (AudioEngine& e) : engine (e) { engine.taxaParaTeste (48000.0); startTimer (10); }
    ~PlacaDeTeste() override { stopTimer(); }
    void hiResTimerCallback() override
    {
        float l[480], r[480], o0[480], o1[480];
        for (int i = 0; i < 480; ++i) { l[i] = 0.3f * (float) std::sin (fase); r[i] = 0.0f; fase += 2.0 * MathConstants<double>::pi * 220.0 / 48000.0; }
        const float* in[2] = { l, r }; float* out[2] = { o0, o1 };
        engine.processar (in, 2, out, 2, 480);
    }
private:
    AudioEngine& engine; double fase = 0;
};

class EstudioView : public Component, private Timer
{
public:
    EstudioView (AudioEngine& e, const String& roteiroTeste) : engine (e), web (opcoes()), roteiro (roteiroTeste)
    {
        // sessões da multipista (no teste automático, numa pasta temporária)
        const auto base = roteiroTeste.isNotEmpty()
            ? File::getSpecialLocation (File::tempDirectory).getChildFile ("Stumix-teste-multipista")
            : File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Estudio").getChildFile ("multipista");
        if (roteiroTeste.isNotEmpty()) base.deleteRecursively();
        // a última sessão da multipista é lida em segundo plano, sem segurar a janela
        const auto ultima = engine.sessao.prepararPasta (base);
        if (ultima.isEmpty()) engine.sessao.nova();
        else abrirSessaoEmSegundoPlano (ultima, [] (const String&) { Abertura::marcar ("sessao"); });

        addAndMakeVisible (web);
        web.goToURL (WebBrowserComponent::getResourceProviderRoot());
        startTimerHz (30);
        if (roteiro.isNotEmpty())
        {
            placaTeste = std::make_unique<PlacaDeTeste> (engine);
            Timer::callAfterDelay (6000, [this] { web.evaluateJavascript (roteiro); });
        }
    }

    void resized() override { web.setBounds (getLocalBounds()); }

private:
    AudioEngine& engine;
    WebBrowserComponent web;

    Opcoes opcoes()
    {
        auto o = Opcoes{};
       #if JUCE_WINDOWS
        o = o.withBackend (Opcoes::Backend::webview2)
             .withWinWebView2Options (Opcoes::WinWebView2{}
                 .withUserDataFolder (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Estudio")) // pasta mantida: guarda os projetos de quem já usava o app
                 .withBackgroundColour (Colour (0xff141518))
                 .withStatusBarDisabled());
       #endif
        return o.withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withResourceProvider ([this] (const String& u) { return recurso (u); })
            .withNativeFunction ("estudioListar", [this] (const Array<var>& a, auto done) { done (engine.listar (a.size() > 0 && (bool) a[0])); })
            .withNativeFunction ("estudioTempos", [] (const Array<var>&, auto done)
            {
                // chamado quando a interface termina de abrir: anota e devolve o tempo de cada etapa
                Abertura::marcar ("pronto");
                auto* o = new DynamicObject();
                String linha = Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S");
                for (auto& m : Abertura::marcas) { o->setProperty (m.name, m.value); linha << "  " << m.name.toString() << "=" << m.value.toString() << "ms"; }
                Abertura::pasta().createDirectory();
                Abertura::pasta().getChildFile ("abertura.log").appendText (linha + "\n", false, false, "\n");
                done (var (o));
            })
            .withNativeFunction ("estudioEstado", [this] (const Array<var>&, auto done) { done (engine.estado()); })
            .withNativeFunction ("estudioConfigurar", [this] (const Array<var>& a, auto done)
            {
                auto erro = engine.configurar (a.isEmpty() ? var() : a[0]);
                auto* r = new DynamicObject();
                r->setProperty ("erro", erro);
                r->setProperty ("estado", engine.estado());
                done (var (r));
            })
            .withNativeFunction ("estudioMonitor", [this] (const Array<var>& a, auto done)
            {
                engine.setMonitor (a.size() > 0 && (bool) a[0], a.size() > 1 ? (int) a[1] : 0, a.size() > 2 ? (float) (double) a[2] : 1.0f);
                done (true);
            })
            .withNativeFunction ("estudioGravar", [this] (const Array<var>&, auto done) { engine.iniciarGravacao(); done (true); })
            .withNativeFunction ("estudioParar", [this] (const Array<var>&, auto done)
            {
                auto r = engine.pararGravacao();
                if (auto* obj = r.getDynamicObject())
                    obj->setProperty ("url", WebBrowserComponent::getResourceProviderRoot() + "gravacao.wav?n=" + String (++contagem));
                done (r);
            })
            .withNativeFunction ("estudioMedirInicio", [this] (const Array<var>&, auto done) { engine.iniciarMedicao(); done (true); })
            .withNativeFunction ("estudioMedirFim", [this] (const Array<var>&, auto done) { done (engine.pararMedicao()); })
            .withNativeFunction ("estudioVersao", [] (const Array<var>&, auto done) { done (String (JUCE_APPLICATION_VERSION_STRING)); })
            .withNativeFunction ("estudioLoja", [] (const Array<var>&, auto done) { done (empacotadoNaLoja()); })
            .withNativeFunction ("estudioVerificarAtualizacao", [] (const Array<var>&, auto done)
            {
                Thread::launch ([done] { done (Atualizador::verificar()); });
            })
            .withNativeFunction ("estudioInstalarAtualizacao", [] (const Array<var>& a, auto done)
            {
                const String url = a.isEmpty() ? String() : a[0].toString();
                Thread::launch ([done, url]
                {
                    auto r = Atualizador::instalar (url);
                    done (r);
                    if ((bool) r["ok"])   // a versão nova já foi aberta (ou o instalador assume): este app fecha
                        MessageManager::callAsync ([] { JUCEApplication::getInstance()->systemRequestedQuit(); });
                });
            })
            .withNativeFunction ("estudioEco", [] (const Array<var>& a, auto done)
            {
                // teste de acentos nos dois sentidos: devolve o que chegou e um texto nativo
                auto* r = new DynamicObject();
                r->setProperty ("chegou", a.isEmpty() ? var() : a[0]);
                r->setProperty ("nativo", String (CharPointer_UTF8 ("Comunica\xc3\xa7\xc3\xb5" "es \xc3\xa9")));
                r->setProperty ("codigos", a.isEmpty() ? String() : String::toHexString ((int) a[0].toString()[0]));
                done (var (r));
            })
            .withNativeFunction ("estudioLog", [] (const Array<var>& a, auto done)
            {
                // chega codificado (encodeURIComponent): ida em ASCII puro, sem risco para os acentos
                if (! Abertura::modoTeste) { done (false); return; }
                const auto linha = a.isEmpty() ? String() : URL::removeEscapeChars (a[0].toString());
                std::printf ("%s\n", linha.toRawUTF8()); std::fflush (stdout);
                // no Windows o app não tem console: o teste automático lê este arquivo
                File::getCurrentWorkingDirectory().getChildFile ("autoteste.log").appendText (linha + "\n", false, false, "\n");
                done (true);
            })
            .withNativeFunction ("estudioSair", [] (const Array<var>&, auto done)
            {
                done (true);
                MessageManager::callAsync ([] { JUCEApplication::getInstance()->systemRequestedQuit(); });
            })
            // ---------- multipista ----------
            .withNativeFunction ("mpEstado", [this] (const Array<var>&, auto done)
            {
                auto e = engine.sessao.estado();
                if (auto* o = e.getDynamicObject())
                {
                    o->setProperty ("latenciaMs", engine.taxa() > 0 ? 1000.0 * engine.latenciaAmostras() / engine.taxa() : 0.0);
                    o->setProperty ("taxaPlaca", engine.taxa());
                    o->setProperty ("sessoes", engine.sessao.listarSessoes());
                }
                done (e);
            })
            .withNativeFunction ("mpPicos", [this] (const Array<var>& a, auto done) { done (engine.sessao.picos (a.isEmpty() ? 0 : (int) a[0])); })
            .withNativeFunction ("mpTocar", [this] (const Array<var>& a, auto done) { engine.sessao.tocar (a.isEmpty() ? 0.0 : (double) a[0]); done (true); })
            .withNativeFunction ("mpPosicionar", [this] (const Array<var>& a, auto done) { engine.sessao.posicionar (a.isEmpty() ? 0.0 : (double) a[0]); done (true); })
            .withNativeFunction ("mpGravar", [this] (const Array<var>& a, auto done)
            {
                engine.sessao.posicionar (a.isEmpty() ? 0.0 : (double) a[0]);
                engine.iniciarGravacao (false);
                engine.sessao.pedidoGravar = true;
                done (true);
            })
            .withNativeFunction ("mpParar", [this] (const Array<var>& a, auto done)
            {
                // a[0] = ajuste fino do atraso, em ms (positivo puxa a faixa gravada para trás)
                auto* r = new DynamicObject();
                engine.sessao.parar();
                if (engine.gravando())
                {
                    std::vector<float> L, R;
                    engine.tomarGravacao (L, R);
                    const double ajuste = a.isEmpty() ? 0.0 : (double) a[0];
                    const int lat = engine.latenciaAmostras() + (int) std::lround (ajuste * engine.taxa() / 1000.0);
                    r->setProperty ("gravadas", engine.sessao.receberGravacao (L, R, lat));
                    r->setProperty ("latencia", lat);
                    engine.sessao.salvar();
                }
                done (var (r));
            })
            .withNativeFunction ("mpNova", [this] (const Array<var>& a, auto done)
            {
                const int id = engine.sessao.novaFaixa ({}, a.size() > 0 ? (int) a[0] : 0, a.size() > 1 && (bool) a[1]);
                engine.sessao.salvar();
                done (id);
            })
            .withNativeFunction ("mpImportar", [this] (const Array<var>&, auto done)
            {
                seletor = std::make_unique<FileChooser> (String::fromUTF8 ("Escolha a base ou o áudio"),
                                                         File::getSpecialLocation (File::userMusicDirectory),
                                                         "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.ogg");
                seletor->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                    [this, done] (const FileChooser& fc)
                    {
                        const auto f = fc.getResult();
                        if (! f.existsAsFile()) { done (var()); return; }
                        ocupado (String::fromUTF8 ("Importando “") + f.getFileName() + String::fromUTF8 ("”…"));
                        Component::SafePointer<EstudioView> eu (this);
                        trabalho.addJob ([this, eu, f, done]
                        {
                            const auto erro = engine.sessao.importar (f);
                            MessageManager::callAsync ([this, eu, erro, done]
                            {
                                if (eu == nullptr) return;
                                engine.sessao.salvar();
                                ocupado ({});
                                done (erro.isEmpty() ? var (true) : var (erro));
                            });
                        });
                    });
            })
            .withNativeFunction ("mpImportarArquivo", [this] (const Array<var>& a, auto done)   // teste automático
            {
                if (! Abertura::modoTeste) { done (var ("indisponível")); return; }
                const auto erro = engine.sessao.importar (File (a.isEmpty() ? String() : a[0].toString()));
                done (erro.isEmpty() ? var (true) : var (erro));
            })
            .withNativeFunction ("mpAjustar", [this] (const Array<var>& a, auto done)
            {
                const bool ok = a.size() > 1 && engine.sessao.ajustar ((int) a[0], a[1]);
                engine.sessao.salvar();
                done (ok);
            })
            .withNativeFunction ("mpRemover", [this] (const Array<var>& a, auto done)
            {
                const bool ok = ! a.isEmpty() && engine.sessao.remover ((int) a[0]);
                engine.sessao.salvar();
                done (ok);
            })
            .withNativeFunction ("mpPlugins", [this] (const Array<var>&, auto done) { done (engine.sessao.listaPlugins()); })
            .withNativeFunction ("mpBuscarPlugins", [this] (const Array<var>&, auto done) { engine.sessao.buscarPlugins (true); done (true); })
            .withNativeFunction ("mpPluginAdd", [this] (const Array<var>& a, auto done)
            {
                const auto erro = a.size() > 1 ? engine.sessao.adicionarPlugin ((int) a[0], (int) a[1]) : String ("?");
                engine.sessao.salvar();
                done (erro.isEmpty() ? var (true) : var (erro));
            })
            .withNativeFunction ("mpPluginRemover", [this] (const Array<var>& a, auto done)
            {
                const bool ok = a.size() > 1 && engine.sessao.removerPlugin ((int) a[0], (int) a[1]);
                engine.sessao.salvar();
                done (ok);
            })
            .withNativeFunction ("mpPluginLigar", [this] (const Array<var>& a, auto done)
            {
                const bool ok = a.size() > 2 && engine.sessao.ligarPlugin ((int) a[0], (int) a[1], (bool) a[2]);
                engine.sessao.salvar();
                done (ok);
            })
            .withNativeFunction ("mpPluginAbrir", [this] (const Array<var>& a, auto done)
            {
                done (a.size() > 1 && engine.sessao.abrirPlugin ((int) a[0], (int) a[1]));
            })
            .withNativeFunction ("mpFaixaWav", [this] (const Array<var>& a, auto done)   // leva a faixa para Editar/Efeitos
            {
                const int id = a.isEmpty() ? -1 : (int) a[0];
                const String url = WebBrowserComponent::getResourceProviderRoot() + "faixa.wav?n=" + String (++contagem);
                trabalho.addJob ([this, id, url, done]
                {
                    const auto erro = engine.sessao.salvarFaixa (id, arquivoFaixa());
                    auto* r = new DynamicObject();
                    r->setProperty ("ok", erro.isEmpty());
                    r->setProperty ("erro", erro);
                    r->setProperty ("url", url);
                    done (var (r));
                });
            })
            .withNativeFunction ("mpReceber", [this] (const Array<var>& a, auto done)   // pedaço em base64 do WAV editado
            {
                if (a.size() > 1 && (bool) a[1]) recebido.reset();
                MemoryOutputStream mos (recebido, true);
                const bool ok = a.size() > 0 && Base64::convertFromBase64 (mos, a[0].toString());
                done (ok);
            })
            .withNativeFunction ("mpSubstituir", [this] (const Array<var>& a, auto done)  // devolve o áudio editado à faixa
            {
                const int id = a.isEmpty() ? -1 : (int) a[0];
                ocupado (String::fromUTF8 ("Devolvendo à faixa…"));
                trabalho.addJob ([this, id, done]
                {
                    const auto tmp = arquivoFaixa().getSiblingFile ("Stumix-faixa-editada.wav");
                    String erro;
                    if (! tmp.replaceWithData (recebido.getData(), recebido.getSize())) erro = String::fromUTF8 ("não consegui gravar o arquivo temporário");
                    else erro = engine.sessao.substituir (id, tmp);
                    tmp.deleteFile(); recebido.reset();
                    if (erro.isEmpty()) engine.sessao.salvar();
                    MessageManager::callAsync ([this] { ocupado ({}); web.emitEventIfBrowserIsVisible ("mpMudou", var()); });
                    done (erro.isEmpty() ? var (true) : var (erro));
                });
            })
            .withNativeFunction ("mpExportar", [this] (const Array<var>&, auto done)
            {
                const String url = WebBrowserComponent::getResourceProviderRoot() + "mixagem.wav?n=" + String (++contagem);
                trabalho.addJob ([this, url, done]
                {
                    auto* r = new DynamicObject();
                    double dur = 0;
                    const auto erro = engine.sessao.exportar (arquivoMixagem(), dur);
                    r->setProperty ("ok", erro.isEmpty());
                    r->setProperty ("erro", erro);
                    r->setProperty ("dur", dur);
                    r->setProperty ("url", url);
                    done (var (r));
                });
            })
            .withNativeFunction ("mpAbrirSessao", [this] (const Array<var>& a, auto done)
            {
                abrirSessaoEmSegundoPlano (a.isEmpty() ? String() : URL::removeEscapeChars (a[0].toString()),
                                          [done] (const String& erro) { done (erro); });
            })
            .withNativeFunction ("mpNovaSessao", [this] (const Array<var>&, auto done) { done (engine.sessao.nova()); })
            .withNativeFunction ("estudioAbrirLink", [] (const Array<var>& a, auto done)
            {
                // só endereços https abrem no navegador (nada de arquivos ou outros programas)
                if (! a.isEmpty() && a[0].toString().startsWithIgnoreCase ("https://")) URL (a[0].toString()).launchInDefaultBrowser();
                done (true);
            });
    }

    std::optional<WebBrowserComponent::Resource> recurso (const String& url)
    {
        auto caminho = url.upToFirstOccurrenceOf ("?", false, false).trimCharactersAtStart ("/");
        if (caminho.isEmpty()) caminho = "index.html";

        if (caminho == "mixagem.wav")
        {
            MemoryBlock m;
            arquivoMixagem().loadFileAsData (m);
            std::vector<std::byte> d ((size_t) m.getSize());
            if (m.getSize() > 0) std::memcpy (d.data(), m.getData(), m.getSize());
            return WebBrowserComponent::Resource { std::move (d), "audio/wav" };
        }

        if (caminho == "faixa.wav")
        {
            MemoryBlock m;
            arquivoFaixa().loadFileAsData (m);
            std::vector<std::byte> d ((size_t) m.getSize());
            if (m.getSize() > 0) std::memcpy (d.data(), m.getData(), m.getSize());
            return WebBrowserComponent::Resource { std::move (d), "audio/wav" };
        }

        if (caminho == "gravacao.wav")
        {
            const auto& m = engine.wav();
            std::vector<std::byte> d ((size_t) m.getSize());
            if (m.getSize() > 0) std::memcpy (d.data(), m.getData(), m.getSize());
            return WebBrowserComponent::Resource { std::move (d), "audio/wav" };
        }

        const auto nome = caminho.fromLastOccurrenceOf ("/", false, false);
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (nome != BinaryData::originalFilenames[i]) continue;
            int tam = 0;
            const char* dados = BinaryData::getNamedResource (BinaryData::namedResourceList[i], tam);
            std::vector<std::byte> d ((size_t) tam);
            std::memcpy (d.data(), dados, (size_t) tam);
            return WebBrowserComponent::Resource { std::move (d), tipoMime (nome) };
        }
        return std::nullopt;
    }

    static String tipoMime (const String& nome)
    {
        if (nome.endsWith (".html")) return "text/html; charset=utf-8";
        if (nome.endsWith (".js"))   return "text/javascript; charset=utf-8";
        if (nome.endsWith (".png"))  return "image/png";
        if (nome.endsWith (".webmanifest")) return "application/manifest+json";
        if (nome.endsWith (".woff2")) return "font/woff2";
        return "application/octet-stream";
    }

    // 30 vezes por segundo: nível da entrada, picos para a onda ao vivo e medições do teste
    void timerCallback() override
    {
        auto* o = new DynamicObject();
        o->setProperty ("pico", Array<var> { engine.pico (0), engine.pico (1) });
        o->setProperty ("gravando", engine.gravando());
        o->setProperty ("frames", engine.framesGravados());
        auto picos = engine.pegarPicosNovos();
        if (! picos.isEmpty()) o->setProperty ("picos", picos);
        o->setProperty ("mpPos", engine.sessao.posicao());
        o->setProperty ("mpTocando", engine.sessao.tocando());
        auto medidas = engine.pegarMedidas();
        if (! medidas.isEmpty()) o->setProperty ("medidas", medidas);
        String nomePlaca;
        const auto ev = engine.pegarEventoPlaca (nomePlaca);
        const bool caiuGravando = engine.caiuDuranteGravacao();
        if (ev.isNotEmpty() || caiuGravando)
        {
            auto* p = new DynamicObject();
            p->setProperty ("evento", ev.isNotEmpty() ? ev : String ("caiu"));
            p->setProperty ("nome", nomePlaca);
            o->setProperty ("placa", var (p));
        }
        web.emitEventIfBrowserIsVisible ("estudioNivel", var (o));
    }

    int contagem = 0;
    std::unique_ptr<FileChooser> seletor;
    ThreadPool trabalho { 1 };   // tarefas longas da multipista: a janela continua respondendo

    void ocupado (const String& texto)   // avisa a interface (mostra ou tira o "Processando…")
    {
        web.emitEventIfBrowserIsVisible ("mpOcupado", texto);
    }

    void abrirSessaoEmSegundoPlano (const String& nome, std::function<void (const String&)> fim)
    {
        Component::SafePointer<EstudioView> eu (this);
        ocupado (String::fromUTF8 ("Abrindo a sessão…"));
        trabalho.addJob ([this, eu, nome, fim]
        {
            auto carga = engine.sessao.carregar (nome);
            auto ptr = std::make_shared<std::unique_ptr<Sessao::Carga>> (std::move (carga));
            MessageManager::callAsync ([this, eu, ptr, fim]
            {
                if (eu == nullptr) return;
                const auto erro = engine.sessao.aplicar (std::move (*ptr));
                ocupado ({});
                web.emitEventIfBrowserIsVisible ("mpMudou", var());
                fim (erro);
            });
        });
    }
    static File arquivoMixagem() { return File::getSpecialLocation (File::tempDirectory).getChildFile ("Stumix-mixagem.wav"); }
    static File arquivoFaixa()   { return File::getSpecialLocation (File::tempDirectory).getChildFile ("Stumix-faixa.wav"); }
    MemoryBlock recebido;        // WAV editado chegando em pedaços
    String roteiro;
    std::unique_ptr<PlacaDeTeste> placaTeste;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EstudioView)
};

//==============================================================================
class JanelaPrincipal : public DocumentWindow
{
public:
    JanelaPrincipal (AudioEngine& e, const String& roteiro)
        : DocumentWindow ("Stumix", Colour (0xff0e0f0c), DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new EstudioView (e, roteiro), false);
        setResizable (true, true);

        // a janela cabe na tela de quem usa (notebook com escala de 125–150% tem pouco espaço)
        Rectangle<int> area (0, 0, 1400, 900);
        if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
            area = d->userArea;
        setResizeLimits (jmin (800, area.getWidth()), jmin (560, area.getHeight()), 10000, 10000);

        const auto estado = arquivoEstado().loadFileAsString();
        if (roteiro.isNotEmpty() || estado.isEmpty() || ! restoreWindowStateFromString (estado))
        {
            const int w = jmin (1400, area.getWidth() - 40), h = jmin (900, area.getHeight() - 40);
            centreWithSize (w, h);
            if (area.getWidth() < 1440 || area.getHeight() < 860)
                setFullScreen (true);   // tela pequena: abre maximizada
        }
        setVisible (true);
        if (auto* p = getPeer()) barraEscura (p->getNativeHandle());
    }

    ~JanelaPrincipal() override { guardarEstado(); }

    void closeButtonPressed() override
    {
        guardarEstado();
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    // tamanho, posição e se está maximizada, para abrir do mesmo jeito da próxima vez
    static File arquivoEstado()
    {
        return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Estudio").getChildFile ("janela.txt");
    }
    void guardarEstado()
    {
        arquivoEstado().getParentDirectory().createDirectory();
        arquivoEstado().replaceWithText (getWindowStateAsString());
    }
};

class EstudioApp : public JUCEApplication
{
public:
    const String getApplicationName() override    { return "Stumix"; }
    const String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override    { return false; }

    void initialise (const String&) override
    {
        Abertura::inicio = Time::getMillisecondCounterHiRes();
        engine = std::make_unique<AudioEngine>();
        // --autoteste=roteiro.js: roda a interface com uma placa simulada e um roteiro de cliques
        String roteiro;
        for (auto& arg : getCommandLineParameterArray())
            if (arg.startsWith ("--esperar="))   // aberto pela atualização: espera a versão antiga fechar
                esperarProcesso ((unsigned long) arg.fromFirstOccurrenceOf ("=", false, false).getLargeIntValue(), 15000);
        if (! empacotadoNaLoja()) File::getSpecialLocation (File::currentExecutableFile).getSiblingFile ("Stumix.antigo.exe").deleteFile();
        for (auto& arg : getCommandLineParameterArray())
            if (arg.startsWith ("--testar-atualizacao="))
            {
                // só para o teste do Windows: baixa e roda o instalador de verdade, anota o resultado e sai
                auto res = Atualizador::instalar (arg.fromFirstOccurrenceOf ("=", false, false).unquoted());
                File::getCurrentWorkingDirectory().getChildFile ("atualizacao.log").replaceWithText (JSON::toString (res, true));
                quit();
                return;
            }
        for (auto& arg : getCommandLineParameterArray())
            if (arg.startsWith ("--autoteste="))
                roteiro = File (arg.fromFirstOccurrenceOf ("=", false, false).unquoted()).loadFileAsString();
        // a janela aparece primeiro; a placa abre logo em seguida, uma vez só, já com a configuração salva
        Abertura::modoTeste = roteiro.isNotEmpty();
        janela = std::make_unique<JanelaPrincipal> (*engine, roteiro);
        Abertura::marcar ("janela");
        if (roteiro.isEmpty())
            MessageManager::callAsync ([this]
            {
                if (engine == nullptr) return;
                engine->iniciar (Abertura::pasta().getChildFile ("placa.xml"));
                Abertura::marcar ("placa");
            });
    }

    void shutdown() override
    {
        janela = nullptr;
        engine = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<JanelaPrincipal> janela;
};

START_JUCE_APPLICATION (EstudioApp)
