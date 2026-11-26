#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Efecto interno generico: todos los knobs van de 0..1 (facil de automatizar y de tocar desde el asistente).
class BuiltinEffect : public juce::AudioProcessor
{
public:
    enum Type { Gain = 0, Eq, DeEsser, Comp, Sat, Reverb, Delay, NumTypes };

    explicit BuiltinEffect (Type t);

    static juce::String typeName (Type);
    static juce::String sectionFor (Type);

    Type getType() const { return type; }
    int  getNumKnobs() const { return numKnobs; }
    float getKnob (int i) const { return i < numKnobs ? knob[i]->get() : 0.0f; }
    void  setKnob (int i, float v) { if (i < numKnobs) knob[i]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v)); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return typeName (type); }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return type == Reverb || type == Delay ? 3.0 : 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using Duplicator = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

    Type type;
    juce::AudioParameterFloat* knob[4] { nullptr, nullptr, nullptr, nullptr };
    int numKnobs = 0;
    double sr = 44100.0;

    // EQ
    Duplicator hpf, mud, pres, air;
    float lastEq[4] { -1, -1, -1, -1 };
    // De-esser
    juce::dsp::LinkwitzRileyFilter<float> split;
    float env[2] { 0, 0 };
    // Comp / reverb / delay
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 192000 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BuiltinEffect)
};
