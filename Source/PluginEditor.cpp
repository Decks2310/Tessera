// File: PluginEditor.cpp
//================================================================================
// File: PluginEditor.cpp
//================================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

// A namespace for layout constants to keep the code clean and easy to modify.
namespace LayoutConstants
{
    // UPDATED: Increased header height to 120 to allow the Response knob to be larger and square.
    constexpr int HEADER_HEIGHT = 120; // Was 100
    // UPDATED: Increased bottom strip height to accommodate the Master Mix label.
    constexpr int BOTTOM_STRIP_HEIGHT = 60; // Was 40
    constexpr int ADD_ROW_BUTTON_HEIGHT = 40;
    constexpr int DEFAULT_SLOT_ROW_HEIGHT = 250;
    // ✅ UPDATED: Increased height to give knobs more room
    constexpr int WIDE_SLOT_ROW_HEIGHT = 350;
    constexpr int SLOT_MARGIN = 5;
    // This now represents the width of the CENTRAL area (the grid).
    constexpr int PLUGIN_WIDTH = 840;
    constexpr int NUM_COLS = 4;
    // NEW: Width for the side faders.
    constexpr int FADER_WIDTH = 50;
}

//==============================================================================
ModularMultiFxAudioProcessorEditor::ModuleInfo ModularMultiFxAudioProcessorEditor::getModuleInfo(int choice)
{
    // Choice 7 is ChromaTape, which occupies 3 horizontal slots.
    if (choice == 7)
        return { choice, 3 };
    return { choice, 1 };
}

//==============================================================================
// UPDATED: Constructor
ModularMultiFxAudioProcessorEditor::ModularMultiFxAudioProcessorEditor(ModularMultiFxAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    // UPDATED: Initialize Faders and Knob
    inputGainFader(p.apvts, "INPUT_GAIN", "Input"),
    outputGainFader(p.apvts, "OUTPUT_GAIN", "Output"),
    responseTimeKnob(p.apvts, "SAG_RESPONSE", "Response")
{
    setLookAndFeel(&customLookAndFeel);
    // Listen for resize requests from the processor (e.g., when a module type changes)
    processorRef.editorResizeBroadcaster.addChangeListener(this);
    // NEW: Listen for OS Lock changes
    processorRef.osLockChangeBroadcaster.addChangeListener(this);

    // NEW: Set tooltip for Response Knob
    responseTimeKnob.setTooltip("Dictates how quickly the autogain applies volume compensation (ms). Lower values = fast response; higher values = slow response.");
    addAndMakeVisible(titleLabel);
    titleLabel.setText("TESSERA", juce::dontSendNotification);
    // JUCE 8 FIX (C4996): Use juce::FontOptions
    titleLabel.setFont(juce::FontOptions(24.0f));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, customLookAndFeel.textColour);
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("MULTIMODULAR FX AUDIO PLUGIN", juce::dontSendNotification);
    // JUCE 8 FIX (C4996): Use juce::FontOptions
    subtitleLabel.setFont(juce::FontOptions(12.0f));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setColour(juce::Label::textColourId, customLookAndFeel.textColour.withAlpha(0.6f));

    // NEW: OS Lock Warning Setup
    addAndMakeVisible(osLockWarningLabel);
    osLockWarningLabel.setText("ChromaTape Active: OS Rate locked to max 2x for stability. (Offline export uses Deluxe 8x).", juce::dontSendNotification);
    osLockWarningLabel.setJustificationType(juce::Justification::centredLeft);
    // Use a distinct color for the warning
    osLockWarningLabel.setColour(juce::Label::textColourId, juce::Colours::orange);

    // JUCE 8 FIX (C2664): Use .withItalic(true) instead of .withStyle(juce::Font::italic).
    osLockWarningLabel.setFont(juce::FontOptions(11.0f).withStyle("Italic"));

    osLockWarningLabel.setVisible(false); // Initially hidden

    // NEW: Oversampling Algorithm Setup
    addAndMakeVisible(oversamplingAlgoBox);
    if (auto* osAlgoParam = processorRef.apvts.getParameter("OVERSAMPLING_ALGO"))
    {
        oversamplingAlgoBox.addItemList(osAlgoParam->getAllValueStrings(), 1);
        oversamplingAlgoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "OVERSAMPLING_ALGO", oversamplingAlgoBox);
    }

    // NEW: Oversampling Rate Setup
    addAndMakeVisible(oversamplingRateBox);
    if (auto* osRateParam = processorRef.apvts.getParameter("OVERSAMPLING_RATE"))
    {
        oversamplingRateBox.addItemList(osRateParam->getAllValueStrings(), 1);
        oversamplingRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "OVERSAMPLING_RATE", oversamplingRateBox);
    }


    // Auto-Gain Setup
    addAndMakeVisible(autoGainButton);
    autoGainButton.setButtonText("Auto-Gain");
    // Ensure the attachment links to the correct parameter ID
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "SAG_ENABLE", autoGainButton);
    // UPDATED: Input/Output/Response Controls Setup (Self-managed attachments)
    addAndMakeVisible(inputGainFader);
    addAndMakeVisible(outputGainFader);
    addAndMakeVisible(responseTimeKnob);
    // Master Mix Setup
    addAndMakeVisible(masterMixLabel);
    masterMixLabel.setText("Master Mix", juce::dontSendNotification);
    masterMixLabel.setJustificationType(juce::Justification::centred);
    // Slightly dimmed text color for aesthetic
    masterMixLabel.setColour(juce::Label::textColourId, customLookAndFeel.textColour.withAlpha(0.7f));
    // JUCE 8 FIX (C4996): Use juce::FontOptions
    masterMixLabel.setFont(juce::FontOptions(14.0f));


    addAndMakeVisible(masterMixSlider);
    masterMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "MASTER_MIX", masterMixSlider);

    addAndMakeVisible(addRowButton);
    addRowButton.onClick = [this]
        {
            int currentSlots = processorRef.visibleSlotCount.getValue();
            if (currentSlots < processorRef.maxSlots)
            {
                // Add a new row of 4 slots, ensuring we don't exceed the maximum.
                int newSlots = std::min(processorRef.maxSlots, currentSlots + LayoutConstants::NUM_COLS);
                processorRef.visibleSlotCount = newSlots;
                updateSlotsAndResize();
            }
        };
    // Initial UI setup
    updateSlotsAndResize();
    // NEW: Initial state update for OS controls
    updateOversamplingControlsState();

}

