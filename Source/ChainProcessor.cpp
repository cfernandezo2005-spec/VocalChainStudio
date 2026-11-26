#include "ChainProcessor.h"
#include "ChainEditor.h"

juce::File ChainProcessor::getAppDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("VocalChainStudio");
    d.createDirectory();
    return d;
}

const juce::StringArray& ChainProcessor::sectionNames()
{
    static const juce::StringArray s { "PRE MIX", "VOZ PRINCIPAL", "BUS", "REVERB", "DELAY" };
    return s;
}

ChainProcessor::ChainProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.addDefaultFormats();
    if (auto xml = juce::XmlDocument::parse (getAppDir().getChildFile ("plugins.xml")))
        knownPlugins.recreateFromXml (*xml);
    knownPlugins.addChangeListener (this);

    loadStyle ("Pop limpio");   // cadena inicial
}

ChainProcessor::~ChainProcessor()
{
    knownPlugins.removeChangeListener (this);
}

void ChainProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto xml = knownPlugins.createXml())
        xml->writeTo (getAppDir().getChildFile ("plugins.xml"));
}

bool ChainProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChainProcessor::prepareProcessor (juce::AudioProcessor& p)
{
    p.setRateAndBufferSizeDetails (sr, blockSize);

    auto layout = p.getBusesLayout();
    if (layout.inputBuses.size() > 0)  layout.inputBuses.set (0, juce::AudioChannelSet::stereo());
    if (layout.outputBuses.size() > 0) layout.outputBuses.set (0, juce::AudioChannelSet::stereo());
    for (int i = 1; i < layout.inputBuses.size(); ++i)  layout.inputBuses.set (i, juce::AudioChannelSet::disabled());
    for (int i = 1; i < layout.outputBuses.size(); ++i) layout.outputBuses.set (i, juce::AudioChannelSet::disabled());
    if (p.checkBusesLayoutSupported (layout)) p.setBusesLayout (layout);

    p.prepareToPlay (sr, blockSize);
}

void ChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    blockSize = samplesPerBlock;

    for (auto& s : slots) { s->proc->releaseResources(); prepareProcessor (*s->proc); }

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    limiter.prepare (spec);
    limiter.setThreshold (-1.0f);
    limiter.setRelease (80.0f);

    capture.setSize (1, (int) (8.0 * sampleRate));
    capturePos = 0;
    updateLatency();
}

void ChainProcessor::updateLatency()
{
    int total = 0;
    for (auto& s : slots) if (! s->bypass) total += s->proc->getLatencySamples();
    setLatencySamples (total);
}

void ChainProcessor::captureInput (const juce::AudioBuffer<float>& buffer)
{
    const int cap = capture.getNumSamples();
    int pos = capturePos.load();
    auto* w = capture.getWritePointer (0);
    const int nc = buffer.getNumChannels();
    for (int i = 0; i < buffer.getNumSamples() && pos < cap; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < nc; ++c) s += buffer.getSample (c, i);
        w[pos++] = s / (float) juce::jmax (1, nc);
    }
    capturePos = pos;
    if (pos >= cap) { listening = false; captureReady = true; }
}

void ChainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (listening.load()) captureInput (buffer);

    {
        const juce::SpinLock::ScopedTryLockType lock (chainLock);
        if (lock.isLocked())
        {
            juce::MidiBuffer midi;
            for (auto& s : slots)
                if (! s->bypass) s->proc->processBlock (buffer, midi);
        }
    }

    juce::dsp::AudioBlock<float> block (buffer);
    limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
}

// ---------------------------------------------------------------- cadena
ChainSlot* ChainProcessor::insertSlot (std::unique_ptr<ChainSlot> s)
{
    const int sec = sectionNames().indexOf (s->section);
    int pos = 0;
    for (int i = 0; i < (int) slots.size(); ++i)
        if (sectionNames().indexOf (slots[(size_t) i]->section) <= sec) pos = i + 1;

    ChainSlot* raw = s.get();
    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        slots.insert (slots.begin() + pos, std::move (s));
    }
    updateLatency();
    chainChanged.sendChangeMessage();
    return raw;
}

ChainSlot* ChainProcessor::addBuiltin (BuiltinEffect::Type t)
{
    auto fx = std::make_unique<BuiltinEffect> (t);
    prepareProcessor (*fx);
    auto s = std::make_unique<ChainSlot>();
    s->name = BuiltinEffect::typeName (t);
    s->section = BuiltinEffect::sectionFor (t);
    s->builtin = true;
    s->builtinType = (int) t;
    s->proc = std::move (fx);
    return insertSlot (std::move (s));
}

