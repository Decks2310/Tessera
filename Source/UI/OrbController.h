#pragma once
#include <JuceHeader.h>
#include "UIConfig.h"

class OrbController : public juce::Component, public juce::Slider::Listener, public juce::Timer
{
public:
    // Public hidden sliders for APVTS attachment
    juce::Slider tuneSlider; // Vertical
    juce::Slider mixSlider;  // Horizontal

    OrbController()
    {
        configureSlider(tuneSlider);
        configureSlider(mixSlider);
        startTimerHz(60); // For animation
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto center = bounds.getCentre();

        // Visualization: Mix level controls brightness and pulsation
        float mixLevel = currentMix;
        float pulsation = (0.05f * mixLevel) * std::sin(phase);

        float baseRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.3f;
        float radius = baseRadius * (1.0f + pulsation);

        // Glow Effect
        juce::ColourGradient gradient(UIConfig::PRIMARY_COLOUR.withAlpha(juce::jlimit(0.2f, 0.8f, mixLevel * 1.2f)), center.x, center.y,
            UIConfig::PRIMARY_COLOUR.withAlpha(0.0f), center.x, center.y + radius * 1.5f, true);

        g.setGradientFill(gradient);
        g.fillEllipse(center.x - radius * 1.5f, center.y - radius * 1.5f, radius * 3.0f, radius * 3.0f);

        // The Orb Outline
        g.setColour(UIConfig::PRIMARY_COLOUR.withAlpha(0.9f));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // Interaction Hint
        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        g.drawText("Drag: Tune (Y) / Mix (X)", bounds.removeFromBottom(20), juce::Justification::centred, false);
    }

    // --- Interaction Logic (Relative Dragging) ---
    void mouseDown(const juce::MouseEvent& event) override
    {
        isDragging = true;
        // Store the values when the drag starts
        startDragValue.x = (float)mixSlider.getValue();
        startDragValue.y = (float)tuneSlider.getValue();
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        isDragging = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!isDragging) return;

        // Calculate the total offset since the drag started
        auto delta = event.getOffsetFromDragStart();

        // Sensitivity: 200 pixels drag corresponds to a full parameter sweep (0 to 1).
        float sensitivity = 1.0f / 200.0f;

        float deltaX = delta.x * sensitivity;
        float deltaY = -delta.y * sensitivity; // Invert Y axis (drag up = higher pitch)

        // Calculate new values based on the STARTING values (crucial for correct relative drag)
        float newMix = juce::jlimit(0.0f, 1.0f, startDragValue.x + deltaX);
        float newTune = juce::jlimit(0.0f, 1.0f, startDragValue.y + deltaY);

        // Update the hidden sliders, which update the VST parameters via attachments
        mixSlider.setValue(newMix, juce::sendNotificationSync);
        tuneSlider.setValue(newTune, juce::sendNotificationSync);
    }

    // --- Visualization Updates ---
    void sliderValueChanged(juce::Slider* slider) override
    {
        // Update local values when parameters change (e.g., via automation or user drag)
        if (slider == &mixSlider)
            currentMix = (float)mixSlider.getValue();

        // We only repaint if the mix changed, as tune doesn't affect the visualization here
        if (slider == &mixSlider)
            repaint();
    }

    void timerCallback() override
    {
        // Update animation phase
        phase += 0.08f;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }
private:
    float currentMix = 0.5f;
    float phase = 0.0f;
    juce::Point<float> startDragValue;
    bool isDragging = false;

    void configureSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setRange(0.0, 1.0); // Assuming normalized parameters
        slider.addListener(this);
        addAndMakeVisible(slider);
        slider.setVisible(false); // Hide the actual slider visualization
    }
};