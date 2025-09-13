//================================================================================
// File: UI/SlotEditors.cpp
//================================================================================
#include "SlotEditors.h"

//==============================================================================
// DistortionSlotEditor Implementation
//==============================================================================
DistortionSlotEditor::DistortionSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix)
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

DistortionSlotEditor::~DistortionSlotEditor()
{
    apvts.removeParameterListener(paramPrefix + "DISTORTION_TYPE", this);
}

void DistortionSlotEditor::resized()
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

void DistortionSlotEditor::parameterChanged(const juce::String&, float) { updateVisibilities(); }

void DistortionSlotEditor::updateVisibilities()
{
    auto type = static_cast<int>(apvts.getRawParameterValue(paramPrefix + "DISTORTION_TYPE")->load());
    biasKnob.setVisible(type == 0);
    characterKnob.setVisible(type == 1 || type == 2);
    if (getWidth() > 0 && getHeight() > 0)
        resized();
}

//==============================================================================
// FilterSlotEditor Implementation
//==============================================================================
FilterSlotEditor::FilterSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix)
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

FilterSlotEditor::~FilterSlotEditor()
{
    apvts.removeParameterListener(paramPrefix + "FILTER_PROFILE", this);
}

void FilterSlotEditor::resized()
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

void FilterSlotEditor::parameterChanged(const juce::String&, float) { updateVisibilities(); }

void FilterSlotEditor::updateVisibilities()
{
    auto* profileParam = apvts.getRawParameterValue(paramPrefix + "FILTER_PROFILE");
    auto profile = static_cast<FilterProcessor::Profile>(static_cast<int>(profileParam->load()));

    typeBox.setVisible(profile == FilterProcessor::svfProfile);
    driveKnob.setVisible(profile == FilterProcessor::transistorLadder || profile == FilterProcessor::diodeLadder);
    if (getWidth() > 0 && getHeight() > 0)
        resized();
}


//==============================================================================
// AdvancedDelaySlotEditor Implementation
//==============================================================================
AdvancedDelaySlotEditor::AdvancedDelaySlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
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

void AdvancedDelaySlotEditor::resized()
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

//==============================================================================
// ModulationSlotEditor Implementation
//==============================================================================
ModulationSlotEditor::ModulationSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
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

void ModulationSlotEditor::resized()
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

//==============================================================================
// ReverbSlotEditor Implementation
//==============================================================================
ReverbSlotEditor::ReverbSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
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

void ReverbSlotEditor::resized()
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

//==============================================================================
// AdvancedCompressorSlotEditor Implementation
//==============================================================================
AdvancedCompressorSlotEditor::AdvancedCompressorSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
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

void AdvancedCompressorSlotEditor::resized()
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

//==============================================================================
// ChromaTapeSlotEditor Implementation
//==============================================================================
ChromaTapeSlotEditor::ChromaTapeSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
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

ChromaTapeSlotEditor::~ChromaTapeSlotEditor()
{
    stopTimer();
}

void ChromaTapeSlotEditor::resized()
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

void ChromaTapeSlotEditor::timerCallback()
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

void ChromaTapeSlotEditor::bandButtonClicked(int bandIndex)
{
    if (currentBand == bandIndex) return;
    currentBand = bandIndex;
}

void ChromaTapeSlotEditor::sliderValueChanged(juce::Slider* slider)
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

void ChromaTapeSlotEditor::updateSliderTargets()
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

void ChromaTapeSlotEditor::setupSliders()
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

void ChromaTapeSlotEditor::setupButtons()
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

void ChromaTapeSlotEditor::initializeAnimations()
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

//==============================================================================
// MorphoCompSlotEditor Implementation
//==============================================================================
MorphoCompSlotEditor::MorphoCompSlotEditor(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramPrefix)
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

void MorphoCompSlotEditor::resized()
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

//==============================================================================
// SpectralAnimatorSlotEditor Implementation
//==============================================================================
SpectralAnimatorSlotEditor::SpectralAnimatorSlotEditor(juce::AudioProcessorValueTreeState& apvtsRef, const juce::String& paramPrefix)
    : SlotEditorBase(apvtsRef, paramPrefix),
    specAnimPrefix(paramPrefix + "SPECANIM_"),
    pitchKnob(apvts, specAnimPrefix + "PITCH", "Pitch"),
    formantXKnob(apvts, specAnimPrefix + "FORMANT_X", "Formant X"),
    formantYKnob(apvts, specAnimPrefix + "FORMANT_Y", "Formant Y"),
    morphKnob(apvts, specAnimPrefix + "MORPH", "Morph"),
    transientKnob(apvts, specAnimPrefix + "TRANSIENT_PRESERVE", "Transients")
{
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

SpectralAnimatorSlotEditor::~SpectralAnimatorSlotEditor()
{
    apvts.removeParameterListener(specAnimPrefix + "MODE", this);
}

void SpectralAnimatorSlotEditor::resized()
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
void SpectralAnimatorSlotEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == specAnimPrefix + "MODE")
    {
        updateVisibilities();
    }
    juce::ignoreUnused(newValue);
}

void SpectralAnimatorSlotEditor::updateVisibilities()
{
    if (auto* modeParam = apvts.getRawParameterValue(specAnimPrefix + "MODE"))
    {
        auto mode = static_cast<int>(modeParam->load());
        pitchKnob.setVisible(mode == 0); // Pitch Mode
        formantXKnob.setVisible(mode == 1); // Formant Mode
        formantYKnob.setVisible(mode == 1);
    }

    if (getWidth() > 0 && getHeight() > 0)
        resized();
}