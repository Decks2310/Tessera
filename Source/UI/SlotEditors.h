//================================================================================
// File: UI/SlotEditors.h
//================================================================================
#pragma once
#include <JuceHeader.h>
#include "ParameterUIs.h"
#include "../FX_Modules/FilterProcessor.h"
#include <map>

namespace LayoutHelpers {
    constexpr float minKnobWidth = 50.0f;
    constexpr float labelHeight = 20.0f;
    constexpr float minKnobHeight = minKnobWidth + labelHeight;

    inline juce::FlexItem createFlexKnob(juce::Component& component, float basis) {
        return juce::FlexItem(component)
            .withFlex(1.0f, 1.0f, basis)
            .withMinWidth(minKnobWidth)
            .withMinHeight(minKnobHeight);
    }

    inline juce::NormalisableRange<double> toDoubleRange(const juce::NormalisableRange<float>& range)
    {
        return juce::NormalisableRange<double>(
            (double)range.start,
            (double)range.end,
            [range](double start, double end, double normalized) {
                juce::ignoreUnused(start, end);
                return (double)range.convertFrom0to1((float)normalized);
            },
            [range](double start, double end, double value) {
                juce::ignoreUnused(start, end);
                return (double)range.convertTo0to1((float)value);
            },
            [range](double start, double end, double value) {
                juce::ignoreUnused(start, end);
                return (double)range.snapToLegalValue((float)value);
            }
        );
    }
}


class SlotEditorBase : public juce::Component
{
public:
    SlotEditorBase(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
        : apvts(apvts), paramPrefix(paramPrefix) {
    }
protected:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramPrefix;
};

class DistortionSlotEditor : public SlotEditorBase,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    DistortionSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix);
    ~DistortionSlotEditor() override;
    void resized() override;
private:
    void parameterChanged(const juce::String&, float) override;
    void updateVisibilities();
    RotaryKnobWithLabels driveKnob, levelKnob, biasKnob, characterKnob;
    juce::ComboBox typeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
};

class FilterSlotEditor : public SlotEditorBase,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    FilterSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix);
    ~FilterSlotEditor() override;
    void resized() override;
private:
    void parameterChanged(const juce::String&, float) override;
    void updateVisibilities();
    RotaryKnobWithLabels cutoffKnob, resonanceKnob, driveKnob;
    juce::ComboBox profileBox, typeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAttachment, typeAttachment;
};

class AdvancedDelaySlotEditor : public SlotEditorBase
{
public:
    AdvancedDelaySlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    juce::String advDelayPrefix;
    RotaryKnobWithLabels timeKnob, feedbackKnob, mixKnob, colorKnob, wowKnob, flutterKnob, ageKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};

class ModulationSlotEditor : public SlotEditorBase
{
public:
    ModulationSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    RotaryKnobWithLabels rateKnob, depthKnob, feedbackKnob, mixKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};

class ReverbSlotEditor : public SlotEditorBase
{
public:
    ReverbSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    RotaryKnobWithLabels roomSizeKnob, dampingKnob, mixKnob, widthKnob;
};

class AdvancedCompressorSlotEditor : public SlotEditorBase
{
public:
    AdvancedCompressorSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    juce::String advCompPrefix;
    RotaryKnobWithLabels thresholdKnob, ratioKnob, attackKnob, releaseKnob, makeupKnob;
    juce::ComboBox topologyBox, detectorBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> topologyAttachment, detectorAttachment;
};

class ChromaTapeSlotEditor : public SlotEditorBase,
    private juce::Timer
{
public:
    ChromaTapeSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    ~ChromaTapeSlotEditor() override;
    void resized() override;
private:
    void timerCallback() override;
    void bandButtonClicked(int bandIndex);
    void sliderValueChanged(juce::Slider* slider);
    void updateSliderTargets();
    void setupSliders();
    void setupButtons();
    void initializeAnimations();

    juce::String ctPrefix;
    int currentBand = 0;
    RotaryKnobWithLabels lowMidCrossKnob, midHighCrossKnob;
    VerticalSliderWithLabel saturationSlider, wowSlider, flutterSlider;
    juce::TextButton lowButton{ "Low" }, midButton{ "Mid" }, highButton{ "High" };
    struct AnimationState { float current = 0.0f; float target = 0.0f; };
    std::map<juce::Slider*, AnimationState> sliderAnimations;
    std::map<juce::Slider*, std::array<juce::RangedAudioParameter*, 3>> sliderParams;
};

class MorphoCompSlotEditor : public SlotEditorBase
{
public:
    MorphoCompSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    RotaryKnobWithLabels amountKnob, responseKnob, mixKnob, morphXKnob, morphYKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};

class SpectralAnimatorSlotEditor : public SlotEditorBase,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    SpectralAnimatorSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix);
    ~SpectralAnimatorSlotEditor() override;
    void resized() override;
private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateVisibilities();

    juce::String specAnimPrefix;
    RotaryKnobWithLabels pitchKnob, formantXKnob, formantYKnob, morphKnob, transientKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};

class HelicalDelaySlotEditor : public SlotEditorBase
{
public:
    HelicalDelaySlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    RotaryKnobWithLabels timeKnob, pitchKnob, feedbackKnob, degradeKnob, textureKnob, mixKnob;
};

class ChronoVerbSlotEditor : public SlotEditorBase
{
public:
    ChronoVerbSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    RotaryKnobWithLabels sizeKnob, decayKnob, diffusionKnob, dampingKnob, modulationKnob, balanceKnob, mixKnob;
    juce::ToggleButton freezeButton{ "Freeze" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
};

class TectonicDelaySlotEditor : public SlotEditorBase
{
public:
    TectonicDelaySlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix);
    void resized() override;
private:
    RotaryKnobWithLabels lowTimeKnob, midTimeKnob, highTimeKnob;
    RotaryKnobWithLabels feedbackKnob, lowMidCrossKnob, midHighCrossKnob;
    RotaryKnobWithLabels decayDriveKnob, decayTextureKnob, decayDensityKnob, decayPitchKnob;
    RotaryKnobWithLabels mixKnob;
    juce::ToggleButton linkButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;
};