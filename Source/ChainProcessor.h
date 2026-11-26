#pragma once
#include "BuiltinEffect.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// Texto UTF-8 (acentos) -> juce::String
inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

struct ChainSlot
{
    std::unique_ptr<juce::AudioProcessor> proc;   // efecto interno o plugin externo
    juce::String name, section;
    std::atomic<bool> bypass { false };
    bool builtin = true;
    int builtinType = 0;
    juce::PluginDescription desc;                 // solo para externos
};

class ChainProcessor : public juce::AudioProcessor, private juce::ChangeListener
{
public:
    ChainProcessor();
    ~ChainProcessor() override;

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vocal Chain Studio"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Cadena (hilo de mensajes / UI) ----
    static const juce::StringArray& sectionNames();
    int getNumSlots() const { return (int) slots.size(); }
    ChainSlot* getSlot (int i) { return juce::isPositiveAndBelow (i, (int) slots.size()) ? slots[(size_t) i].get() : nullptr; }
    ChainSlot* addBuiltin (BuiltinEffect::Type t);
    juce::String addExternal (const juce::PluginDescription& d, const juce::String& section); // devuelve error o ""
    void removeSlot (int index);
    void clearChain();
    void moveSlot (int index, int delta);
    void setBypass (int index, bool shouldBypass);
    BuiltinEffect* findBuiltin (BuiltinEffect::Type t, bool createIfMissing);

    // ---- Presets de estilo y asistente ----
    static juce::StringArray getStyleNames();
    void loadStyle (const juce::String& name);
    juce::String runAssistant (const juce::String& text);

    // ---- Escuchar la voz (8 s) y construir cadena ----
    bool startListening();
    void cancelListening() { listening = false; }
    bool isListening() const { return listening.load(); }
    bool isCaptureReady() const { return captureReady.load(); }
    float getListenProgress() const;
    juce::String buildFromCapture();

    // ---- Hosting ----
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::ChangeBroadcaster chainChanged;
    std::function<void()> onBeforeStructureChange;
    static juce::File getAppDir();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void prepareProcessor (juce::AudioProcessor&);
    ChainSlot* insertSlot (std::unique_ptr<ChainSlot>);
    void updateLatency();
    void captureInput (const juce::AudioBuffer<float>&);

    std::vector<std::unique_ptr<ChainSlot>> slots;
    juce::SpinLock chainLock;

    double sr = 44100.0;
    int blockSize = 512;
    juce::dsp::Limiter<float> limiter;

    juce::AudioBuffer<float> capture;
    std::atomic<int> capturePos { 0 };
    std::atomic<bool> listening { false }, captureReady { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainProcessor)
};
