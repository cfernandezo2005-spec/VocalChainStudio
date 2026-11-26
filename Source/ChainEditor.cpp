#include "ChainEditor.h"
#include <map>

namespace { const juce::Colour accent (0xffffa24c), panel (0xff26211c), bg (0xff14110f); }


// Mascota: corazoncito con tirita y ojitos
static void drawMascot (juce::Graphics& g, juce::Rectangle<float> b)
{
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();

    juce::Path heart;
    heart.startNewSubPath (x + w * 0.5f, y + h * 0.95f);
    heart.cubicTo (x - w * 0.18f, y + h * 0.55f, x + w * 0.02f, y - h * 0.08f, x + w * 0.5f, y + h * 0.27f);
    heart.cubicTo (x + w * 0.98f, y - h * 0.08f, x + w * 1.18f, y + h * 0.55f, x + w * 0.5f, y + h * 0.95f);
    heart.closeSubPath();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff8fa8), x, y, juce::Colour (0xffff5d82), x, y + h, false));
    g.fillPath (heart);
    g.setColour (juce::Colour (0xffd93a63));
    g.strokePath (heart, juce::PathStrokeType (2.0f));

    // brillo
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillEllipse (x + w * 0.17f, y + h * 0.18f, w * 0.14f, h * 0.10f);

    // tirita
    {
        juce::Path band;
        const float bw = w * 0.62f, bh = h * 0.22f;
        band.addRoundedRectangle (-bw * 0.5f, -bh * 0.5f, bw, bh, bh * 0.35f);
        auto t = juce::AffineTransform::rotation (-0.65f).translated (x + w * 0.72f, y + h * 0.30f);
        g.setColour (juce::Colour (0xffeccfa0));
        g.fillPath (band, t);
        g.setColour (juce::Colour (0xffc9a574));
        g.strokePath (band, juce::PathStrokeType (1.2f), t);

        juce::Path pad;
        pad.addRoundedRectangle (-bw * 0.17f, -bh * 0.5f, bw * 0.34f, bh, 2.0f);
        g.setColour (juce::Colour (0xfff6e3c2));
        g.fillPath (pad, t);
        g.setColour (juce::Colour (0xffc9a574).withAlpha (0.8f));
        for (int ix = -1; ix <= 1; ++ix)
            for (int iy = -1; iy <= 1; iy += 2)
            {
                juce::Path dot;
                dot.addEllipse (ix * bw * 0.09f - 1.0f, iy * bh * 0.17f - 1.0f, 2.0f, 2.0f);
                g.fillPath (dot, t);
            }
    }

    // ojitos
    const float ey = y + h * 0.52f, er = w * 0.075f;
    for (float ex : { x + w * 0.36f, x + w * 0.64f })
    {
        g.setColour (juce::Colour (0xff2a1418));
        g.fillEllipse (ex - er, ey - er * 1.2f, er * 2.0f, er * 2.4f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (ex - er * 0.2f, ey - er * 0.8f, er * 0.8f, er * 0.8f);
    }
    // mejillas y sonrisa
    g.setColour (juce::Colour (0xffff2f62).withAlpha (0.35f));
    g.fillEllipse (x + w * 0.20f, y + h * 0.58f, w * 0.12f, h * 0.08f);
    g.fillEllipse (x + w * 0.68f, y + h * 0.58f, w * 0.12f, h * 0.08f);
    juce::Path smile;
    smile.addCentredArc (x + w * 0.5f, y + h * 0.60f, w * 0.07f, h * 0.05f, 0.0f, 2.3f, 3.98f, true);
    g.setColour (juce::Colour (0xff2a1418));
    g.strokePath (smile, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---------------------------------------------------------------- ventana de plugin
class ChainEditor::PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioProcessor& p, const juce::String& title, std::function<void (PluginWindow*)> onClose)
        : DocumentWindow (title, juce::Colours::black, DocumentWindow::closeButton), closeCb (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        if (auto* ed = p.createEditorIfNeeded()) setContentOwned (ed, true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        toFront (true);
    }
    void closeButtonPressed() override { closeCb (this); }

private:
    std::function<void (PluginWindow*)> closeCb;
};

// ---------------------------------------------------------------- fila de slot
class ChainEditor::SlotRow : public juce::Component
{
public:
    explicit SlotRow (ChainEditor& o) : owner (o)
    {
        up.setButtonText ("^");  down.setButtonText ("v");  open.setButtonText ("Abrir");  del.setButtonText ("X");
        on.setButtonText ("On");
        for (auto* c : std::initializer_list<juce::Component*> { &up, &down, &open, &del, &on }) addAndMakeVisible (c);

        on.onClick   = [this] { owner.host.setBypass (row, ! on.getToggleState()); repaint(); };
        open.onClick = [this] { owner.openSlotWindow (row); };
        up.onClick   = [this] { auto safe = juce::Component::SafePointer<ChainEditor> (&owner); int r = row;
                                juce::MessageManager::callAsync ([safe, r] { if (safe != nullptr) safe->moveRow (r, -1); }); };
        down.onClick = [this] { auto safe = juce::Component::SafePointer<ChainEditor> (&owner); int r = row;
                                juce::MessageManager::callAsync ([safe, r] { if (safe != nullptr) safe->moveRow (r, 1); }); };
        del.onClick  = [this] { auto safe = juce::Component::SafePointer<ChainEditor> (&owner); int r = row;
                                juce::MessageManager::callAsync ([safe, r] { if (safe != nullptr) safe->removeRow (r); }); };
    }

    void setRow (int r)
    {
        row = r;
        if (auto* s = owner.host.getSlot (r)) on.setToggleState (! s->bypass, juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto* s = owner.host.getSlot (row);
        if (s == nullptr) return;
        auto* prev = owner.host.getSlot (row - 1);
        const bool first = (prev == nullptr) || prev->section != s->section;

        auto r = getLocalBounds();
        auto head = r.removeFromTop (16);
        if (first)
        {
            g.setColour (accent);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (s->section, head.reduced (8, 0), juce::Justification::centredLeft);
        }
        g.setColour (panel);
        g.fillRoundedRectangle (r.reduced (4, 2).toFloat(), 6.0f);

        g.setColour (s->bypass ? juce::Colours::grey : juce::Colours::white);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (juce::String (row + 1).paddedLeft ('0', 2) + "   " + s->name + (s->builtin ? "" : "   (externo)"),
                    r.reduced (14, 0).withTrimmedRight (230), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop (16).reduced (10, 6);
        del.setBounds (r.removeFromRight (30));   r.removeFromRight (4);
        down.setBounds (r.removeFromRight (30));  r.removeFromRight (4);
        up.setBounds (r.removeFromRight (30));    r.removeFromRight (8);
        open.setBounds (r.removeFromRight (60));  r.removeFromRight (8);
        on.setBounds (r.removeFromRight (56));
    }

private:
    ChainEditor& owner;
    int row = 0;
    juce::TextButton up, down, open, del;
    juce::ToggleButton on;
};

// ---------------------------------------------------------------- editor
ChainEditor::ChainEditor (ChainProcessor& p) : AudioProcessorEditor (&p), host (p)
{
    setSize (780, 700);

    addAndMakeVisible (list);
    list.setRowHeight (52);
    list.setColour (juce::ListBox::backgroundColourId, bg);

    styleBox.addItemList (ChainProcessor::getStyleNames(), 1);
    styleBox.setTextWhenNothingSelected ("Estilo...");
    styleBox.onChange = [this]
    {
        host.loadStyle (styleBox.getText());
        log (T ("Cargué el estilo: ") + styleBox.getText());
    };
    addAndMakeVisible (styleBox);

    btnListen.onClick = [this]
    {
        if (host.isListening()) { host.cancelListening(); status.setText ("", juce::dontSendNotification); btnListen.setButtonText ("Escuchar mi voz (8 s)"); return; }
        if (host.startListening())
        {
            btnListen.setButtonText ("Cancelar");
            status.setText (T ("Escuchando... reproduce o canta ahora (el DAW debe estar sonando)"), juce::dontSendNotification);
        }
        else log (T ("Aún no hay audio corriendo. Dale a play en el DAW e inténtalo de nuevo."));
    };
    btnAdd.setButtonText (T ("+ Añadir"));
    btnAdd.onClick  = [this] { showAddMenu(); };
    btnScan.onClick = [this] { showScanDialog(); };
    btnSend.onClick = [this] { sendPrompt(); };
    for (auto* b : { &btnListen, &btnAdd, &btnScan, &btnSend }) addAndMakeVisible (*b);

    status.setColour (juce::Label::textColourId, accent);
    addAndMakeVisible (status);

    logBox.setMultiLine (true, true);
    logBox.setReadOnly (true);
    logBox.setColour (juce::TextEditor::backgroundColourId, panel);
    logBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (logBox);

    input.setTextToShowWhenEmpty (T ("Pídele algo a la cadena: \"más brillante\", \"menos sibilancia\", \"más reverb\"..."), juce::Colours::grey);
    input.onReturnKey = [this] { sendPrompt(); };
    addAndMakeVisible (input);

    host.chainChanged.addChangeListener (this);
    host.onBeforeStructureChange = [this] { closeAllWindows(); };
    startTimerHz (10);

    log (T ("Hola, soy tu asistente de cadena de voz. Usa \"Escuchar mi voz\" para que arme la cadena, "
            "o escríbeme qué quieres (por ejemplo: \"más brillante y con menos eses\")."));
}

ChainEditor::~ChainEditor()
{
    stopTimer();
    host.onBeforeStructureChange = nullptr;
    host.chainChanged.removeChangeListener (this);
    windows.clear();
}

void ChainEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (accent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VOCAL CHAIN STUDIO", 16, 8, 400, 34, juce::Justification::centredLeft);
    drawMascot (g, juce::Rectangle<float> ((float) getWidth() - 78.0f, 6.0f, 62.0f, 56.0f));
}

void ChainEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (50);
    auto top = r.removeFromTop (32);
    styleBox.setBounds (top.removeFromLeft (190));  top.removeFromLeft (8);
    btnListen.setBounds (top.removeFromLeft (170)); top.removeFromLeft (8);
    btnAdd.setBounds (top.removeFromLeft (110));    top.removeFromLeft (8);
    btnScan.setBounds (top.removeFromLeft (110));
    status.setBounds (r.removeFromTop (24));

    auto bottom = r.removeFromBottom (32);
    btnSend.setBounds (bottom.removeFromRight (80));
    bottom.removeFromRight (6);
    input.setBounds (bottom);
    r.removeFromBottom (6);
    logBox.setBounds (r.removeFromBottom (140));
    r.removeFromBottom (6);
    list.setBounds (r);
}

int ChainEditor::getNumRows() { return host.getNumSlots(); }

juce::Component* ChainEditor::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    if (row >= host.getNumSlots()) { delete existing; return nullptr; }
    auto* comp = dynamic_cast<SlotRow*> (existing);
    if (comp == nullptr) { delete existing; comp = new SlotRow (*this); }
    comp->setRow (row);
    return comp;
}

void ChainEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    list.updateContent();
    list.repaint();
}

