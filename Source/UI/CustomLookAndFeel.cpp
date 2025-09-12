// File: UI/CustomLookAndFeel.cpp
#include "CustomLookAndFeel.h"

CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff2d2d2d));
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
    setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);

    moduleBgColour = juce::Colour(0xff3a3a3a);
    emptySlotColour = juce::Colour(0xff2d2d2d);
    accentColour = juce::Colour(0xfff0c419);
    textColour = juce::Colours::white;
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(10);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = 2.0f;
    auto arcRadius = radius - lineW;

    g.setColour(moduleBgColour.brighter(0.2f));
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    if (slider.isEnabled())
    {
        g.setColour(accentColour);
        juce::Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    juce::Point<float> thumbPoint = bounds.getCentre().getPointOnCircumference(radius * 0.7f, toAngle);
    g.setColour(textColour);
    g.drawLine({ bounds.getCentre(), thumbPoint }, 2.0f);
}

// UPDATED: Overhauled for a modern, clean look, including Bipolar visualization, corrected geometry, and thumb outline.
void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // FIXED: Address C4100 warnings by ignoring unused parameters.
    juce::ignoreUnused(minSliderPos);
    juce::ignoreUnused(maxSliderPos);

    // Check for standard linear styles (Horizontal/Vertical)
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearVertical)
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();

        // 1. Determine Thumb Size FIRST (Crucial for track calculation)
        // FIX: Use a consistent thumb size (16.0f) for both vertical and horizontal sliders
        // to match the appearance of the Master Mix slider.
        // auto thumbDiameter = (style == juce::Slider::LinearVertical) ? 12.0f : 16.0f; // OLD
        auto thumbDiameter = 16.0f; // NEW
        auto thumbRadius = thumbDiameter / 2.0f;

        // 2. Define the track appearance
        // FIX: Increased thickness to match the Master Mix slider visual weight.
        // float trackThickness = 4.0f; // OLD
        float trackThickness = 8.0f; // NEW
        float cornerRadius = trackThickness / 2.0f;

        // 3. Calculate Track Bounds (THE FIX)
        // We reduce the bounds by the thumb radius at the ends so the track visualization aligns with sliderPos.
        juce::Rectangle<float> trackBounds;

        if (style == juce::Slider::LinearHorizontal)
        {
            // Horizontal: Reduce the width by the thumb radius on both sides
            trackBounds = bounds.reduced(thumbRadius, 0)
                .withHeight(trackThickness)
                .withCentre(bounds.getCentre());
        }
        else
        {
            // Vertical: Reduce the height by the thumb radius on both top and bottom
            trackBounds = bounds.reduced(0, thumbRadius)
                .withWidth(trackThickness)
                .withCentre(bounds.getCentre());
        }


        // 4. Draw the background track
        g.setColour(moduleBgColour.brighter(0.3f)); // Slightly brighter than module background
        g.fillRoundedRectangle(trackBounds, cornerRadius);

        // 5. Draw the value track (the filled portion)
        juce::Rectangle<float> valueTrackBounds = trackBounds;

        // Use effectiveSliderPos clamped to the visual track bounds for drawing the fill
        float effectiveSliderPos;

        if (style == juce::Slider::LinearHorizontal)
        {
            effectiveSliderPos = juce::jlimit(trackBounds.getX(), trackBounds.getRight(), sliderPos);
            // Standard horizontal fill (Master Mix)
            valueTrackBounds = valueTrackBounds.withWidth(effectiveSliderPos - trackBounds.getX());
        }
        else // Vertical
        {
            effectiveSliderPos = juce::jlimit(trackBounds.getY(), trackBounds.getBottom(), sliderPos);

            // Bipolar visualization for vertical sliders (Input/Output Gain)
            // This logic is kept as it correctly visualizes gain from the center 0dB point.
            if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
            {
                // Calculate the zero position based on OUR track geometry.
                const float proportion = (float)slider.valueToProportionOfLength(0.0);
                // Note: In JUCE vertical sliders (default), proportion 1.0 is at the TOP, 0.0 is at the BOTTOM.
                float zeroPos = trackBounds.getBottom() - trackBounds.getHeight() * proportion;

                // Clamp zeroPos within track bounds just in case of precision issues
                zeroPos = juce::jlimit(trackBounds.getY(), trackBounds.getBottom(), zeroPos);

                if (slider.getValue() >= 0.0)
                {
                    // Positive value: Fill from zeroPos UPWARDS to effectiveSliderPos
                    valueTrackBounds = trackBounds.withTop(effectiveSliderPos).withHeight(zeroPos - effectiveSliderPos);
                }
                else
                {
                    // Negative value: Fill from zeroPos DOWNWARDS to effectiveSliderPos
                    valueTrackBounds = trackBounds.withTop(zeroPos).withHeight(effectiveSliderPos - zeroPos);
                }
            }
            else
            {
                // Standard visualization (fill from bottom up)
                valueTrackBounds = valueTrackBounds.withTop(effectiveSliderPos).withHeight(trackBounds.getBottom() - effectiveSliderPos);
            }
        }

        g.setColour(accentColour);
        g.fillRoundedRectangle(valueTrackBounds, cornerRadius);

        // 6. Draw the Thumb
        juce::Rectangle<float> thumbBounds(thumbDiameter, thumbDiameter);

        // The input sliderPos is the correct center point for the thumb.
        if (style == juce::Slider::LinearHorizontal)
        {
            thumbBounds.setCentre({ sliderPos, trackBounds.getCentreY() });
        }
        else
        {
            thumbBounds.setCentre({ trackBounds.getCentreX(), sliderPos });
        }

        g.setColour(accentColour);
        g.fillEllipse(thumbBounds);

        // FIX: Add an outline to the thumb (Matches Picture 2).
        g.setColour(emptySlotColour); // Use the darkest color for outline contrast
        g.drawEllipse(thumbBounds.reduced(1.0f), 1.5f);
    }
    // Keep the existing logic for Bar style if needed
    // FIX: Replace deprecated slider.isBar()
    // else if (slider.isBar()) // OLD
    else if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical) // NEW
    {
        // (Existing bar implementation remains)
        g.setColour(moduleBgColour.brighter(0.2f));
        g.fillRect(slider.getLocalBounds().toFloat());

        g.setColour(accentColour);
        const float value = (float)slider.valueToProportionOfLength(slider.getValue());
        if (value > 0)
            g.fillRect(slider.getLocalBounds().withWidth((int)((float)slider.getWidth() * value)).toFloat());
    }
    else
    {
        // (Fallback implementation remains the same)
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(0.5f);
        g.setColour(moduleBgColour.brighter(0.2f));
        g.fillRoundedRectangle(bounds, 5.0f);

        auto thumbWidth = 10.0f;
        auto thumbBounds = juce::Rectangle<float>(thumbWidth, (float)height).withCentre(bounds.getCentre());
        thumbBounds.setX(sliderPos - (thumbWidth / 2.0f));

        g.setColour(accentColour);
        g.fillRoundedRectangle(thumbBounds, 5.0f);
    }
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(button.getToggleState() ? accentColour : moduleBgColour.brighter(0.2f));
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(textColour);
    g.drawFittedText(button.getButtonText(), bounds.toNearestInt(), juce::Justification::centred, 1);
}

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
    int, int, int, int, juce::ComboBox& box)
{
    juce::ignoreUnused(box);
    auto cornerSize = 5.0f;
    juce::Rectangle<int> boxBounds(0, 0, width, height);

    g.setColour(moduleBgColour.brighter(0.2f));
    g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);
}

void CustomLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(box.getLocalBounds().reduced(5, 0));
    label.setFont(juce::FontOptions((float)box.getHeight() * 0.7f));
    label.setColour(juce::Label::textColourId, textColour);
    label.setJustificationType(juce::Justification::centredLeft);
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions().withHeight(14.0f));
}

void CustomLookAndFeel::drawTextBoxedText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> bounds)
{
    g.setColour(moduleBgColour.brighter(0.2f));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    g.setColour(textColour.withAlpha(0.5f));
    g.drawRect(bounds.toFloat(), 1.0f);

    g.setColour(textColour);
    g.drawFittedText(text, bounds, juce::Justification::centred, 1);
}