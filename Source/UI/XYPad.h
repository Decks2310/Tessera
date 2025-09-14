// File: XYPad.h
#pragma once
#include <JuceHeader.h>
// Assuming UIConfig.h exists and defines UIConfig::PRIMARY_COLOUR
#include "UIConfig.h"

class XYPad : public juce::Component, public juce::Slider::Listener
{
public:
    // Public hidden sliders required for APVTS attachment
    juce::Slider xSlider;
    juce::Slider ySlider;

    XYPad()
    {
        configureSlider(xSlider);
        configureSlider(ySlider);
        // Initialize internal state using public API
        normX = (float)xSlider.valueToProportionOfLength(xSlider.getValue());
        normY = (float)ySlider.valueToProportionOfLength(ySlider.getValue());
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getPaddedBounds();

        // Define colors
        juce::Colour primaryColour = juce::Colours::yellow;
        juce::Colour secondaryColour = juce::Colours::grey;
#ifdef UIConfig_h
        primaryColour = UIConfig::PRIMARY_COLOUR;
#endif

        // Background grid visualization
        g.setColour(secondaryColour.darker(0.5f));
        g.drawRect(bounds, 2.0f);

        g.setColour(secondaryColour.withAlpha(0.3f));
        g.drawLine(bounds.getCentreX(), bounds.getY(), bounds.getCentreX(), bounds.getBottom());
        g.drawLine(bounds.getX(), bounds.getCentreY(), bounds.getRight(), bounds.getCentreY());

        // The Dot (Puck)
        float dotRadius = 8.0f;
        // Calculate position based on normalized values.
        float posX = bounds.getX() + normX * bounds.getWidth();
        // Y axis is inverted in graphics (0.0 is top), but parameters assume 0.0 is bottom.
        float posY = bounds.getY() + (1.0f - normY) * bounds.getHeight();

        // Use the primary colour (yellow) for the dot
        g.setColour(primaryColour);
        g.fillEllipse(posX - dotRadius, posY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }

    // --- Interaction Logic (Using Public API) ---
    void mouseDown(const juce::MouseEvent& event) override
    {
        // FIX: Use startedDragging() for automation gestures.
        xSlider.startedDragging();
        ySlider.startedDragging();
        // Immediately update position to the click location
        mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        // FIX: Use stoppedDragging() for automation gestures.
        xSlider.stoppedDragging();
        ySlider.stoppedDragging();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        auto bounds = getPaddedBounds();
        if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;

        // Calculate normalized position based on mouse coordinates relative to the padded bounds
        float newNormX = (event.position.x - bounds.getX()) / bounds.getWidth();
        // Invert Y axis calculation (Graphics Y=0 is top, Parameter Y=0 is bottom)
        float newNormY = 1.0f - ((event.position.y - bounds.getY()) / bounds.getHeight());

        // Clamp values
        newNormX = juce::jlimit(0.0f, 1.0f, newNormX);
        newNormY = juce::jlimit(0.0f, 1.0f, newNormY);

        // FIX: Update sliders by converting normalized values back to actual values.
        double newXValue = xSlider.proportionOfLengthToValue(newNormX);
        double newYValue = ySlider.proportionOfLengthToValue(newNormY);

        // Set the value. sendNotification is crucial.
        xSlider.setValue(newXValue, juce::sendNotification);
        ySlider.setValue(newYValue, juce::sendNotification);
    }

    void sliderValueChanged(juce::Slider* slider) override
    {
        // Update internal normalized values when parameters change
        // FIX: Use public API for normalization.
        if (slider == &xSlider)
            normX = (float)xSlider.valueToProportionOfLength(xSlider.getValue());
        else if (slider == &ySlider)
            normY = (float)ySlider.valueToProportionOfLength(ySlider.getValue());

        // CRUCIAL: Repaint the component to move the dot visually
        repaint();
    }

private:
    // Internal tracking of normalized positions
    float normX = 0.5f;
    float normY = 0.5f;
    // Add padding so the dot isn't clipped at the edges
    const float padding = 5.0f;

    juce::Rectangle<float> getPaddedBounds() const
    {
        return getLocalBounds().toFloat().reduced(padding);
    }

    void configureSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        // DO NOT set range manually. Let the attachment handle it.
        slider.addListener(this);
        addAndMakeVisible(slider);
        slider.setVisible(false);
    }
};