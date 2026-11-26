#include "BuiltinEffect.h"

BuiltinEffect::BuiltinEffect (Type t)
    : AudioProcessor (BusesProperties()
          .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      type (t)
{
    struct K { const char* name; float def; };
    std::vector<K> ks;
    switch (t)
    {
        case Gain:    ks = { { "Gain", 0.5f } }; break;
        case Eq:      ks = { { "Low Cut", 0.5f }, { "Mud Cut", 0.25f }, { "Presence", 0.25f }, { "Air", 0.25f } }; break;
        case DeEsser: ks = { { "Amount", 0.5f } }; break;
        case Comp:    ks = { { "Amount", 0.5f }, { "Makeup", 0.5f } }; break;
        case Sat:     ks = { { "Drive", 0.2f }, { "Mix", 0.5f } }; break;
        case Reverb:  ks = { { "Mix", 0.2f }, { "Size", 0.4f } }; break;
        case Delay:   ks = { { "Mix", 0.1f }, { "Time", 0.3f }, { "Feedback", 0.3f } }; break;
        default: break;
    }
    numKnobs = (int) ks.size();
    for (int i = 0; i < numKnobs; ++i)
    {
        knob[i] = new juce::AudioParameterFloat (juce::ParameterID { "k" + juce::String (i), 1 }, ks[(size_t) i].name,
                                                 juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), ks[(size_t) i].def);
        addParameter (knob[i]);
    }
}

juce::String BuiltinEffect::typeName (Type t)
{
    switch (t)
    {
        case Gain:    return "Input Gain";
        case Eq:      return "Vocal EQ";
        case DeEsser: return "De-Esser";
        case Comp:    return "Compressor";
        case Sat:     return "Saturation";
        case Reverb:  return "Reverb";
        case Delay:   return "Delay";
        default:      return "?";
    }
}

juce::String BuiltinEffect::sectionFor (Type t)
{
    switch (t)
    {
        case Gain:   return "PRE MIX";
        case Reverb: return "REVERB";
        case Delay:  return "DELAY";
        default:     return "VOZ PRINCIPAL";
    }
}

bool BuiltinEffect::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void BuiltinEffect::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    for (auto* f : { &hpf, &mud, &pres, &air }) { f->prepare (spec); f->reset(); }
    for (auto& l : lastEq) l = -1.0f;

    split.prepare (spec);
    split.setCutoffFrequency (6000.0f);
    env[0] = env[1] = 0.0f;

    compressor.prepare (spec);
    compressor.setAttack (8.0f);
    compressor.setRelease (120.0f);

    reverb.prepare (spec);
    reverb.reset();

    delay.setMaximumDelayInSamples ((int) (sampleRate * 1.0) + 8);
    delay.prepare (spec);
    delay.reset();
}

void BuiltinEffect::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n  = buffer.getNumSamples();
    const int nc = juce::jmin (2, buffer.getNumChannels());
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    auto k = [this] (int i) { return knob[i] != nullptr ? knob[i]->get() : 0.0f; };

    switch (type)
    {
        case Gain:
            buffer.applyGain (juce::Decibels::decibelsToGain ((k (0) - 0.5f) * 24.0f));
            break;

        case Eq:
        {
            bool changed = false;
            for (int i = 0; i < 4; ++i)
                if (std::abs (k (i) - lastEq[i]) > 1.0e-4f) changed = true;

            if (changed)
            {
                for (int i = 0; i < 4; ++i) lastEq[i] = k (i);
                *hpf.state  = *Coeffs::makeHighPass  (sr, 20.0f * std::pow (15.0f, k (0)));
                *mud.state  = *Coeffs::makePeakFilter (sr, 300.0f, 1.0f, juce::Decibels::decibelsToGain (-k (1) * 8.0f));
                *pres.state = *Coeffs::makePeakFilter (sr, 3500.0f, 0.8f, juce::Decibels::decibelsToGain (k (2) * 8.0f));
                *air.state  = *Coeffs::makeHighShelf  (sr, 11000.0f, 0.7f, juce::Decibels::decibelsToGain (k (3) * 8.0f));
            }
            hpf.process (ctx); mud.process (ctx); pres.process (ctx); air.process (ctx);
            break;
        }

        case DeEsser:
        {
            const float a = k (0);
            if (a < 0.001f) break;
            const float thrDb = -20.0f - 20.0f * a;
            const float atk = std::exp (-1.0f / (0.0005f * (float) sr));
            const float rel = std::exp (-1.0f / (0.05f * (float) sr));
            for (int c = 0; c < nc; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    float low = 0.0f, high = 0.0f;
                    split.processSample (c, d[i], low, high);
                    const float amp = std::abs (high);
                    env[c] = amp > env[c] ? atk * env[c] + (1.0f - atk) * amp
                                          : rel * env[c] + (1.0f - rel) * amp;
                    const float envDb = juce::Decibels::gainToDecibels (env[c], -100.0f);
                    const float grDb = envDb > thrDb ? -(envDb - thrDb) * 0.7f : 0.0f;
                    d[i] = low + high * juce::Decibels::decibelsToGain (grDb);
                }
            }
            break;
        }

        case Comp:
        {
            const float a = k (0);
            compressor.setThreshold (-10.0f - 20.0f * a);
            compressor.setRatio (1.5f + 4.5f * a);
            compressor.process (ctx);
            buffer.applyGain (juce::Decibels::decibelsToGain (k (1) * 6.0f));
            break;
        }

        case Sat:
        {
            const float drive = 1.0f + 8.0f * k (0);
            const float mix = k (1);
            const float norm = std::tanh (drive);
            for (int c = 0; c < nc; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = d[i] * (1.0f - mix) + (std::tanh (d[i] * drive) / norm) * mix;
            }
            break;
        }

        case Reverb:
        {
            juce::dsp::Reverb::Parameters rp;
            const float wet = k (0) * 0.5f;
            rp.roomSize = juce::jlimit (0.05f, 0.95f, k (1));
            rp.damping = 0.6f; rp.width = 1.0f;
            rp.wetLevel = wet; rp.dryLevel = 1.0f - wet * 0.5f;
            reverb.setParameters (rp);
            reverb.process (ctx);
            break;
        }

        case Delay:
        {
            const float wet = k (0) * 0.6f;
            if (wet < 0.001f) break;
            const float dSamp = (0.05f + 0.55f * k (1)) * (float) sr;
            const float fb = k (2) * 0.7f;
            for (int c = 0; c < nc; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    const float in = d[i];
                    const float del = delay.popSample (c, dSamp, true);
                    delay.pushSample (c, in + del * fb);
                    d[i] = in + del * wet;
                }
            }
            break;
        }

        default: break;
    }
}

juce::AudioProcessorEditor* BuiltinEffect::createEditor() { return new juce::GenericAudioProcessorEditor (*this); }

void BuiltinEffect::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("FX");
    for (int i = 0; i < numKnobs; ++i) xml.setAttribute ("k" + juce::String (i), (double) knob[i]->get());
    copyXmlToBinary (xml, dest);
}

void BuiltinEffect::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        for (int i = 0; i < numKnobs; ++i)
            if (xml->hasAttribute ("k" + juce::String (i)))
                setKnob (i, (float) xml->getDoubleAttribute ("k" + juce::String (i)));
}