// UPDATED: Destructor
ModularMultiFxAudioProcessorEditor::~ModularMultiFxAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    processorRef.editorResizeBroadcaster.removeChangeListener(this);
    // NEW: Remove listener
    processorRef.osLockChangeBroadcaster.removeChangeListener(this);
}

//==============================================================================
void ModularMultiFxAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(customLookAndFeel.findColour(juce::ResizableWindow::backgroundColourId));
}

//==============================================================================
// UPDATED: changeListenerCallback
void ModularMultiFxAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    // NEW: Handle OS Lock changes
    if (source == &processorRef.osLockChangeBroadcaster)
    {
        // This should already be on the message thread (called via callAsync from Processor).
        updateOversamplingControlsState();
        return;
    }

    if (source == &processorRef.editorResizeBroadcaster)
    {
        // ... (Existing resize logic) ...
        juce::Component::SafePointer<ModularMultiFxAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis] {
            if (auto* editor = safeThis.getComponent())
            {
                editor->updateSlotsAndResize();
                // NEW: Also update controls state as module selection affects the lock
                editor->updateOversamplingControlsState();
            }
            });
    }
}

// Helper function to get parameter index robustly (handles compatibility issues)
int getParameterIndexRobust(juce::RangedAudioParameter* param)
{
    if (!param) return 0;

    // Preferred method: Use dynamic_cast to access AudioParameterChoice::getIndex()
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
    {
        return choiceParam->getIndex();
    }

    // Fallback: Manual calculation from normalized value.
    int numSteps = param->getNumSteps();
    if (numSteps > 1)
    {
        float normalizedValue = param->getValue();
        // Calculate index = round(normalizedValue * (N-1)).
        // Adding 0.5f before casting implements rounding.
        return static_cast<int>(normalizedValue * (float)(numSteps - 1) + 0.5f);
    }
    return 0;
}


