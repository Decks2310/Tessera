#include "PhysicalResonatorSlotEditor.h"

PhysicalResonatorSlotEditor::PhysicalResonatorSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
    : SlotEditorBase(apvts, prefix),
    // RESTORE: Physical Resonator parameter prefix must include PHYSRES_
    physResPrefix(prefix + "PHYSRES_")
{
    // --- Setup Components ---
    addAndMakeVisible(orbController);
    addAndMakeVisible(xyPad);
    addAndMakeVisible(modelSelector);
    addAndMakeVisible(noiseTypeSelector);

    // Labels
    addAndMakeVisible(modelLabel);
    modelLabel.setText("Model", juce::dontSendNotification);
    modelLabel.attachToComponent(&modelSelector, false);
    modelLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(noiseTypeLabel);
    noiseTypeLabel.setText("Noise Type", juce::dontSendNotification);
    noiseTypeLabel.attachToComponent(&noiseTypeSelector, false);
    noiseTypeLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(excitationLabel);
    excitationLabel.setText("Excitation (X: Excite Type / Y: Sensitivity)", juce::dontSendNotification);
    excitationLabel.setJustificationType(juce::Justification::centred);

    // Populate ComboBoxes from APVTS (only if parameters exist)
    if (auto* param = apvts.getParameter(physResPrefix + "MODEL"))
        modelSelector.addItemList(param->getAllValueStrings(), 1);

    if (auto* param = apvts.getParameter(physResPrefix + "NOISE_TYPE"))
        noiseTypeSelector.addItemList(param->getAllValueStrings(), 1);

    // --- Setup Attachments (Correct parameter IDs) ---
    tuneAttachment = std::make_unique<SliderAttachment>(apvts, physResPrefix + "TUNE", orbController.tuneSlider);
    mixAttachment = std::make_unique<SliderAttachment>(apvts, physResPrefix + "MIX", orbController.mixSlider);
    modelAttachment = std::make_unique<ComboBoxAttachment>(apvts, physResPrefix + "MODEL", modelSelector);

    exciteTypeAttachment = std::make_unique<SliderAttachment>(apvts, physResPrefix + "EXCITE_TYPE", xyPad.xSlider);
    sensitivityAttachment = std::make_unique<SliderAttachment>(apvts, physResPrefix + "SENSITIVITY", xyPad.ySlider);
    noiseTypeAttachment = std::make_unique<ComboBoxAttachment>(apvts, physResPrefix + "NOISE_TYPE", noiseTypeSelector);

    // Ensure initial internal normalised values reflect current slider state
    orbController.sliderValueChanged(&orbController.mixSlider);
    xyPad.sliderValueChanged(&xyPad.xSlider);
    xyPad.sliderValueChanged(&xyPad.ySlider);

    setSize(300, 450);
}

void PhysicalResonatorSlotEditor::paint(juce::Graphics& g)
{
    // Background handled externally.
}

void PhysicalResonatorSlotEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(10, 15); // Add some padding

    // 1. Resonator Core Area
    auto resonatorArea = bounds.removeFromTop(bounds.getHeight() * 0.55f);

    // Model Selector (placed above the Orb)
    modelSelector.setBounds(resonatorArea.removeFromTop(50).reduced(60, 10));

    // The Orb
    orbController.setBounds(resonatorArea);

    // 2. Excitation Engine Area
    bounds.removeFromTop(10); // Spacer
    excitationLabel.setBounds(bounds.removeFromTop(20));

    auto excitationArea = bounds;

    // XY Pad
    xyPad.setBounds(excitationArea.removeFromTop(100).reduced(40, 0));

    // Noise Type Selector (placed below the XY Pad)
    noiseTypeSelector.setBounds(excitationArea.removeFromTop(50).reduced(60, 10));
}