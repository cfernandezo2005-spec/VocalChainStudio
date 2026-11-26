#pragma once
#include "ChainProcessor.h"

class ChainEditor : public juce::AudioProcessorEditor,
                    private juce::ListBoxModel,
                    private juce::ChangeListener,
                    private juce::Timer
{
public:
    explicit ChainEditor (ChainProcessor&);
    ~ChainEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // usados por las filas
    ChainProcessor& host;
    void openSlotWindow (int row);
    void removeRow (int row);
    void moveRow (int row, int delta);
    void closeAllWindows();
    void log (const juce::String&);

private:
    class PluginWindow;
    class SlotRow;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool, juce::Component* existing) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void showAddMenu();
    void showSectionMenu (const juce::PluginDescription&);
    void showScanDialog();
    void sendPrompt();

    juce::ListBox list { "chain", this };
    juce::ComboBox styleBox;
    juce::TextButton btnListen { "Escuchar mi voz (8 s)" }, btnAdd { "+ Anadir" }, btnScan { "Plugins..." }, btnSend { "Enviar" };
    juce::Label status;
    juce::TextEditor logBox, input;
    juce::OwnedArray<PluginWindow> windows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainEditor)
};
