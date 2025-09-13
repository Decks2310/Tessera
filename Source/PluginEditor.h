// File: PluginEditor.h
//================================================================================
// File: PluginEditor.h
//================================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/ModuleSlot.h"
// NEW: Include ParameterUIs for the knobs
#include "UI/ParameterUIs.h" 

class ModularMultiFxAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::ChangeListener
{
public:
    explicit ModularMultiFxAudioProcessorEditor(ModularMultiFxAudioProcessor&);
    ~ModularMultiFxAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // UPDATED: Listener callback (Handles resize and OS lock changes)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
private:
    // ✅ *** ADDED: Helper to get module info (e.g., how many slots it occupies) ***
    struct ModuleInfo { int choice; int slotsUsed = 1; };
    static ModuleInfo getModuleInfo(int choice);

    // ✅ *** ADDED: Helper to manage UI updates and window resizing ***
    void updateSlotsAndResize();

    // NEW: Helper function to update the state of OS controls
    void updateOversamplingControlsState();

    ModularMultiFxAudioProcessor& processorRef;
    CustomLookAndFeel customLookAndFeel;

    // ✅ *** UPDATED: Switched from std::array to std::vector for UI slots ***
    std::vector<std::unique_ptr<ModuleSlot>> moduleSlots;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // ✅ *** ADDED: The "+" button to add more rows ***
    juce::TextButton addRowButton{ "+" };
    // Header Controls
    // UPDATED: Replaced single box with two boxes
    // juce::ComboBox oversamplingBox; // REMOVED
    juce::ComboBox oversamplingAlgoBox;
    juce::ComboBox oversamplingRateBox;

    // NEW: Label for OS Lock Warning
    juce::Label osLockWarningLabel;

    juce::ToggleButton autoGainButton;
    // UPDATED: Input/Output Gain Faders and Response Knob
    // These components manage their own attachments internally (see ParameterUIs.h).
    VerticalFaderWithAttachment inputGainFader;  // Changed from RotaryKnobWithLabels
    VerticalFaderWithAttachment outputGainFader; // Changed from RotaryKnobWithLabels
    RotaryKnobWithLabels responseTimeKnob;

    // NEW: Added label for the mix slider
    juce::Label masterMixLabel;
    juce::Slider masterMixSlider;

    // Attachments (Only needed for standard JUCE components)
    // UPDATED: Attachments for the new boxes
    // std::unique_ptr<...> oversamplingAttachment; // REMOVED
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAlgoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingRateAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterMixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModularMultiFxAudioProcessorEditor)
};