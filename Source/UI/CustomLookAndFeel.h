#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle, juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
        int, int, int, int, juce::ComboBox& box) override;

    // Add the declaration for our new function
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    juce::Font getLabelFont(juce::Label& label) override;

    // Helper function for drawing text labels for parameters
    void drawTextBoxedText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> bounds);

    juce::Colour moduleBgColour;
    juce::Colour emptySlotColour;
    juce::Colour accentColour;
    juce::Colour textColour;
};