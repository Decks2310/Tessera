#include "ModuleSelectionGrid.h"
#include "EmbeddedSVGs.h" // Include the new SVG data

//==============================================================================
// ModuleGridButton Implementation
//==============================================================================

ModuleGridButton::ModuleGridButton(const juce::String& text, const char* svgData)
    : juce::Button(text)
{
    // Load the SVG data from the embedded string.
    if (svgData != nullptr)
    {
        // FIXED: Simply construct the juce::String from the const char* as it is null-terminated.
        juce::String svgString(svgData);

        // Use parseXML (free function) to get the XmlElement
        if (auto svgElement = juce::parseXML(svgString))
        {
            // Create the Drawable from the XmlElement
            svgImage = juce::Drawable::createFromSVG(*svgElement);
        }
    }
    // Ensure the button doesn't toggle state, it's just for selection
    setClickingTogglesState(false);
}

// CLEANED: Removed invisible characters (0xa0) that caused C3872 errors.
void ModuleGridButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    // FIX: Retrieve the active LookAndFeel and cast it to access custom colors.
    auto* lnf = dynamic_cast<CustomLookAndFeel*>(&getLookAndFeel());

    // Define Colors based on the active LAF, providing fallbacks if the cast fails.
    const juce::Colour defaultBackground = (lnf != nullptr) ? lnf->moduleBgColour : juce::Colour(0xff3a3a3a);
    const juce::Colour highlightColour = (lnf != nullptr) ?
        lnf->accentColour : juce::Colour(0xfff0c419);
    const juce::Colour defaultIconColour = (lnf != nullptr) ? lnf->textColour : juce::Colours::white;
    const juce::Colour highlightIconColour = (lnf != nullptr) ? lnf->emptySlotColour : juce::Colour(0xff2d2d2d);

    bool isInteracting = shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown;
    juce::Colour effectiveBackground;
    juce::Colour effectiveIconColour;

    // Set colors based on state
    if (isInteracting)
    {
        effectiveBackground = highlightColour;
        effectiveIconColour = highlightIconColour;
    }
    else
    {
        effectiveBackground = defaultBackground;
        effectiveIconColour = defaultIconColour;
    }

    // 1. Draw Background
    g.setColour(effectiveBackground);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
    // 2. Define Areas for Icon and Text
    auto bounds = getLocalBounds().reduced(5);
    auto textArea = bounds.removeFromBottom(20);
    auto iconArea = bounds.reduced(5); // Padding for the icon

    // 3. Draw Icon
    if (svgImage != nullptr)
    {
        // CRITICAL: Correctly handle dynamic recoloring for persistent Drawables.
        // Step A: Replace the base color (black) with the effective color.
        svgImage->replaceColour(juce::Colours::black, effectiveIconColour);

        // Step B: Draw the SVG.
        svgImage->drawWithin(g, iconArea.toFloat(),
            juce::RectanglePlacement::centred, 1.0f);
        // Step C: Revert the color back to the base color (black) so it's ready for the next paint cycle.
        svgImage->replaceColour(effectiveIconColour, juce::Colours::black);
    }

    // 4. Draw Text
    g.setColour(effectiveIconColour);
    g.setFont(13.0f);
    g.drawText(getButtonText(), textArea, juce::Justification::centred);
}

//==============================================================================
// ModuleSelectionGrid Implementation
//==============================================================================

ModuleSelectionGrid::ModuleSelectionGrid(juce::StringArray choices)
{
    // Ensure the grid uses the custom LAF for background color
    setLookAndFeel(&lookAndFeel);
    for (int i = 0; i < choices.size(); ++i)
    {
        auto* button = new ModuleGridButton(choices[i], getSvgDataForChoice(i + 1));
        button->onClick = [this, i]
            {
                // Dismiss the CallOutBox when a selection is made
                if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
                    callout->dismiss();
                if (onModuleSelected)
                    onModuleSelected(i + 1);
            };
        buttons.add(button);
        addAndMakeVisible(button);
    }
}

ModuleSelectionGrid::~ModuleSelectionGrid()
{
    setLookAndFeel(nullptr);
}

void ModuleSelectionGrid::paint(juce::Graphics& g) {
    // Set the background color for the entire grid area (the popup background)
    // Use the darkest color (emptySlotColour or ResizableWindow::backgroundColourId)
    g.fillAll(lookAndFeel.emptySlotColour);
}

// UPDATED: 4 columns x 4 rows layout for 13 modules
void ModuleSelectionGrid::resized() {
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    // UPDATED: 4 columns x 4 rows layout to accommodate 13 modules.
    grid.templateColumns = { Track(Fr(1)), Track(Fr(1)), Track(Fr(1)), Track(Fr(1)) };
    grid.templateRows = { Track(Fr(1)), Track(Fr(1)), Track(Fr(1)), Track(Fr(1)) };
    // Add spacing
    grid.setGap(juce::Grid::Px(8));

    for (auto* b : buttons)
        grid.items.add(juce::GridItem(b));
    // Perform layout with padding
    grid.performLayout(getLocalBounds().reduced(10));
}

const char* ModuleSelectionGrid::getSvgDataForChoice(int choice) {
    switch (choice)
    {
    case 1: return EmbeddedSVGs::distortionData;
    case 2: return EmbeddedSVGs::filterData;
    case 3: return EmbeddedSVGs::modulationData;
    case 4: return EmbeddedSVGs::delayData;
    case 5: return EmbeddedSVGs::reverbData;
    case 6: return EmbeddedSVGs::compressorData;
    case 7: return EmbeddedSVGs::chromaTapeData;
    case 8: return EmbeddedSVGs::morphoCompData;
    case 9: return EmbeddedSVGs::diceData; // For Physical Resonator
    case 10: return EmbeddedSVGs::spectralAnimatorData;
    case 11: return EmbeddedSVGs::helicalDelayData;
    case 12: return EmbeddedSVGs::chronoVerbData;
    case 13: return EmbeddedSVGs::tectonicDelayData;
    default: return nullptr;
    }
}