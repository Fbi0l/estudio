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
        auto opcoes = URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                          .withExtraHeaders ("User-Agent: Stumix-Desktop\r\nAccept: application/vnd.github+json")
                          .withConnectionTimeoutMs (15000);
        std::unique_ptr<InputStream> in (URL (api).createInputStream (opcoes));
        if (in == nullptr) { r->setProperty ("erro", t ("sem conexão com o GitHub")); return var (r); }
        auto json = JSON::parse (in->readEntireStreamAsString());
        const String tag = json["tag_name"].toString();
        if (! tag.startsWith ("desktop-v")) { r->setProperty ("erro", t ("nenhuma versão publicada ainda")); return var (r); }
        const String versao = tag.fromFirstOccurrenceOf ("desktop-v", false, false);
        String url;
        if (auto* assets = json["assets"].getArray())
            for (auto& a : *assets)
                if (a["name"].toString().endsWith ("Instalador.exe")) url = a["browser_download_url"].toString();
        r->setProperty ("versao", versao);
        r->setProperty ("url", url);
        r->setProperty ("notas", json["body"].toString());
        r->setProperty ("nova", url.isNotEmpty() && comparar (JUCE_APPLICATION_VERSION_STRING, versao) < 0);
        return var (r);
    }

    static var instalar (const String& url)
    {
        auto* r = new DynamicObject();
        if (! url.startsWith ("https://github.com/Fbi0l/estudio/releases/download/"))
        { r->setProperty ("ok", false); r->setProperty ("erro", t ("endereço inesperado")); return var (r); }
        auto destino = File::getSpecialLocation (File::tempDirectory).getChildFile ("Stumix-Atualizacao.exe");
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
            .withNativeFunction ("estudioListar", [this] (const Array<var>&, auto done) { done (engine.listar()); })
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
                    if ((bool) r["ok"])   // o instalador assume daqui: fecha o app, instala e reabre
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
            .withNativeFunction ("estudioAbrirLink", [] (const Array<var>& a, auto done)
            {
                if (! a.isEmpty()) URL (a[0].toString()).launchInDefaultBrowser();
                done (true);
            });
    }

    std::optional<WebBrowserComponent::Resource> recurso (const String& url)
    {
        auto caminho = url.upToFirstOccurrenceOf ("?", false, false).trimCharactersAtStart ("/");
        if (caminho.isEmpty()) caminho = "index.html";

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
        auto medidas = engine.pegarMedidas();
        if (! medidas.isEmpty()) o->setProperty ("medidas", medidas);
        web.emitEventIfBrowserIsVisible ("estudioNivel", var (o));
    }

    int contagem = 0;
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
        engine = std::make_unique<AudioEngine>();
        // --autoteste=roteiro.js: roda a interface com uma placa simulada e um roteiro de cliques
        String roteiro;
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
        if (roteiro.isEmpty()) engine->iniciar();
        janela = std::make_unique<JanelaPrincipal> (*engine, roteiro);
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