juce::String ChainProcessor::addExternal (const juce::PluginDescription& d, const juce::String& section)
{
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> inst = formatManager.createPluginInstance (d, sr, blockSize, error);
    if (inst == nullptr)
        return T ("No pude cargar ") + d.name + ": " + error;

    prepareProcessor (*inst);
    auto s = std::make_unique<ChainSlot>();
    s->name = d.name;
    s->section = section;
    s->builtin = false;
    s->desc = d;
    s->proc = std::move (inst);
    insertSlot (std::move (s));
    return {};
}

void ChainProcessor::removeSlot (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) slots.size())) return;
    if (onBeforeStructureChange) onBeforeStructureChange();

    std::unique_ptr<ChainSlot> dead;
    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        dead = std::move (slots[(size_t) index]);
        slots.erase (slots.begin() + index);
    }
    dead.reset();
    updateLatency();
    chainChanged.sendChangeMessage();
}

void ChainProcessor::clearChain()
{
    if (onBeforeStructureChange) onBeforeStructureChange();

    std::vector<std::unique_ptr<ChainSlot>> dead;
    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        dead.swap (slots);
    }
    dead.clear();
    updateLatency();
    chainChanged.sendChangeMessage();
}

void ChainProcessor::moveSlot (int index, int delta)
{
    const int other = index + delta;
    if (! juce::isPositiveAndBelow (index, (int) slots.size()) || ! juce::isPositiveAndBelow (other, (int) slots.size())) return;
    if (slots[(size_t) index]->section != slots[(size_t) other]->section) return;
    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        std::swap (slots[(size_t) index], slots[(size_t) other]);
    }
    chainChanged.sendChangeMessage();
}

void ChainProcessor::setBypass (int index, bool shouldBypass)
{
    if (auto* s = getSlot (index)) { s->bypass = shouldBypass; updateLatency(); }
}

BuiltinEffect* ChainProcessor::findBuiltin (BuiltinEffect::Type t, bool createIfMissing)
{
    for (auto& s : slots)
        if (s->builtin && s->builtinType == (int) t) return static_cast<BuiltinEffect*> (s->proc.get());
    if (createIfMissing)
        if (auto* s = addBuiltin (t)) return static_cast<BuiltinEffect*> (s->proc.get());
    return nullptr;
}

// ---------------------------------------------------------------- estado
void ChainProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("CHAIN");
    for (auto& s : slots)
    {
        auto* e = root.createNewChildElement ("SLOT");
        e->setAttribute ("builtin", s->builtin);
        e->setAttribute ("type", s->builtinType);
        e->setAttribute ("name", s->name);
        e->setAttribute ("section", s->section);
        e->setAttribute ("bypass", s->bypass.load());
        if (! s->builtin)
            if (auto d = s->desc.createXml()) e->addChildElement (d.release());

        juce::MemoryBlock mb;
        s->proc->getStateInformation (mb);
        e->setAttribute ("state", mb.toBase64Encoding());
    }
    copyXmlToBinary (root, dest);
}

void ChainProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("CHAIN")) return;

    clearChain();
    for (auto* e : xml->getChildWithTagNameIterator ("SLOT"))
    {
        ChainSlot* slot = nullptr;
        if (e->getBoolAttribute ("builtin", true))
        {
            slot = addBuiltin ((BuiltinEffect::Type) juce::jlimit (0, (int) BuiltinEffect::NumTypes - 1, e->getIntAttribute ("type")));
        }
        else if (auto* dx = e->getFirstChildElement())
        {
            juce::PluginDescription d;
            if (d.loadFromXml (*dx))
            {
                const int before = getNumSlots();
                addExternal (d, e->getStringAttribute ("section", "VOZ PRINCIPAL"));
                if (getNumSlots() > before)
                    for (auto& s : slots) if (! s->builtin && s->desc.isDuplicateOf (d)) slot = s.get();
            }
        }
        if (slot != nullptr)
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (e->getStringAttribute ("state")))
                slot->proc->setStateInformation (mb.getData(), (int) mb.getSize());
            slot->bypass = e->getBoolAttribute ("bypass", false);
        }
    }
    updateLatency();
}

juce::AudioProcessorEditor* ChainProcessor::createEditor() { return new ChainEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChainProcessor(); }
