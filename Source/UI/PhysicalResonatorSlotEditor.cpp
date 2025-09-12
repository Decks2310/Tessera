/*
  ==============================================================================

    PhysicalResonatorSlotEditor.cpp
    Created: 11 Sep 2025 12:09:46pm
    Author:  agusr

  ==============================================================================
*/
#include "PhysicalResonatorSlotEditor.h"

PhysicalResonatorSlotEditor::PhysicalResonatorSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
    : SlotEditorBase(apvts, prefix),
    physResPrefix(prefix + "PHYSRES_"),
    // Initialize Main Controls
    modelBox(apvts, physResPrefix + "MODEL", "Model"),
    tuneKnob(apvts, physResPrefix + "TUNE", "Tune"),
    structureKnob(apvts, physResPrefix + "STRUCTURE", "Structure"),
    brightnessKnob(apvts, physResPrefix + "BRIGHTNESS", "Brightness"),
    dampingKnob(apvts, physResPrefix + "DAMPING", "Damping"),

    positionKnob(apvts, physResPrefix + "POSITION", "Position"),
    mixKnob(apvts, physResPrefix + "MIX", "Mix"),
    // Initialize Excitation Engine Controls
    exciteTypeKnob(apvts, physResPrefix + "EXCITE_TYPE", "Excite Type"),
    sensitivityKnob(apvts, physResPrefix + "SENSITIVITY", "Sensitivity"),
    // Initialize Advanced Excitation
    noiseTypeBox(apvts, physResPrefix + "NOISE_TYPE", "Noise Type"),
    attackKnob(apvts, physResPrefix + "ATTACK", "Attack"),
    decayKnob(apvts, physResPrefix + "DECAY", "Decay"),
    sustainKnob(apvts, physResPrefix + "SUSTAIN", "Sustain"),
    releaseKnob(apvts, physResPrefix + "RELEASE", "Release")
{
    // Add all components
    addAndMakeVisible(modelBox);
    addAndMakeVisible(tuneKnob);
    addAndMakeVisible(structureKnob);
    addAndMakeVisible(brightnessKnob);
    addAndMakeVisible(dampingKnob);
    addAndMakeVisible(positionKnob);
    addAndMakeVisible(mixKnob);

    addAndMakeVisible(exciteTypeKnob);
    addAndMakeVisible(sensitivityKnob);
    addAndMakeVisible(noiseTypeBox);
    addAndMakeVisible(attackKnob);
    addAndMakeVisible(decayKnob);
    addAndMakeVisible(sustainKnob);
    addAndMakeVisible(releaseKnob);
    // Setup Group Components for visual separation
    addAndMakeVisible(mainGroup);
    mainGroup.setText("Resonator Core");
    // Set look and feel to use the custom one for consistent colors
    mainGroup.setLookAndFeel(&getLookAndFeel());

    addAndMakeVisible(excitationGroup);
    excitationGroup.setText("Excitation Engine");
    excitationGroup.setLookAndFeel(&getLookAndFeel());
}
void PhysicalResonatorSlotEditor::resized()
{
    // This module has many controls, so we use a structured layout with groups.
    auto bounds = getLocalBounds().reduced(5);

    // Split the layout horizontally if wide enough (e.g., > 450px), or vertically if narrow.
    bool isWide = bounds.getWidth() > 450;

    juce::Rectangle<int> mainArea, excitationArea;

    if (isWide)
    {
        // Wide layout: Resonator on the left, Excitation on the right
        mainArea = bounds.removeFromLeft(bounds.getWidth() * 0.55f).reduced(5);
        excitationArea = bounds.reduced(5);
    }
    else
    {
        // Narrow layout: Resonator on top, Excitation on the bottom
        mainArea = bounds.removeFromTop(bounds.getHeight() * 0.5f).reduced(5);
        excitationArea = bounds.reduced(5);
    }

    // Set bounds for the GroupComponents
    mainGroup.setBounds(mainArea);
    excitationGroup.setBounds(excitationArea);
    // Adjust areas for internal layout (accounting for group borders/labels)
    // Trim top for the label (approx 20px) and reduce sides for padding
    auto internalMain = mainArea.reduced(10).withTrimmedTop(20);
    auto internalExcitation = excitationArea.reduced(10).withTrimmedTop(20);


    // --- Layout Main Area (Resonator Core) ---
    juce::FlexBox mainFb;
    mainFb.flexWrap = juce::FlexBox::Wrap::wrap;
    mainFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    mainFb.alignContent = juce::FlexBox::AlignContent::spaceAround;

    // Define basis for 3 columns in main area
    float mainBasis = (float)internalMain.getWidth() / 3.0f;
    // Helper to define FlexItems, using the existing LayoutHelpers
    // We adapt it slightly to handle the ComboBox width preference.
    auto createFlexItem = [&](juce::Component& component, float basis) {
        // ComboBoxes might need slightly more width than standard knobs
        float minWidth = dynamic_cast<ComboBoxWithLabel*>(&component) ?
            80.0f : LayoutHelpers::minKnobWidth;

        return juce::FlexItem(component)
            .withFlex(1.0f, 1.0f, basis)
            .withMinWidth(minWidth)
            .withMinHeight(LayoutHelpers::minKnobHeight);
        };

    // Row 1
    mainFb.items.add(createFlexItem(modelBox, mainBasis));
    mainFb.items.add(createFlexItem(tuneKnob, mainBasis));
    mainFb.items.add(createFlexItem(mixKnob, mainBasis));
    // Row 2+
    mainFb.items.add(createFlexItem(structureKnob, mainBasis));
    mainFb.items.add(createFlexItem(brightnessKnob, mainBasis));
    mainFb.items.add(createFlexItem(dampingKnob, mainBasis));
    mainFb.items.add(createFlexItem(positionKnob, mainBasis));

    mainFb.performLayout(internalMain);
    // --- Layout Excitation Area (Excitation Engine) ---
    juce::FlexBox excitationFb;
    excitationFb.flexWrap = juce::FlexBox::Wrap::wrap;
    excitationFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    excitationFb.alignContent = juce::FlexBox::AlignContent::spaceAround;

    // Define basis for 3 columns for the top row of excitation
    float exciteBasis = (float)internalExcitation.getWidth() / 3.0f;
    // Row 1
    excitationFb.items.add(createFlexItem(exciteTypeKnob, exciteBasis));
    excitationFb.items.add(createFlexItem(sensitivityKnob, exciteBasis));
    excitationFb.items.add(createFlexItem(noiseTypeBox, exciteBasis));
    // ADSR usually laid out in 4 columns
    float adsrBasis = (float)internalExcitation.getWidth() / 4.0f;
    // Row 2
    excitationFb.items.add(createFlexItem(attackKnob, adsrBasis));
    excitationFb.items.add(createFlexItem(decayKnob, adsrBasis));
    excitationFb.items.add(createFlexItem(sustainKnob, adsrBasis));
    excitationFb.items.add(createFlexItem(releaseKnob, adsrBasis));

    excitationFb.performLayout(internalExcitation);
}