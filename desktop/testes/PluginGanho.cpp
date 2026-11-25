// Plugin VST3 mínimo, só para os testes automáticos do Stumix: multiplica o som pelo "ganho".
#include <juce_audio_processors/juce_audio_processors.h>
using namespace juce;

class Ganho : public AudioProcessor
{
public:
    Ganho() : AudioProcessor (BusesProperties().withInput ("Entrada", AudioChannelSet::stereo()).withOutput ("Saída", AudioChannelSet::stereo()))
    {
        addParameter (ganho = new AudioParameterFloat (ParameterID { "ganho", 1 }, "Ganho", 0.0f, 1.0f, 0.5f));
    }
    const String getName() const override { return "Stumix Teste Ganho"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (AudioBuffer<float>& b, MidiBuffer&) override { b.applyGain (ganho->get()); }
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainInputChannelSet() == l.getMainOutputChannelSet() && ! l.getMainOutputChannelSet().isDisabled(); }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}
    void getStateInformation (MemoryBlock& m) override { MemoryOutputStream (m, false).writeFloat (ganho->get()); }
    void setStateInformation (const void* d, int n) override { if (n >= 4) *ganho = MemoryInputStream (d, (size_t) n, false).readFloat(); }
    AudioParameterFloat* ganho;
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Ganho(); }
