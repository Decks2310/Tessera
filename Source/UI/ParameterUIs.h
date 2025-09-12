// File: UI/ParameterUIs.h
#pragma once
#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

// UPDATED: Inherit from juce::SettableTooltipClient
class RotaryKnobWithLabels : public juce::Component, public juce::SettableTooltipClient
{
public:
    RotaryKnobWithLabels(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(label);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromBottom(20));
        slider.setBounds(bounds);
    }

    // FIX: Removed 'override' keyword (C3668)
    void setTooltip(const juce::String& newTooltip)
    {
        // FIX: Call the implementation provided by juce::SettableTooltipClient (C2039)
        juce::SettableTooltipClient::setTooltip(newTooltip);
        // Also ensure child components explicitly have the tooltip set
        slider.setTooltip(newTooltip);
        label.setTooltip(newTooltip);
    }
private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// =============================================================================
// NEW: Helper class for ComboBoxes (Used in PhysicalResonator)
// =============================================================================
class ComboBoxWithLabel : public juce::Component, public juce::SettableTooltipClient
{
public:
    ComboBoxWithLabel(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name)
    {
        addAndMakeVisible(comboBox);
        // Populate the combo box items from the parameter definition
        if (auto* param = apvts.getParameter(paramID))
        {
            comboBox.addItemList(param->getAllValueStrings(), 1);
        }

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramID, comboBox);
    }

    void resized() override
    {
        // Layout matching RotaryKnobWithLabels for consistency
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromBottom(20));
        // Adjust the bounds for the ComboBox aesthetics within the remaining space
        // Center the combobox vertically with a fixed height of 30
        comboBox.setBounds(bounds.withSizeKeepingCentre(bounds.getWidth(), 30));
    }

    void setTooltip(const juce::String& newTooltip)
    {
        juce::SettableTooltipClient::setTooltip(newTooltip);
        comboBox.setTooltip(newTooltip);
        label.setTooltip(newTooltip);
    }

    juce::ComboBox& getComboBox() { return comboBox; }
private:
    juce::ComboBox comboBox;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// NEW: An encapsulated component for the Input/Output vertical faders.
// UPDATED: Inherit from juce::SettableTooltipClient
class VerticalFaderWithAttachment : public juce::Component, public juce::SettableTooltipClient
{
public:
    VerticalFaderWithAttachment(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
    {
        // Configure slider for vertical movement
        slider.setSliderStyle(juce::Slider::LinearVertical);
        // Hide the text box as shown in the screenshot
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);

        /* FIX: Removed the manual setRange call. The SliderAttachment handles this automatically and robustly.
        // Explicitly set the range to ensure the LookAndFeel correctly identifies the 0 point for bipolar visualization
        if (auto* param = apvts.getParameter(paramID)) {
            slider.setRange(param->getNormalisableRange().start, param->getNormalisableRange().end);
        }
        */

        // Setup the attachment
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);

        // Configure the label
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(label);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        // Label at the top
        label.setBounds(bounds.removeFromTop(20));
        slider.setBounds(bounds);
    }

    // Implement setTooltip for consistency
    void setTooltip(const juce::String& newTooltip)
    {
        juce::SettableTooltipClient::setTooltip(newTooltip);
        slider.setTooltip(newTooltip);
        label.setTooltip(newTooltip);
    }
private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
// A vertical slider component with a label (used in ChromaTape)
// UPDATED: Inherit from juce::SettableTooltipClient
class VerticalSliderWithLabel : public juce::Component, public juce::SettableTooltipClient
{
public:
    VerticalSliderWithLabel(const juce::String& labelText)
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(label);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        // Label at the top, matching the screenshot (Saturation, Wow, Flutter labels)
        label.setBounds(bounds.removeFromTop(20));
        slider.setBounds(bounds);
    }

    // Public accessor for the internal slider
    juce::Slider& getSlider() {
        return slider;
    }

    // Implement setTooltip
    void setTooltip(const juce::String& newTooltip)
    {
        juce::SettableTooltipClient::setTooltip(newTooltip);
        slider.setTooltip(newTooltip);
        label.setTooltip(newTooltip);
    }
private:
    juce::Slider slider;
    juce::Label label;
};
// A text box for displaying parameter values
// UPDATED: Inherit from juce::SettableTooltipClient
class ParameterTextBox : public juce::Component, public juce::SettableTooltipClient,
    private juce::AudioProcessorValueTreeState::Listener,
    private juce::Timer
{
public:
    ParameterTextBox(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText);
    ~ParameterTextBox() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Implement setTooltip
    void setTooltip(const juce::String& newTooltip)
    {
        juce::SettableTooltipClient::setTooltip(newTooltip);
    }
private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void timerCallback() override {
        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramId;
    juce::String labelText;
    juce::RangedAudioParameter* parameter = nullptr;

    CustomLookAndFeel lookAndFeel;
};