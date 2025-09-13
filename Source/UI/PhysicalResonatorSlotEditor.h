#pragma once
#include "SlotEditors.h"
#include "UIConfig.h"
#include "OrbController.h"
#include "XYPad.h"

class PhysicalResonatorSlotEditor : public SlotEditorBase
{
public:
    PhysicalResonatorSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    ~PhysicalResonatorSlotEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String physResPrefix;

    // New UI Elements
    OrbController orbController;
    XYPad xyPad;

    // Standard Selectors
    juce::ComboBox modelSelector, noiseTypeSelector;
    juce::Label modelLabel, noiseTypeLabel, excitationLabel;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> tuneAttachment, mixAttachment, exciteTypeAttachment, sensitivityAttachment;
    std::unique_ptr<ComboBoxAttachment> modelAttachment, noiseTypeAttachment;
};