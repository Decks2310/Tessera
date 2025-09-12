//================================================================================
// File: UI/SlotEditors.h
//================================================================================
#pragma once
#include <JuceHeader.h>
#include "ParameterUIs.h"
#include "../FX_Modules/FilterProcessor.h"
#include <map>

// Helper namespace for layout constants and functions
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

// A simple base class for our new editors
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

//==============================================================================
// Standard Editors
//==============================================================================

class DistortionSlotEditor : public SlotEditorBase,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    DistortionSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix)
        : SlotEditorBase(apvtsRef, paramPrefix),
        driveKnob(apvts, paramPrefix + "DISTORTION_DRIVE", "Drive"),
        levelKnob(apvts, paramPrefix + "DISTORTION_LEVEL", "Level"),
        biasKnob(apvts, paramPrefix + "DISTORTION_BIAS", "Bias"),
        characterKnob(apvts, paramPrefix + "DISTORTION_CHARACTER", "Character")
    {
        addAndMakeVisible(driveKnob);
        addAndMakeVisible(levelKnob);
        addAndMakeVisible(biasKnob);
        addAndMakeVisible(characterKnob);

        typeBox.addItemList(apvts.getParameter(paramPrefix + "DISTORTION_TYPE")->getAllValueStrings(), 1);
        addAndMakeVisible(typeBox);
        typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramPrefix + "DISTORTION_TYPE", typeBox);

        apvts.addParameterListener(paramPrefix + "DISTORTION_TYPE", this);
        updateVisibilities();
    }

    ~DistortionSlotEditor() override
    {
        apvts.removeParameterListener(paramPrefix + "DISTORTION_TYPE", this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        typeBox.setBounds(bounds.removeFromTop(30).reduced(5, 0));

        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;

        float basis = (float)bounds.getWidth() / 2.0f;
        fb.items.add(LayoutHelpers::createFlexKnob(driveKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(levelKnob, basis));

        if (biasKnob.isVisible())
            fb.items.add(LayoutHelpers::createFlexKnob(biasKnob, basis));
        if (characterKnob.isVisible())
            fb.items.add(LayoutHelpers::createFlexKnob(characterKnob, basis));

        fb.performLayout(bounds);
    }
private:
    void parameterChanged(const juce::String&, float) override {
        updateVisibilities();
    }

    void updateVisibilities()
    {
        auto type = static_cast<int>(apvts.getRawParameterValue(paramPrefix + "DISTORTION_TYPE")->load());
        biasKnob.setVisible(type == 0);
        characterKnob.setVisible(type == 1 || type == 2);
        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    RotaryKnobWithLabels driveKnob, levelKnob, biasKnob, characterKnob;
    juce::ComboBox typeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
};
class FilterSlotEditor : public SlotEditorBase,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    FilterSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix)
        : SlotEditorBase(apvtsRef, paramPrefix),
        cutoffKnob(apvts, paramPrefix + "FILTER_CUTOFF", "Cutoff"),
        resonanceKnob(apvts, paramPrefix + "FILTER_RESONANCE", "Resonance"),
        driveKnob(apvts, paramPrefix + "FILTER_DRIVE", "Drive")
    {
        addAndMakeVisible(cutoffKnob);
        addAndMakeVisible(resonanceKnob);
        addAndMakeVisible(driveKnob);

        profileBox.addItemList(apvts.getParameter(paramPrefix + "FILTER_PROFILE")->getAllValueStrings(), 1);
        addAndMakeVisible(profileBox);
        profileAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramPrefix + "FILTER_PROFILE", profileBox);

        typeBox.addItemList(apvts.getParameter(paramPrefix + "FILTER_TYPE")->getAllValueStrings(), 1);
        addAndMakeVisible(typeBox);
        typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramPrefix + "FILTER_TYPE", typeBox);

        apvts.addParameterListener(paramPrefix + "FILTER_PROFILE", this);
        updateVisibilities();
    }

    ~FilterSlotEditor() override
    {
        apvts.removeParameterListener(paramPrefix + "FILTER_PROFILE", this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        auto topStrip = bounds.removeFromTop(30);

        if (typeBox.isVisible())
        {
            profileBox.setBounds(topStrip.removeFromLeft(topStrip.getWidth() / 2).reduced(5, 0));
            typeBox.setBounds(topStrip.reduced(5, 0));
        }
        else
        {
            profileBox.setBounds(topStrip.reduced(5, 0));
        }

        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;
        int visibleKnobs = 2 + (driveKnob.isVisible() ? 1 : 0);
        float basis = (float)bounds.getWidth() / (float)visibleKnobs;

        fb.items.add(LayoutHelpers::createFlexKnob(cutoffKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(resonanceKnob, basis));
        if (driveKnob.isVisible())
            fb.items.add(LayoutHelpers::createFlexKnob(driveKnob, basis));

        fb.performLayout(bounds);
    }
private:
    void parameterChanged(const juce::String&, float) override {
        updateVisibilities();
    }

    void updateVisibilities()
    {
        auto* profileParam = apvts.getRawParameterValue(paramPrefix + "FILTER_PROFILE");
        auto profile = static_cast<FilterProcessor::Profile>(static_cast<int>(profileParam->load()));

        typeBox.setVisible(profile == FilterProcessor::svfProfile);
        driveKnob.setVisible(profile == FilterProcessor::transistorLadder || profile == FilterProcessor::diodeLadder);
        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    RotaryKnobWithLabels cutoffKnob, resonanceKnob, driveKnob;
    juce::ComboBox profileBox, typeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAttachment, typeAttachment;
};
class AdvancedDelaySlotEditor : public SlotEditorBase
{
public:
    AdvancedDelaySlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
        : SlotEditorBase(apvts, paramPrefix),
        advDelayPrefix(paramPrefix + "ADVDELAY_"),
        timeKnob(apvts, advDelayPrefix + "TIME", "Time"),
        feedbackKnob(apvts, advDelayPrefix + "FEEDBACK", "Feedback"),
        mixKnob(apvts, advDelayPrefix + "MIX", "Mix"),
        colorKnob(apvts, advDelayPrefix + "COLOR", "Color"),
        wowKnob(apvts, advDelayPrefix + "WOW", "Wow"),

        flutterKnob(apvts, advDelayPrefix + "FLUTTER", "Flutter"),
        ageKnob(apvts, advDelayPrefix + "AGE", "Age")
    {
        addAndMakeVisible(timeKnob);
        addAndMakeVisible(feedbackKnob);
        addAndMakeVisible(mixKnob);
        addAndMakeVisible(colorKnob);
        addAndMakeVisible(wowKnob);
        addAndMakeVisible(flutterKnob);
        addAndMakeVisible(ageKnob);

        if (apvts.getParameter(advDelayPrefix + "MODE"))
        {
            modeBox.addItemList(apvts.getParameter(advDelayPrefix + "MODE")->getAllValueStrings(), 1);
            addAndMakeVisible(modeBox);
            modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, advDelayPrefix + "MODE", modeBox);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        if (modeBox.isVisible())
            modeBox.setBounds(bounds.removeFromTop(30).reduced(5, 0));

        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;

        float basis = (float)bounds.getWidth() / 3.0f;
        if (basis < LayoutHelpers::minKnobWidth && bounds.getWidth() > LayoutHelpers::minKnobWidth * 2)
            basis = (float)bounds.getWidth() / 2.0f;
        fb.items.add(LayoutHelpers::createFlexKnob(timeKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(feedbackKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(mixKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(colorKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(wowKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(flutterKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(ageKnob, basis));

        fb.performLayout(bounds);
    }
private:
    juce::String advDelayPrefix;
    RotaryKnobWithLabels timeKnob, feedbackKnob, mixKnob, colorKnob, wowKnob, flutterKnob, ageKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};
class ModulationSlotEditor : public SlotEditorBase
{
public:
    ModulationSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
        : SlotEditorBase(apvts, paramPrefix),
        rateKnob(apvts, paramPrefix + "MODULATION_RATE", "Rate"),
        depthKnob(apvts, paramPrefix + "MODULATION_DEPTH", "Depth"),
        feedbackKnob(apvts, paramPrefix + "MODULATION_FEEDBACK", "Feedback"),
        mixKnob(apvts, paramPrefix + "MODULATION_MIX", "Mix")
    {
        addAndMakeVisible(rateKnob);
        addAndMakeVisible(depthKnob);
        addAndMakeVisible(feedbackKnob);
        addAndMakeVisible(mixKnob);

        modeBox.addItemList(apvts.getParameter(paramPrefix + "MODULATION_MODE")->getAllValueStrings(), 1);
        addAndMakeVisible(modeBox);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramPrefix + "MODULATION_MODE", modeBox);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        modeBox.setBounds(bounds.removeFromTop(30).reduced(5, 0));

        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;

        float basis = (float)bounds.getWidth() / 2.0f;
        fb.items.add(LayoutHelpers::createFlexKnob(rateKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(depthKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(feedbackKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(mixKnob, basis));

        fb.performLayout(bounds);
    }
private:
    RotaryKnobWithLabels rateKnob, depthKnob, feedbackKnob, mixKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};

class ReverbSlotEditor : public SlotEditorBase
{
public:
    ReverbSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
        : SlotEditorBase(apvts, paramPrefix),
        roomSizeKnob(apvts, paramPrefix + "REVERB_ROOM_SIZE", "Room Size"),
        dampingKnob(apvts, paramPrefix + "REVERB_DAMPING", "Damping"),
        mixKnob(apvts, paramPrefix + "REVERB_MIX", "Mix"),
        widthKnob(apvts, paramPrefix + "REVERB_WIDTH", "Width")
    {
        addAndMakeVisible(roomSizeKnob);
        addAndMakeVisible(dampingKnob);
        addAndMakeVisible(mixKnob);
        addAndMakeVisible(widthKnob);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;

        float basis = (float)bounds.getWidth() / 2.0f;

        fb.items.add(LayoutHelpers::createFlexKnob(roomSizeKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(dampingKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(mixKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(widthKnob, basis));

        fb.performLayout(bounds);
    }
private:
    RotaryKnobWithLabels roomSizeKnob, dampingKnob, mixKnob, widthKnob;
};
class AdvancedCompressorSlotEditor : public SlotEditorBase
{
public:
    AdvancedCompressorSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
        : SlotEditorBase(apvts, paramPrefix),
        advCompPrefix(paramPrefix + "ADVCOMP_"),
        thresholdKnob(apvts, advCompPrefix + "THRESHOLD", "Threshold"),
        ratioKnob(apvts, advCompPrefix + "RATIO", "Ratio"),
        attackKnob(apvts, advCompPrefix + "ATTACK", "Attack"),
        releaseKnob(apvts, advCompPrefix + "RELEASE", "Release"),
        makeupKnob(apvts, advCompPrefix + "MAKEUP", "Makeup")
    {

        addAndMakeVisible(thresholdKnob);
        addAndMakeVisible(ratioKnob);
        addAndMakeVisible(attackKnob);
        addAndMakeVisible(releaseKnob);
        addAndMakeVisible(makeupKnob);

        if (apvts.getParameter(advCompPrefix + "TOPOLOGY") && apvts.getParameter(advCompPrefix + "DETECTOR"))
        {
            topologyBox.addItemList(apvts.getParameter(advCompPrefix + "TOPOLOGY")->getAllValueStrings(), 1);
            addAndMakeVisible(topologyBox);
            topologyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, advCompPrefix + "TOPOLOGY", topologyBox);

            detectorBox.addItemList(apvts.getParameter(advCompPrefix + "DETECTOR")->getAllValueStrings(), 1);
            addAndMakeVisible(detectorBox);
            detectorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, advCompPrefix + "DETECTOR", detectorBox);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        auto topStrip = bounds.removeFromTop(30);

        if (topologyBox.isVisible() && detectorBox.isVisible())
        {
            topologyBox.setBounds(topStrip.removeFromLeft(topStrip.getWidth() / 2).reduced(5, 0));
            detectorBox.setBounds(topStrip.reduced(5, 0));
        }

        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;

        float basis = (float)bounds.getWidth() / 3.0f;
        if (basis < LayoutHelpers::minKnobWidth && bounds.getWidth() > LayoutHelpers::minKnobWidth * 2)
            basis = (float)bounds.getWidth() / 2.0f;
        fb.items.add(LayoutHelpers::createFlexKnob(thresholdKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(ratioKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(attackKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(releaseKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(makeupKnob, basis));

        fb.performLayout(bounds);
    }
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
    ChromaTapeSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
        : SlotEditorBase(apvts, paramPrefix),
        ctPrefix(paramPrefix + "CT_"),
        lowMidCrossKnob(apvts, ctPrefix + "LOWMID_CROSS", "L/M Blend"),
        midHighCrossKnob(apvts, ctPrefix + "MIDHIGH_CROSS", "M/H Blend"),
        saturationSlider("Saturation"),
        wowSlider("Wow"),
        flutterSlider("Flutter")
    {
        addAndMakeVisible(lowMidCrossKnob);
        addAndMakeVisible(midHighCrossKnob);
        addAndMakeVisible(saturationSlider);
        addAndMakeVisible(wowSlider);
        addAndMakeVisible(flutterSlider);

        const std::array<juce::String, 3> bands = { "LOW", "MID", "HIGH" };
        for (int i = 0; i < 3; ++i)
        {
            sliderParams[&saturationSlider.getSlider()][i] = apvts.getParameter(ctPrefix + bands[i] + "_SATURATION");
            sliderParams[&wowSlider.getSlider()][i] = apvts.getParameter(ctPrefix + bands[i] + "_WOW");
            sliderParams[&flutterSlider.getSlider()][i] = apvts.getParameter(ctPrefix + bands[i] + "_FLUTTER");
        }

        setupSliders();
        setupButtons();
        lowButton.setToggleState(true, juce::dontSendNotification);
        updateSliderTargets();
        initializeAnimations();
        startTimerHz(60);
    }

    ~ChromaTapeSlotEditor() override
    {
        stopTimer();
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        int crossoverHeight = 100;
        int buttonHeight = 40;
        auto crossoverArea = bounds.removeFromTop(crossoverHeight);
        auto buttonArea = bounds.removeFromBottom(buttonHeight);
        auto sliderArea = bounds;
        juce::FlexBox crossoverFb;
        crossoverFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        crossoverFb.items.add(LayoutHelpers::createFlexKnob(lowMidCrossKnob, 100.0f));
        crossoverFb.items.add(LayoutHelpers::createFlexKnob(midHighCrossKnob, 100.0f));
        crossoverFb.performLayout(crossoverArea);

        juce::FlexBox sliderFb;
        sliderFb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        sliderFb.alignItems = juce::FlexBox::AlignItems::stretch;
        auto createSliderItem = [&](VerticalSliderWithLabel& slider) {
            return juce::FlexItem(slider).withFlex(1.0f).withMinWidth(50.0f).withMargin(5);
            };
        sliderFb.items.add(createSliderItem(saturationSlider));
        sliderFb.items.add(createSliderItem(wowSlider));
        sliderFb.items.add(createSliderItem(flutterSlider));
        sliderFb.performLayout(sliderArea);

        int btnHeight = 30;
        lowButton.setBounds(buttonArea.withSize(saturationSlider.getWidth(), btnHeight)
            .withCentre(juce::Point<int>(saturationSlider.getBounds().getCentreX(), buttonArea.getCentreY())));
        midButton.setBounds(buttonArea.withSize(wowSlider.getWidth(), btnHeight)
            .withCentre(juce::Point<int>(wowSlider.getBounds().getCentreX(), buttonArea.getCentreY())));
        highButton.setBounds(buttonArea.withSize(flutterSlider.getWidth(), btnHeight)
            .withCentre(juce::Point<int>(flutterSlider.getBounds().getCentreX(), buttonArea.getCentreY())));
    }
private:
    void timerCallback() override
    {
        updateSliderTargets();
        const float animationSpeed = 0.25f;

        for (auto& pair : sliderAnimations)
        {
            juce::Slider* slider = pair.first;
            AnimationState& state = pair.second;

            if (std::abs(state.target - state.current) > 1e-4)
            {
                state.current += (state.target - state.current) * animationSpeed;
                slider->setValue(slider->proportionOfLengthToValue(state.current), juce::dontSendNotification);
            }
            else if (state.current != state.target)
            {
                state.current = state.target;
                slider->setValue(slider->proportionOfLengthToValue(state.current), juce::dontSendNotification);
            }
        }
    }

    void bandButtonClicked(int bandIndex)
    {
        if (currentBand == bandIndex) return;
        currentBand = bandIndex;
    }

    void sliderValueChanged(juce::Slider* slider)
    {
        auto* param = sliderParams[slider][currentBand];
        if (param)
        {
            float normalizedValue = (float)slider->valueToProportionOfLength(slider->getValue());
            if (std::abs(param->getValue() - normalizedValue) > 1e-6)
            {
                param->setValueNotifyingHost(normalizedValue);
            }

            sliderAnimations[slider].current = normalizedValue;
            sliderAnimations[slider].target = normalizedValue;
        }
    }

    void updateSliderTargets()
    {
        for (auto& pair : sliderParams)
        {
            juce::Slider* slider = pair.first;
            auto* param = pair.second[currentBand];
            if (param)
            {
                slider->setNormalisableRange(LayoutHelpers::toDoubleRange(param->getNormalisableRange()));
                sliderAnimations[slider].target = param->getValue();
            }
        }
    }

    void setupSliders()
    {
        auto configureSlider = [&](VerticalSliderWithLabel& vSlider)
            {
                juce::Slider& slider = vSlider.getSlider();
                slider.onValueChange = [this, &slider] { sliderValueChanged(&slider); };
            };

        configureSlider(saturationSlider);
        configureSlider(wowSlider);
        configureSlider(flutterSlider);
    }

    void setupButtons()
    {
        addAndMakeVisible(lowButton);
        addAndMakeVisible(midButton);
        addAndMakeVisible(highButton);
        const int radioGroup = 1001;
        lowButton.setRadioGroupId(radioGroup);
        midButton.setRadioGroupId(radioGroup);
        highButton.setRadioGroupId(radioGroup);
        lowButton.setClickingTogglesState(true);
        midButton.setClickingTogglesState(true);
        highButton.setClickingTogglesState(true);

        auto* laf = &getLookAndFeel();
        lowButton.setLookAndFeel(laf);
        midButton.setLookAndFeel(laf);
        highButton.setLookAndFeel(laf);
        lowButton.onClick = [this] { bandButtonClicked(0); };
        midButton.onClick = [this] { bandButtonClicked(1); };
        highButton.onClick = [this] { bandButtonClicked(2); };
    }

    void initializeAnimations()
    {
        for (auto& pair : sliderParams)
        {
            juce::Slider* slider = pair.first;
            auto* param = pair.second[0];
            if (param)
            {
                slider->setNormalisableRange(LayoutHelpers::toDoubleRange(param->getNormalisableRange()));
            }
        }

        updateSliderTargets();
        for (auto& pair : sliderAnimations)
        {
            pair.second.current = pair.second.target;
            juce::Slider* slider = pair.first;
            slider->setValue(slider->proportionOfLengthToValue(pair.second.current), juce::dontSendNotification);
        }
    }

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
    MorphoCompSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
        : SlotEditorBase(apvts, paramPrefix),
        amountKnob(apvts, paramPrefix + "MORPHO_AMOUNT", "Amount"),
        responseKnob(apvts, paramPrefix + "MORPHO_RESPONSE", "Response"),
        mixKnob(apvts, paramPrefix + "MORPHO_MIX", "Mix"),
        morphXKnob(apvts, paramPrefix + "MORPHO_X", "Morph X"),
        morphYKnob(apvts, paramPrefix + "MORPHO_Y", "Morph Y")

    {
        addAndMakeVisible(amountKnob);
        addAndMakeVisible(responseKnob);
        addAndMakeVisible(mixKnob);
        addAndMakeVisible(morphXKnob);
        addAndMakeVisible(morphYKnob);

        modeBox.addItemList(apvts.getParameter(paramPrefix + "MORPHO_MODE")->getAllValueStrings(), 1);
        addAndMakeVisible(modeBox);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramPrefix + "MORPHO_MODE", modeBox);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        modeBox.setBounds(bounds.removeFromTop(30).reduced(5, 0));


        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;
        float basis = (float)bounds.getWidth() / 3.0f;
        fb.items.add(LayoutHelpers::createFlexKnob(amountKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(responseKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(mixKnob, basis));

        fb.items.add(LayoutHelpers::createFlexKnob(morphXKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(morphYKnob, basis));

        fb.performLayout(bounds);
    }
private:
    RotaryKnobWithLabels amountKnob, responseKnob, mixKnob, morphXKnob, morphYKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};
class PlaceholderSlotEditor : public SlotEditorBase
{
public:
    PlaceholderSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix, const juce::String& message)
        : SlotEditorBase(apvts, paramPrefix)
    {
        label.setText(message + "\n(DSP Pending)", juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    }
    void resized() override { label.setBounds(getLocalBounds()); }
private:
    juce::Label label;
};
//==============================================================================
// NEW: Spectral Animator Editor
// FIX: Ensure this definition is complete (it was truncated in the input)
//==============================================================================
class SpectralAnimatorSlotEditor : public SlotEditorBase,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    SpectralAnimatorSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix)
        : SlotEditorBase(apvtsRef, paramPrefix),
        specAnimPrefix(paramPrefix + "SPECANIM_"),
        pitchKnob(apvts, specAnimPrefix + "PITCH", "Pitch"),
        formantXKnob(apvts, specAnimPrefix + "FORMANT_X", "Formant X"),
        formantYKnob(apvts, specAnimPrefix + "FORMANT_Y", "Formant Y"),
        morphKnob(apvts,
            specAnimPrefix + "MORPH", "Morph"),
        transientKnob(apvts, specAnimPrefix + "TRANSIENT_PRESERVE", "Transients")
    {
        // FIX: Complete the truncated constructor
        addAndMakeVisible(pitchKnob);
        addAndMakeVisible(formantXKnob);
        addAndMakeVisible(formantYKnob);
        addAndMakeVisible(morphKnob);
        addAndMakeVisible(transientKnob);

        // Setup Mode ComboBox
        if (auto* modeParam = apvts.getParameter(specAnimPrefix + "MODE"))
        {
            modeBox.addItemList(modeParam->getAllValueStrings(), 1);
            addAndMakeVisible(modeBox);
            modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, specAnimPrefix + "MODE", modeBox);
        }

        // Register listener for mode changes
        apvts.addParameterListener(specAnimPrefix + "MODE", this);
        updateVisibilities(); // Initial visibility setup
    }

    // FIX: Add missing methods (Destructor, resized, etc.)
    ~SpectralAnimatorSlotEditor() override
    {
        apvts.removeParameterListener(specAnimPrefix + "MODE", this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        if (modeBox.isVisible())
            modeBox.setBounds(bounds.removeFromTop(30).reduced(5, 0));

        juce::FlexBox fb;
        fb.flexWrap = juce::FlexBox::Wrap::wrap;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        fb.alignContent = juce::FlexBox::AlignContent::spaceAround;

        float basis = (float)bounds.getWidth() / 3.0f;

        if (pitchKnob.isVisible()) fb.items.add(LayoutHelpers::createFlexKnob(pitchKnob, basis));
        if (formantXKnob.isVisible()) fb.items.add(LayoutHelpers::createFlexKnob(formantXKnob, basis));
        if (formantYKnob.isVisible()) fb.items.add(LayoutHelpers::createFlexKnob(formantYKnob, basis));

        fb.items.add(LayoutHelpers::createFlexKnob(morphKnob, basis));
        fb.items.add(LayoutHelpers::createFlexKnob(transientKnob, basis));

        fb.performLayout(bounds);
    }
private:
    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        if (parameterID == specAnimPrefix + "MODE")
        {
            updateVisibilities();
        }
        juce::ignoreUnused(newValue);
    }

    void updateVisibilities()
    {
        if (auto* modeParam = apvts.getRawParameterValue(specAnimPrefix + "MODE"))
        {
            auto mode = static_cast<int>(modeParam->load());
            pitchKnob.setVisible(mode == 0); // Pitch Mode
            formantXKnob.setVisible(mode == 1);
            // Formant Mode
            formantYKnob.setVisible(mode == 1);
        }

        if (getWidth() > 0 && getHeight() > 0)
            resized();
    }

    juce::String specAnimPrefix;
    RotaryKnobWithLabels pitchKnob, formantXKnob, formantYKnob, morphKnob, transientKnob;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
};