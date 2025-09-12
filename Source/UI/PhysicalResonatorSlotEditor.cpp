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
    // Initialize ALL Main Controls
    modelBox(apvts, physResPrefix + "MODEL", "Model"),
    tuneKnob(apvts, physResPrefix + "TUNE", "Tune"),
    structureKnob(apvts, physResPrefix + "STRUCTURE", "Structure"),
    brightnessKnob(apvts, physResPrefix + "BRIGHTNESS", "Brightness"),
    dampingKnob(apvts, physResPrefix + "DAMPING", "Damping"),
    positionKnob(apvts, physResPrefix + "POSITION", "Position"),
    mixKnob(apvts, physResPrefix + "MIX", "Mix"),
    // Initialize ALL Excitation Engine Controls
    exciteTypeKnob(apvts, physResPrefix + "EXCITE_TYPE", "Excite Type"),
    sensitivityKnob(apvts, physResPrefix + "SENSITIVITY", "Sensitivity"),
    noiseTypeBox(apvts, physResPrefix + "NOISE_TYPE", "Noise Type"),
    attackKnob(apvts, physResPrefix + "ATTACK", "Attack"),
    decayKnob(apvts, physResPrefix + "DECAY", "Decay"),
    sustainKnob(apvts, physResPrefix + "SUSTAIN", "Sustain"),
    releaseKnob(apvts, physResPrefix + "RELEASE", "Release")
{
    // Setup Groups
    addAndMakeVisible(mainGroup);
    mainGroup.setText("Resonator Core");
    mainGroup.setLookAndFeel(&getLookAndFeel());
    addAndMakeVisible(excitationGroup);
    excitationGroup.setText("Excitation Engine");
    excitationGroup.setLookAndFeel(&getLookAndFeel());

    // Add Main controls to the mainGroup
    mainGroup.addAndMakeVisible(modelBox);
    mainGroup.addAndMakeVisible(tuneKnob);
    mainGroup.addAndMakeVisible(structureKnob);
    mainGroup.addAndMakeVisible(brightnessKnob);
    mainGroup.addAndMakeVisible(dampingKnob);
    mainGroup.addAndMakeVisible(positionKnob);
    mainGroup.addAndMakeVisible(mixKnob);

    // Add Excitation controls to the excitationGroup
    excitationGroup.addAndMakeVisible(exciteTypeKnob);
    excitationGroup.addAndMakeVisible(sensitivityKnob);
    excitationGroup.addAndMakeVisible(noiseTypeBox);
    excitationGroup.addAndMakeVisible(attackKnob);
    excitationGroup.addAndMakeVisible(decayKnob);
    excitationGroup.addAndMakeVisible(sustainKnob);
    excitationGroup.addAndMakeVisible(releaseKnob);

    // Apply the custom LookAndFeel to all RotaryKnobWithLabels
    exciteTypeKnob.setLookAndFeel(&customKnobStyle);
    sensitivityKnob.setLookAndFeel(&customKnobStyle);
    attackKnob.setLookAndFeel(&customKnobStyle);
    decayKnob.setLookAndFeel(&customKnobStyle);
    sustainKnob.setLookAndFeel(&customKnobStyle);
    releaseKnob.setLookAndFeel(&customKnobStyle);
    tuneKnob.setLookAndFeel(&customKnobStyle);
    structureKnob.setLookAndFeel(&customKnobStyle);
    brightnessKnob.setLookAndFeel(&customKnobStyle);
    dampingKnob.setLookAndFeel(&customKnobStyle);
    positionKnob.setLookAndFeel(&customKnobStyle);
    mixKnob.setLookAndFeel(&customKnobStyle);
}

PhysicalResonatorSlotEditor::~PhysicalResonatorSlotEditor()
{
    // You MUST set the LookAndFeel to nullptr before the LookAndFeel instance
    // (customKnobStyle) is destroyed to prevent crashes.
    exciteTypeKnob.setLookAndFeel(nullptr);
    sensitivityKnob.setLookAndFeel(nullptr);
    attackKnob.setLookAndFeel(nullptr);
    decayKnob.setLookAndFeel(nullptr);
    sustainKnob.setLookAndFeel(nullptr);
    releaseKnob.setLookAndFeel(nullptr);
    tuneKnob.setLookAndFeel(nullptr);
    structureKnob.setLookAndFeel(nullptr);
    brightnessKnob.setLookAndFeel(nullptr);
    dampingKnob.setLookAndFeel(nullptr);
    positionKnob.setLookAndFeel(nullptr);
    mixKnob.setLookAndFeel(nullptr);
}

void PhysicalResonatorSlotEditor::paint(juce::Graphics& g) {
    // Optional: Set background color if not handled globally
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PhysicalResonatorSlotEditor::resized()
{
    // FIX 3: Define the layout and set bounds for all components.
    const int margin = 10;
    // This offset accounts for the space the GroupComponent uses for its title text.
    const int titleOffset = 20;

    auto bounds = getLocalBounds();

    // 1. Layout the two groups (stacked vertically)
    int groupHeight = (bounds.getHeight() - margin * 3) / 2;

    mainGroup.setBounds(margin, margin, bounds.getWidth() - 2 * margin, groupHeight);
    excitationGroup.setBounds(margin, groupHeight + 2 * margin, bounds.getWidth() - 2 * margin, groupHeight);

    // 2. Layout Resonator Core internals (Example)
    auto coreBounds = mainGroup.getLocalBounds().reduced(margin);
    coreBounds.removeFromTop(titleOffset);

    // Using FlexBox for a more robust layout
    juce::FlexBox mainFb;
    mainFb.flexWrap = juce::FlexBox::Wrap::wrap;
    mainFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    mainFb.alignContent = juce::FlexBox::AlignContent::spaceAround;

    mainFb.items.add(juce::FlexItem(modelBox).withFlex(1.0f));
    mainFb.items.add(juce::FlexItem(tuneKnob).withFlex(1.0f));
    mainFb.items.add(juce::FlexItem(structureKnob).withFlex(1.0f));
    mainFb.items.add(juce::FlexItem(brightnessKnob).withFlex(1.0f));
    mainFb.items.add(juce::FlexItem(dampingKnob).withFlex(1.0f));
    mainFb.items.add(juce::FlexItem(positionKnob).withFlex(1.0f));
    mainFb.items.add(juce::FlexItem(mixKnob).withFlex(1.0f));

    mainFb.performLayout(coreBounds);


    // 3. Layout Excitation Engine internals
    auto engineBounds = excitationGroup.getLocalBounds().reduced(margin);
    engineBounds.removeFromTop(titleOffset);

    juce::FlexBox exciteFb;
    exciteFb.flexWrap = juce::FlexBox::Wrap::wrap;
    exciteFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    exciteFb.alignContent = juce::FlexBox::AlignContent::spaceAround;

    exciteFb.items.add(juce::FlexItem(exciteTypeKnob).withFlex(1.0f));
    exciteFb.items.add(juce::FlexItem(sensitivityKnob).withFlex(1.0f));
    exciteFb.items.add(juce::FlexItem(noiseTypeBox).withFlex(1.0f));
    exciteFb.items.add(juce::FlexItem(attackKnob).withFlex(1.0f));
    exciteFb.items.add(juce::FlexItem(decayKnob).withFlex(1.0f));
    exciteFb.items.add(juce::FlexItem(sustainKnob).withFlex(1.0f));
    exciteFb.items.add(juce::FlexItem(releaseKnob).withFlex(1.0f));

    exciteFb.performLayout(engineBounds);
}