void ChainEditor::timerCallback()
{
    if (host.isListening())
    {
        const int sec = (int) (host.getListenProgress() * 8.0f);
        status.setText (T ("Escuchando... ") + juce::String (sec) + "/8 s", juce::dontSendNotification);
    }
    else if (host.isCaptureReady())
    {
        status.setText ("", juce::dontSendNotification);
        btnListen.setButtonText ("Escuchar mi voz (8 s)");
        log (host.buildFromCapture());
    }
    else if (btnListen.getButtonText() == "Cancelar")
    {
        btnListen.setButtonText ("Escuchar mi voz (8 s)");
        status.setText ("", juce::dontSendNotification);
    }
}

void ChainEditor::log (const juce::String& s)
{
    if (s.isEmpty()) return;
    logBox.moveCaretToEnd();
    logBox.insertTextAtCaret (s + "\n\n");
}

void ChainEditor::sendPrompt()
{
    const auto t = input.getText().trim();
    if (t.isEmpty()) return;
    log ("> " + t);
    log (host.runAssistant (t));
    input.clear();
}

// ---------------------------------------------------------------- acciones de fila
void ChainEditor::openSlotWindow (int row)
{
    auto* s = host.getSlot (row);
    if (s == nullptr || ! s->proc->hasEditor()) return;

    for (auto* w : windows)
        if (w->getName() == s->name + "#" + juce::String ((juce::pointer_sized_int) s->proc.get()))
        { w->toFront (true); return; }

    juce::Component::SafePointer<ChainEditor> safe (this);
    auto* w = new PluginWindow (*s->proc, s->name, [safe] (PluginWindow* closing)
    {
        juce::MessageManager::callAsync ([safe, closing]
        {
            if (safe != nullptr) safe->windows.removeObject (closing);
        });
    });
    w->setName (s->name + "#" + juce::String ((juce::pointer_sized_int) s->proc.get()));
    windows.add (w);
}

