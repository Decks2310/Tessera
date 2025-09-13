// File: UI/ModuleSlot.cpp
#include "ModuleSlot.h"
#include "ModuleHeader.h"
#include "ModuleSelectionGrid.h"
#include "SlotEditors.h"
// NEW: Explicitly include the new editor definition
#include "PhysicalResonatorSlotEditor.h"

ModuleSlot::ModuleSlot(juce::AudioProcessorValueTreeState& apvts, int slotIndex)
    : valueTreeState(apvts), index(slotIndex)
{
    setLookAndFeel(&lookAndFeel);
    slotChoiceParamId = "SLOT_" + juce::String(index + 1) + "_CHOICE";
    slotPrefix = "SLOT_" + juce::String(index + 1) + "_";
    header = std::make_unique<ModuleHeader>();
    addAndMakeVisible(*header);
    header->onMenuClicked = [this] { showModuleMenu(); };
    header->getSlotIndex = [this] { return index; };
    header->onDeleteClicked = [this]
        {
            auto* param = valueTreeState.getParameter(slotChoiceParamId);
            param->setValueNotifyingHost(0.0f);
        };
    header->onSlotMoved = [this](int sourceSlot, int targetSlot)
        {
            auto sourceParamId = "SLOT_" + juce::String(sourceSlot + 1) + "_CHOICE";
            auto targetParamId = "SLOT_" + juce::String(targetSlot + 1) + "_CHOICE";

            auto* sourceParam = valueTreeState.getParameter(sourceParamId);
            auto* targetParam = valueTreeState.getParameter(targetParamId);
            if (sourceParam && targetParam)
            {
                float sourceVal = sourceParam->getValue();
                float targetVal = targetParam->getValue();
                sourceParam->setValueNotifyingHost(targetVal);
                targetParam->setValueNotifyingHost(sourceVal);
            }
        };

    addAndMakeVisible(addModuleButton);
    addModuleButton.onClick = [this] { showModuleMenu(); };

    // === FIX: Initialize Synchronously ===
    // We must initialize the module content synchronously before adding the listener.
    if (auto* paramValue = valueTreeState.getRawParameterValue(slotChoiceParamId))
    {
        // Call createModule directly.
        createModule(static_cast<int>(paramValue->load()));
    }
    else
    {
        createModule(0);
        // Safety fallback
    }

    // Now register the listener for future changes.
    valueTreeState.addParameterListener(slotChoiceParamId, this);
}

ModuleSlot::~ModuleSlot()
{
    setLookAndFeel(nullptr);
    valueTreeState.removeParameterListener(slotChoiceParamId, this);
}

void ModuleSlot::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.setColour(lookAndFeel.moduleBgColour);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    if (currentEditor == nullptr)
    {
        g.setColour(lookAndFeel.emptySlotColour);
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
        g.setColour(lookAndFeel.emptySlotColour.brighter(0.1f));
        for (int i = -bounds.getHeight(); i < bounds.getWidth(); i += 15)
        {
            g.drawLine((float)i, (float)bounds.getBottom(), (float)i + (float)bounds.getHeight(), (float)bounds.getY(), 2.0f);
        }
    }
}

void ModuleSlot::resized() {
    auto bounds = getLocalBounds();
    header->setBounds(bounds.removeFromTop(30));
    if (currentEditor != nullptr)
        currentEditor->setBounds(bounds);
    else
        addModuleButton.setBounds(bounds.withSizeKeepingCentre(40, 40));
}

// === FIX: Implement Hybrid Sync/Async Update ===
void ModuleSlot::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == slotChoiceParamId)
    {
        // If we are on the message thread (e.g., user clicked the UI), update immediately (synchronously).
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            createModule((int)newValue);
        }
        else
        {
            // If called from another thread (e.g., automation), schedule it safely (asynchronously).
            // Use SafePointer in case the slot is deleted (e.g., row removed) before the async call runs.
            juce::Component::SafePointer<ModuleSlot> safeThis(this);
            int choice = (int)newValue;

            juce::MessageManager::callAsync([safeThis, choice] {
                if (ModuleSlot* slot = safeThis.getComponent()) {
                    slot->createModule(choice);
                }
                });
        }
    }
}

void ModuleSlot::createModule(int choice) {
    currentEditor.reset();
    if (choice == 0)
    {
        header->setVisible(false);
        addAndMakeVisible(addModuleButton);
    }
    else
    {
        header->setVisible(true);
        currentEditor = createEditorForChoice(choice);
        if (currentEditor)
        {
            header->title.setText(getModuleName(choice), juce::dontSendNotification);
            removeChildComponent(&addModuleButton);
            addAndMakeVisible(*currentEditor);
            resized();
        }
    }
    repaint();
}

std::unique_ptr<juce::Component> ModuleSlot::createEditorForChoice(int choice) {
    switch (choice)
    {
    case 1: return std::make_unique<DistortionSlotEditor>(valueTreeState, slotPrefix);
    case 2: return std::make_unique<FilterSlotEditor>(valueTreeState, slotPrefix);
    case 3: return std::make_unique<ModulationSlotEditor>(valueTreeState, slotPrefix);
    case 4: return std::make_unique<AdvancedDelaySlotEditor>(valueTreeState, slotPrefix);
    case 5: return std::make_unique<ReverbSlotEditor>(valueTreeState, slotPrefix);
    case 6: return std::make_unique<AdvancedCompressorSlotEditor>(valueTreeState, slotPrefix);
    case 7: return std::make_unique<ChromaTapeSlotEditor>(valueTreeState, slotPrefix);
    case 8: return std::make_unique<MorphoCompSlotEditor>(valueTreeState, slotPrefix);
    case 9: return std::make_unique<PhysicalResonatorSlotEditor>(valueTreeState, slotPrefix);
    case 10: return std::make_unique<SpectralAnimatorSlotEditor>(valueTreeState, slotPrefix);
    default: return nullptr;
    }
}
juce::String ModuleSlot::getModuleName(int choice) {
    switch (choice)
    {
    case 1: return "Distortion";
    case 2: return "Filter";
    case 3: return "Modulation";
    case 4: return "Delay";
    case 5: return "Reverb";
    case 6: return "Compressor";
    case 7: return "ChromaTape";
    case 8: return "MorphoComp";
    case 9: return "Physical Resonator";
    case 10: return "Spectral Animator";
    default: return "";
    }
}

void ModuleSlot::showModuleMenu() {
    auto* parameter = valueTreeState.getParameter(slotChoiceParamId);
    if (parameter == nullptr) return;
    auto choices = parameter->getAllValueStrings();
    choices.remove(0); // Remove "Empty"

    auto* grid = new ModuleSelectionGrid(choices);
    grid->setSize(420, 330);
    grid->onModuleSelected = [this, parameter](int choice)
        {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(choice)));
        };

    juce::Rectangle<int> launchBounds;
    if (currentEditor == nullptr)
    {
        launchBounds = addModuleButton.getScreenBounds();
    }
    else
    {
        launchBounds = header->optionsButton.getScreenBounds();
    }

    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(grid), launchBounds, nullptr);
}