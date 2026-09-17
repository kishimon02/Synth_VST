#pragma once

#include "../Controls.h"
#include "../../PluginProcessor.h"
#include "../../ai/RequestPresets.h"
#include "../../ai/MidiExport.h"

namespace ui
{

// Chat with the assistant: capture bar on top, conversation in the middle
// (replies carry cards for notes / parameter diffs / arp patterns), request
// input with presets and quick buttons at the bottom.
class AiPage final : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit AiPage (WaveForgeProcessor&);
    ~AiPage() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Puts a request into the input box without sending it (used by the chord page).
    void setRequestText (const juce::String& text);

private:
    //==========================================================================
    class InputEditor final : public juce::TextEditor
    {
    public:
        std::function<void()> onSend;
        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::returnKey && ! key.getModifiers().isShiftDown() && ! key.getModifiers().isCommandDown())
            {
                if (onSend) onSend();
                return true;
            }
            return juce::TextEditor::keyPressed (key);
        }
    };

    // Piano-roll style preview of suggested notes + audition / drag / save.
    class NotesCard final : public juce::Component
    {
    public:
        NotesCard (AiPage&, const ai::ChatMessage&);
        void paint (juce::Graphics&) override;
        void resized() override;
        static int preferredHeight() { return 190; }
    private:
        class DragHandle final : public juce::TextButton
        {
        public:
            DragHandle (NotesCard& c) : juce::TextButton ("Drag to DAW"), card (c) {}
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override { dragging = false; }
        private:
            NotesCard& card;
            bool dragging = false;
        };
        juce::File ensureFile();
        AiPage& page;
        const ai::ChatMessage message;
        juce::TextButton auditionButton { "Audition" }, stopButton { "Stop" }, saveButton { "Save .mid..." };
        DragHandle dragHandle;
        juce::File exported;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    // Parameter diff (+ wavetable) with Apply / Undo.
    class ParamsCard final : public juce::Component
    {
    public:
        ParamsCard (AiPage&, const ai::ChatMessage&);
        void paint (juce::Graphics&) override;
        void resized() override;
        int preferredHeight() const;
    private:
        void refreshButtons();
        AiPage& page;
        const ai::ChatMessage message;
        std::vector<ai::ParamCatalog::Change> changes;
        juce::StringArray unknown;
        juce::TextButton applyButton { "Apply" }, undoButton { "Undo" }, wavetableButton { "Apply wavetable to OSC A" };
        bool applied = false;
    };

    class ArpCard final : public juce::Component
    {
    public:
        ArpCard (AiPage&, const ai::ChatMessage&);
        void paint (juce::Graphics&) override;
        void resized() override;
        static int preferredHeight() { return 96; }
    private:
        AiPage& page;
        const ai::ChatMessage message;
        juce::TextButton applyButton { "Apply pattern" }, saveButton { "Save as preset..." };
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class MessageView final : public juce::Component
    {
    public:
        MessageView (AiPage&, const ai::ChatMessage&);
        int heightFor (int width);
        void paint (juce::Graphics&) override;
        void resized() override;
    private:
        AiPage& page;
        ai::ChatMessage message;
        std::unique_ptr<NotesCard> notesCard;
        std::unique_ptr<ParamsCard> paramsCard;
        std::unique_ptr<ArpCard> arpCard;
        int textHeight = 20;
    };

    // "Waiting for the reply" bubble shown at the end of the chat while a request is in flight.
    class PendingView final : public juce::Component
    {
    public:
        void begin() { startTime = juce::Time::getMillisecondCounter(); }
        void paint (juce::Graphics&) override;
        static int preferredHeight() { return 44; }
    private:
        juce::uint32 startTime = 0;
    };

    class ChatList final : public juce::Component
    {
    public:
        explicit ChatList (AiPage& p) : page (p) { addChildComponent (pending); }
        void rebuild (const std::vector<ai::ChatMessage>&, juce::uint32 epoch);
        void layoutFor (int width);
        void setPending (bool busy);
        void tick() { if (pending.isVisible()) pending.repaint(); }
    private:
        AiPage& page;
        std::vector<std::unique_ptr<MessageView>> views;
        PendingView pending;
        size_t built = 0;
        juce::uint32 builtEpoch = 0;
    };

    //==========================================================================
    void timerCallback() override;
    void refreshAll();
    void sendInput();
    void quick (const juce::String& category);
    void insertRequest (const juce::String& text);
    void showPresetsMenu();
    void showHistoryMenu();
    void saveInputAsPreset();
    void addMidiFile (const juce::File&);
    void refreshContextLabel();

    WaveForgeProcessor& processor;
    ai::Assistant& assistant;

    juce::Label title { {}, "AI ASSISTANT" }, contextLabel, usageLabel, hintLabel, sessionLabel;
    juce::TextButton recButton { "Rec" }, clearCaptureButton { "Clear" }, addMidiButton { "Add .mid..." },
                     clearTracksButton { "Clear tracks" }, settingsButton { "Settings" }, newChatButton { "New chat" },
                     historyButton { "History" };
    juce::Viewport chatViewport;
    ChatList chatList;
    InputEditor input;
    juce::TextButton presetsButton { "Presets" }, savePresetButton { "Save" }, sendButton { "Send" }, cancelButton { "Cancel" };
    juce::OwnedArray<juce::TextButton> quickButtons;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragOver = false;
    size_t lastHistorySize = 0;
    juce::uint32 lastEpoch = 0;
    bool lastBusy = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiPage)
};

} // namespace ui