void ChainEditor::closeAllWindows() { windows.clear(); }
void ChainEditor::removeRow (int row) { host.removeSlot (row); }
void ChainEditor::moveRow (int row, int delta) { host.moveSlot (row, delta); }

// ---------------------------------------------------------------- menus
void ChainEditor::showAddMenu()
{
    juce::PopupMenu m, fx, ext;
    for (int t = 0; t < BuiltinEffect::NumTypes; ++t)
        fx.addItem (100 + t, BuiltinEffect::typeName ((BuiltinEffect::Type) t));
    m.addSubMenu (T ("Efectos incluidos"), fx);

    const auto types = host.knownPlugins.getTypes();
    std::map<juce::String, juce::PopupMenu> makers;
    for (int i = 0; i < (int) types.size(); ++i)
    {
        const auto& d = types[i];
        if (d.isInstrument) continue;
        makers[d.manufacturerName.isEmpty() ? T ("Otros") : d.manufacturerName].addItem (1000 + i, d.name);
    }
    for (auto& kv : makers) ext.addSubMenu (kv.first, kv.second);
    if (makers.empty()) ext.addItem (1, T ("(primero escanea tus plugins)"), false, false);
    m.addSubMenu (T ("Tus plugins (VST3)"), ext);

    juce::Component::SafePointer<ChainEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btnAdd), [safe, types] (int result)
    {
        if (safe == nullptr || result == 0) return;
        if (result >= 1000)      safe->showSectionMenu (types[result - 1000]);
        else if (result >= 100)  safe->host.addBuiltin ((BuiltinEffect::Type) (result - 100));
    });
}

void ChainEditor::showSectionMenu (const juce::PluginDescription& desc)
{
    juce::PopupMenu sm;
    sm.addSectionHeader (T ("¿En qué sección?"));
    for (int i = 0; i < ChainProcessor::sectionNames().size(); ++i)
        sm.addItem (i + 1, ChainProcessor::sectionNames()[i]);

    juce::Component::SafePointer<ChainEditor> safe (this);
    sm.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btnAdd), [safe, desc] (int r)
    {
        if (safe == nullptr || r == 0) return;
        const auto err = safe->host.addExternal (desc, ChainProcessor::sectionNames()[r - 1]);
        if (err.isNotEmpty()) safe->log (err);
    });
}

void ChainEditor::showScanDialog()
{
    auto* comp = new juce::PluginListComponent (host.formatManager, host.knownPlugins,
                                                ChainProcessor::getAppDir().getChildFile ("scan_pedal.txt"), nullptr, false);
    comp->setSize (640, 480);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (comp);
    o.dialogTitle = "Plugins instalados";
    o.componentToCentreAround = this;
    o.dialogBackgroundColour = bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}