// NEW HELPER FUNCTION: Update UI elements based on the lock status
void ModularMultiFxAudioProcessorEditor::updateOversamplingControlsState()
{
    // === FIX C2039 (getIndex): Helper lambda using the robust index retrieval ===
    // This replaces the reliance on param->getIndex() directly on the base pointer.

    auto syncComboBoxToParameter = [&](juce::ComboBox& box, juce::RangedAudioParameter* param)
        {
            if (param)
            {
                // Use the compatible helper function.
                int index = getParameterIndexRobust(param);
                // Set the ComboBox ID (IDs start at 1)
                box.setSelectedId(index + 1, juce::dontSendNotification);
            }
        };
    // ==================================================================================================

    auto* osRateParam = processorRef.apvts.getParameter("OVERSAMPLING_RATE");
    auto* osAlgoParam = processorRef.apvts.getParameter("OVERSAMPLING_ALGO");

    // Do not lock the UI if we are rendering offline (the user should see the high-quality settings being used).
    if (processorRef.isNonRealtime())
    {
        oversamplingRateBox.setEnabled(true);
        osLockWarningLabel.setVisible(false);

        // Manually sync the UI to the APVTS parameters (replaces missing sendInitialUpdate).
        syncComboBoxToParameter(oversamplingRateBox, osRateParam);
        syncComboBoxToParameter(oversamplingAlgoBox, osAlgoParam);
        return;
    }

    bool isLocked = processorRef.isOversamplingLocked();

    // Disable the rate box if locked. Algorithm box remains enabled.
    oversamplingRateBox.setEnabled(!isLocked);

    // Show/Hide the warning label
    osLockWarningLabel.setVisible(isLocked);

    // Ensure the UI reflects the effective state when locked
    if (isLocked)
    {
        // If locked, the effective rate is capped at 2x. We need to visually reflect this
        // without modifying the underlying parameter (so the user's choice is restored later).
        if (osRateParam)
        {
            // Rate Indices: 0=1x, 1=2x, 2=4x, ...

            // FIX C2039 (getIndex): Use the compatible helper function.
            int currentIndex = getParameterIndexRobust(osRateParam);

            if (currentIndex > 1) // If the user selection (parameter) is > 2x
            {
                // Visually set the (disabled) combo box to 2x (Index 1). IDs start at 1.
                oversamplingRateBox.setSelectedId(1 + 1, juce::dontSendNotification);
            }
            else
            {
                // If the user selection is 1x or 2x, show that selection.
                oversamplingRateBox.setSelectedId(currentIndex + 1, juce::dontSendNotification);
            }
        }
    }
    else
    {
        // When unlocked, ensure the combo box reflects the actual underlying parameter value.
        // Manually sync the ComboBox (replaces missing sendInitialUpdate).
        syncComboBoxToParameter(oversamplingRateBox, osRateParam);
        // It's also good practice to sync the Algo box when unlocking, just in case.
        syncComboBoxToParameter(oversamplingAlgoBox, osAlgoParam);
    }
}


// UPDATED: Modified to calculate the new total width including faders.
void ModularMultiFxAudioProcessorEditor::updateSlotsAndResize()
{
    int slotsToShow = processorRef.visibleSlotCount.getValue();
    // === FIX: Optimized Slot Management ===
    // 1. Add new slots if the count increased
    if (moduleSlots.size() < (size_t)slotsToShow)
    {
        for (int i = (int)moduleSlots.size(); i < slotsToShow; ++i)
        {
            // ModuleSlot now initializes synchronously in its constructor.
            moduleSlots.push_back(std::make_unique<ModuleSlot>(processorRef.apvts, i));
            addAndMakeVisible(*moduleSlots.back());
        }
    }
    // 2. Remove excess slots if the count decreased
    else if (moduleSlots.size() > (size_t)slotsToShow)
    {
        // This deletes the excess components safely.
        moduleSlots.resize(slotsToShow);
    }
    // ======================================

    // --- Calculate new window height based on dynamic content ---
    int totalSlotAreaHeight = 0;
    int currentSlotIndex = 0;
    while (currentSlotIndex < slotsToShow)
    {
        bool isWideModuleInRow = false;
        // Check all slots that start in this row to see if any are ChromaTape
        for (int i = 0; i < LayoutConstants::NUM_COLS; ++i)
        {
            int slotToCheck = currentSlotIndex + i;
            if (slotToCheck < slotsToShow)
            {
                // ... (Logic for checking ChromaTape remains the same) ...
                auto* param = processorRef.apvts.getRawParameterValue("SLOT_" + juce::String(slotToCheck + 1) + "_CHOICE");
                if (param)
                {
                    // Warning C4244 (Line 215): Conversion from float to int.
                    // This static_cast is safe and necessary for AudioParameterChoice.
                    auto choiceVal = param->load();
                    if (getModuleInfo(static_cast<int>(choiceVal)).choice == 7) // 7 is ChromaTape
                    {
                        isWideModuleInRow = true;
                        break;
                    }
                }
            }
        }

        totalSlotAreaHeight += isWideModuleInRow ? LayoutConstants::WIDE_SLOT_ROW_HEIGHT : LayoutConstants::DEFAULT_SLOT_ROW_HEIGHT;
        currentSlotIndex += LayoutConstants::NUM_COLS; // Advance to the start of the next row
    }

    // Calculate total height (Uses the updated BOTTOM_STRIP_HEIGHT automatically)
    int totalHeight = LayoutConstants::HEADER_HEIGHT
        + totalSlotAreaHeight
        + LayoutConstants::ADD_ROW_BUTTON_HEIGHT
        + LayoutConstants::BOTTOM_STRIP_HEIGHT;
    // NEW: Calculate total width (Center width + 2 * Fader width)
    int totalWidth = LayoutConstants::PLUGIN_WIDTH + (LayoutConstants::FADER_WIDTH * 2);
    // Handle addRowButton visibility (remains the same)
    addRowButton.setVisible(slotsToShow < processorRef.maxSlots);
    if (!addRowButton.isVisible())
    {
        totalHeight -= LayoutConstants::ADD_ROW_BUTTON_HEIGHT;
    }

    // === FIX: Ensure Layout Refresh ===
    // Check if the size is actually changing.
    bool sizeChanged = (getWidth() != totalWidth || getHeight() != totalHeight);
    // Set the new size, which will trigger resized() IF the size changed.
    setSize(totalWidth, totalHeight);
    // If the size didn't change (e.g., modules swapped but height is the same), 
    // we must manually call resized() to force the layout update.
    if (!sizeChanged) {
        resized();
    }
}

