#pragma once
#include <JuceHeader.h>
#include "UIConfig.h"

class XYPad : public juce::Component, public juce::Slider::Listener
{
public:
    // Public hidden sliders for APVTS attachment
    juce::Slider xSlider; // e.g., Excite Tz
    juce::Slider ySlider; // e.g., Sensitivity

    XYPad()
    {
        configureSlider(xSlider);
        configureSlider(ySlider);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        // Get current values from sliders
        float xVal = (float)xSlider.getValue();
        float yVal = (float)ySlider.getValue();

        // Calculate thumb position
        float xPos = bounds.getX() + bounds.getWidth() * xVal;
        float yPos = bounds.getY() + bounds.getHeight() * (1.0f - yVal); // Invert Y

        // Draw crosshair
        g.setColour(juce::Colours::darkgrey);
        g.drawLine(xPos, bounds.getY(), xPos, bounds.getBottom(), 1.0f);
        g.drawLine(bounds.getX(), yPos, bounds.getRight(), yPos, 1.0f);

        // Draw Thumb
        g.setColour(UIConfig::PRIMARY_COLOUR);
        g.fillEllipse(xPos - 6.0f, yPos - 6.0f, 12.0f, 12.0f);
    }

    // --- Interaction Logic (Absolute Positioning) ---
    void updatePosition(const juce::MouseEvent& event)
    {
        auto bounds = getLocalBounds().toFloat();

        // Calculate normalized values based on mouse position within the pad
        float newX = juce::jlimit(0.0f, 1.0f, (event.position.x - bounds.getX()) / bounds.getWidth());
        float newY = juce::jlimit(0.0f, 1.0f, 1.0f - ((event.position.y - bounds.getY()) / bounds.getHeight()));

        // Update the hidden sliders
        xSlider.setValue(newX, juce::sendNotificationSync);
        ySlider.setValue(newY, juce::sendNotificationSync);
    }

    void mouseDown(const juce::MouseEvent& event) override { updatePosition(event); }
    void mouseDrag(const juce::MouseEvent& event) override { updatePosition(event); }

    // --- Visualization Updates ---
    void sliderValueChanged(juce::Slider* slider) override
    {
        // Called when sliders change (user interaction or host automation). 
        // Ensures the visualization is always synchronized.
        repaint();
    }
private:
    void configureSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setRange(0.0, 1.0);
        slider.addListener(this);
        addAndMakeVisible(slider);
        slider.setVisible(false); // Hide the actual slider visualization
    }
};