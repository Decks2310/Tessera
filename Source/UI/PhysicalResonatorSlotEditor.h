/*
  ==============================================================================

    PhysicalResonatorSlotEditor.h
    Created: 11 Sep 2025 12:09:04pm
    Author:  agusr

  ==============================================================================
*/
#pragma once

// We must include SlotEditors.h first to get the base class (SlotEditorBase) and LayoutHelpers
#include "SlotEditors.h"
#include "ParameterUIs.h"

// Inherit from SlotEditorBase
class PhysicalResonatorSlotEditor : public SlotEditorBase
{
public:
    PhysicalResonatorSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void resized() override;
private:
    // Define the prefix for this module
    juce::String physResPrefix;
    // Main Controls
    ComboBoxWithLabel modelBox;
    RotaryKnobWithLabels tuneKnob, structureKnob, brightnessKnob, dampingKnob, positionKnob, mixKnob;
    // Excitation Engine Controls
    RotaryKnobWithLabels exciteTypeKnob, sensitivityKnob;
    // Advanced Excitation (ADSR + Noise)
    ComboBoxWithLabel noiseTypeBox;
    RotaryKnobWithLabels attackKnob, decayKnob, sustainKnob, releaseKnob;
    // Grouping components for visual structure
    juce::GroupComponent mainGroup, excitationGroup;
};