// UPDATED: resized (Adjust layout for the warning label)
void ModularMultiFxAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // === FIX: Revised Layout Strategy ===

    // 1. Carve out the side columns
    auto leftFaderColumn = bounds.removeFromLeft(LayoutConstants::FADER_WIDTH);
    auto rightFaderColumn = bounds.removeFromRight(LayoutConstants::FADER_WIDTH);

    // 2. Header Layout (Central Column)
    auto headerArea = bounds.removeFromTop(LayoutConstants::HEADER_HEIGHT).reduced(10, 2);

    // Divide header into top (title/combos), middle (warning), and bottom (Response knob)
    // UPDATED: Adjust heights to fit the warning label.
    auto headerTop = headerArea.removeFromTop(40);
    // NEW: Add space for the warning label (15px height)
    auto headerWarning = headerArea.removeFromTop(15);
    auto headerBottom = headerArea; // Response knob takes the rest

    // Top Row Layout
    auto osArea = headerTop.removeFromLeft(250).reduced(0, 8);
    oversamplingAlgoBox.setBounds(osArea.removeFromLeft(140));
    osArea.removeFromLeft(10);
    oversamplingRateBox.setBounds(osArea);

    // NEW: Layout the warning label below the OS boxes area.
    // We give it a wider area to fit the text.
    osLockWarningLabel.setBounds(headerWarning.removeFromLeft(550));

    // ... (Auto-Gain, Title/Subtitle layout) ...
    autoGainButton.setBounds(headerTop.removeFromRight(120).reduced(0, 8));
    auto titleBounds = headerTop;
    titleLabel.setBounds(titleBounds.removeFromTop(24));
    subtitleLabel.setBounds(titleBounds);

    // Bottom Row Layout (Response Knob Centered)
    // (FlexBox layout for responseTimeKnob remains the same, using the slightly reduced headerBottom height)
    juce::FlexBox knobFb;
    knobFb.justifyContent = juce::FlexBox::JustifyContent::center;
    knobFb.alignItems = juce::FlexBox::AlignItems::center;
    // Ensure the knob size adapts to the slightly reduced available height.
    float knobComponentSize = (float)headerBottom.getHeight();
    knobFb.items.add(juce::FlexItem(responseTimeKnob).withWidth(knobComponentSize).withHeight(knobComponentSize));
    knobFb.performLayout(headerBottom);


    // 3. Footer Layout (Central Column)
    auto bottomStrip = bounds.removeFromBottom(LayoutConstants::BOTTOM_STRIP_HEIGHT).reduced(20, 5);
    masterMixLabel.setBounds(bottomStrip.removeFromTop(20)); // Label on top
    masterMixSlider.setBounds(bottomStrip); // Slider below the label

    // 4. Add Row Button (Central Column)
    if (addRowButton.isVisible())
    {
        auto addRowArea = bounds.removeFromBottom(LayoutConstants::ADD_ROW_BUTTON_HEIGHT);
        addRowButton.setBounds(addRowArea.withSizeKeepingCentre(40, 30));
    }

    // 'bounds' now represents the main Slot Grid Area.
    // 5. Layout Faders (Aligning vertically with the Slot Grid Area)
    int contentStartY = bounds.getY();
    int contentHeight = bounds.getHeight();

    // Define the area for the fader components within their columns, aligned with the grid.
    // Apply horizontal padding (5px) and vertical padding (10px) relative to the grid area.
    auto leftFaderArea = leftFaderColumn
        .withY(contentStartY)
        .withHeight(contentHeight)
        .reduced(5, 10);
    auto rightFaderArea = rightFaderColumn
        .withY(contentStartY)
        .withHeight(contentHeight)
        .reduced(5, 10);
    inputGainFader.setBounds(leftFaderArea);
    outputGainFader.setBounds(rightFaderArea);

    // === END FIX ===


    // 6. Slot Grid Layout (Central Area)

    // FIX: Use the full central 'bounds' for the slotArea. Margins will be handled per slot.
    // This prevents double margins on the edges and ensures correct alignment with faders.
    // auto slotArea = bounds.reduced(LayoutConstants::SLOT_MARGIN); // OLD
    auto slotArea = bounds; // NEW


    const int numVisibleSlots = processorRef.visibleSlotCount.getValue();
    if (numVisibleSlots == 0 || moduleSlots.empty()) return;

    // Hide all slot components initially.
    for (auto& slot : moduleSlots)
    {
        slot->setVisible(false);
    }

    int currentSlotIndex = 0;
    while (currentSlotIndex < numVisibleSlots)
    {
        // 1. Determine the height for the current row
        bool isWideModuleInRow = false;
        for (int i = 0; i < LayoutConstants::NUM_COLS; ++i)
        {
            int slotToCheck = currentSlotIndex + i;
            if (slotToCheck < numVisibleSlots)
            {
                // ✅ FIX: Added a null check here as well for robustness.
                auto* param = processorRef.apvts.getRawParameterValue("SLOT_" + juce::String(slotToCheck + 1) + "_CHOICE");
                if (param)
                {
                    // Warning C4244: Safe conversion float->int for parameter choice.
                    auto choiceVal = param->load();
                    if (getModuleInfo(static_cast<int>(choiceVal)).choice == 7) // ChromaTape
                    {
                        isWideModuleInRow = true;
                        break;
                    }
                }
            }
        }
        const int currentRowHeight = isWideModuleInRow ? LayoutConstants::WIDE_SLOT_ROW_HEIGHT : LayoutConstants::DEFAULT_SLOT_ROW_HEIGHT;
        auto rowBounds = slotArea.removeFromTop(currentRowHeight);

        // 2. Lay out the modules within this row's bounds
        const float slotWidth = (float)rowBounds.getWidth() / LayoutConstants::NUM_COLS;
        int col = 0;
        while (col < LayoutConstants::NUM_COLS && currentSlotIndex < numVisibleSlots)
        {
            int choice = 0;
            // ✅ FIX: Added a null check for safety during layout.
            auto* param = processorRef.apvts.getRawParameterValue("SLOT_" + juce::String(currentSlotIndex + 1) + "_CHOICE");
            if (param)
            {
                // Warning C4244: Safe conversion float->int for parameter choice.
                choice = static_cast<int>(param->load());
            }
            auto info = getModuleInfo(choice);
            int slotsToUse = info.slotsUsed;

            // Constrain span to prevent wrapping past the end of the row
            if (col + slotsToUse > LayoutConstants::NUM_COLS)
                slotsToUse = LayoutConstants::NUM_COLS - col;

            // Constrain span to prevent going past the total number of visible slots
            if (currentSlotIndex + slotsToUse > numVisibleSlots)
                slotsToUse = numVisibleSlots - currentSlotIndex;

            // Calculate the bounds for this module based on how many slots it uses

            // === FIX for Alignment Issue ===
            // We calculate the absolute X coordinates. Using std::round improves precision for float-based grids.
            int startX = rowBounds.getX() + (int)std::round(col * slotWidth);
            int endX = rowBounds.getX() + (int)std::round((col + slotsToUse) * slotWidth);
            int width = endX - startX;

            // Create the rectangle using the calculated absolute coordinates.
            auto slotBounds = juce::Rectangle<int>(startX, rowBounds.getY(), width, rowBounds.getHeight())
                .reduced(LayoutConstants::SLOT_MARGIN);
            // Set the bounds for the primary slot component and make it visible
            if (currentSlotIndex < moduleSlots.size())
            {
                moduleSlots[currentSlotIndex]->setBounds(slotBounds);
                moduleSlots[currentSlotIndex]->setVisible(true);
            }

            // Advance our column and slot index counters by the number of slots consumed
            col += slotsToUse;
            currentSlotIndex += slotsToUse;
        }
    }
}