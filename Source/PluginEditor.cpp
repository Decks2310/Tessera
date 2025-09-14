// File: PluginEditor.cpp
//================================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace LayoutConstants {
    constexpr int HEADER_HEIGHT = 120;
    constexpr int BOTTOM_STRIP_HEIGHT = 60;
    constexpr int ADD_ROW_BUTTON_HEIGHT = 40;
    constexpr int DEFAULT_SLOT_ROW_HEIGHT = 250;
    constexpr int WIDE_SLOT_ROW_HEIGHT = 350;
    constexpr int SLOT_MARGIN = 5;
    constexpr int PLUGIN_WIDTH = 840;
    constexpr int NUM_COLS = 4;
    constexpr int FADER_WIDTH = 50;
}

ModularMultiFxAudioProcessorEditor::ModuleInfo ModularMultiFxAudioProcessorEditor::getModuleInfo(int choice) {
    if (choice == 7) return { choice, 3 }; // ChromaTape spans 3 slots
    return { choice, 1 };
}

ModularMultiFxAudioProcessorEditor::ModularMultiFxAudioProcessorEditor(ModularMultiFxAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
      inputGainFader(p.apvts, "INPUT_GAIN", "Input"),
      outputGainFader(p.apvts, "OUTPUT_GAIN", "Output"),
      responseTimeKnob(p.apvts, "SAG_RESPONSE", "Response") {

    setLookAndFeel(&customLookAndFeel);
    processorRef.editorResizeBroadcaster.addChangeListener(this);
    processorRef.osLockChangeBroadcaster.addChangeListener(this);

    addAndMakeVisible(titleLabel); titleLabel.setText("TESSERA", juce::dontSendNotification); titleLabel.setFont(juce::FontOptions(24.0f)); titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel); subtitleLabel.setText("MULTIMODULAR FX AUDIO PLUGIN", juce::dontSendNotification); subtitleLabel.setFont(juce::FontOptions(12.0f)); subtitleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(oversamplingAlgoBox);
    if (auto* osAlgoParam = processorRef.apvts.getParameter("OVERSAMPLING_ALGO")) {
        oversamplingAlgoBox.addItemList(osAlgoParam->getAllValueStrings(), 1);
        oversamplingAlgoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "OVERSAMPLING_ALGO", oversamplingAlgoBox);
    }
    addAndMakeVisible(oversamplingRateBox);
    if (auto* osRateParam = processorRef.apvts.getParameter("OVERSAMPLING_RATE")) {
        oversamplingRateBox.addItemList(osRateParam->getAllValueStrings(), 1);
        oversamplingRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.apvts, "OVERSAMPLING_RATE", oversamplingRateBox);
    }

    addAndMakeVisible(osLockWarningLabel); osLockWarningLabel.setFont(juce::FontOptions(11.0f).withStyle("Italic")); osLockWarningLabel.setColour(juce::Label::textColourId, juce::Colours::orange); osLockWarningLabel.setJustificationType(juce::Justification::centredLeft); osLockWarningLabel.setVisible(false);

    addAndMakeVisible(autoGainButton); autoGainButton.setButtonText("Auto-Gain"); autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.apvts, "SAG_ENABLE", autoGainButton);

    addAndMakeVisible(inputGainFader); addAndMakeVisible(outputGainFader); addAndMakeVisible(responseTimeKnob);

    addAndMakeVisible(masterMixLabel); masterMixLabel.setText("Master Mix", juce::dontSendNotification); masterMixLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(masterMixSlider); masterMixSlider.setSliderStyle(juce::Slider::LinearHorizontal); masterMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); masterMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, "MASTER_MIX", masterMixSlider);

    addAndMakeVisible(addRowButton);
    addRowButton.onClick = [this] {
        int current = processorRef.getVisibleSlotCount();
        if (current >= ModularMultiFxAudioProcessor::maxSlots) return;
        int proposed = current + LayoutConstants::NUM_COLS;
        if (proposed > ModularMultiFxAudioProcessor::maxSlots) proposed = ModularMultiFxAudioProcessor::maxSlots;
        processorRef.setVisibleSlotCount(proposed);
    };

    addAndMakeVisible(globalPresetBox); addAndMakeVisible(savePresetButton); addAndMakeVisible(deletePresetButton); addAndMakeVisible(randomPresetButton); addAndMakeVisible(newPresetButton);
    refreshPresetBar();
    globalPresetBox.onChange = [this]{ if(auto* pm = processorRef.getPresetManager()){ auto nm = globalPresetBox.getText(); if(nm.isNotEmpty()) pm->loadGlobal(nm);} };
    savePresetButton.onClick = [this]{ if(auto* pm = processorRef.getPresetManager()){ auto nm = globalPresetBox.getText(); if(nm.isEmpty()) nm = "Preset_" + juce::Time::getCurrentTime().toString(true,true,true,true); pm->saveGlobal(nm); pm->refreshGlobal(); refreshPresetBar(); globalPresetBox.setText(nm, juce::dontSendNotification);} };
    deletePresetButton.onClick = [this]{ if(auto* pm = processorRef.getPresetManager()){ auto nm = globalPresetBox.getText(); if(nm.isNotEmpty()){ pm->deleteGlobal(nm); pm->refreshGlobal(); refreshPresetBar(); globalPresetBox.setText({}, juce::dontSendNotification);} } };
    randomPresetButton.onClick = [this]{ if(auto* pm = processorRef.getPresetManager()) pm->randomizeGlobal(); };
    newPresetButton.onClick = [this]{ globalPresetBox.setText({}, juce::dontSendNotification); };

    updateSlotsAndResize();
    updateOversamplingControlsState();
    setSize(LayoutConstants::PLUGIN_WIDTH + LayoutConstants::FADER_WIDTH * 2, LayoutConstants::HEADER_HEIGHT + LayoutConstants::BOTTOM_STRIP_HEIGHT + 400);
}

ModularMultiFxAudioProcessorEditor::~ModularMultiFxAudioProcessorEditor() {
    setLookAndFeel(nullptr);
    processorRef.editorResizeBroadcaster.removeChangeListener(this);
    processorRef.osLockChangeBroadcaster.removeChangeListener(this);
}

void ModularMultiFxAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(customLookAndFeel.findColour(juce::ResizableWindow::backgroundColourId));
}

void ModularMultiFxAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* src) {
    if (src == &processorRef.osLockChangeBroadcaster) { updateOversamplingControlsState(); return; }
    if (src == &processorRef.editorResizeBroadcaster) {
        juce::Component::SafePointer<ModularMultiFxAudioProcessorEditor> safe(this);
        juce::MessageManager::callAsync([safe] { if (auto* ed = safe.getComponent()) { ed->updateSlotsAndResize(); ed->updateOversamplingControlsState(); } });
    }
}

static int getParameterIndexRobust(juce::RangedAudioParameter* param) {
    if (!param) return 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(param)) return c->getIndex();
    int steps = param->getNumSteps();
    if (steps > 1) return (int) std::round(param->getValue() * (steps - 1));
    return 0;
}

void ModularMultiFxAudioProcessorEditor::updateOversamplingControlsState() {
    auto* rateParam = processorRef.apvts.getParameter("OVERSAMPLING_RATE");
    auto* algoParam = processorRef.apvts.getParameter("OVERSAMPLING_ALGO");
    auto sync = [](juce::ComboBox& box, juce::RangedAudioParameter* p) { if (p) box.setSelectedId(getParameterIndexRobust(p) + 1, juce::dontSendNotification); };

    if (processorRef.isNonRealtime()) {
        oversamplingRateBox.setEnabled(true); osLockWarningLabel.setVisible(false);
        sync(oversamplingRateBox, rateParam); sync(oversamplingAlgoBox, algoParam); return;
    }
    bool locked = processorRef.isOversamplingLocked(); oversamplingRateBox.setEnabled(!locked); osLockWarningLabel.setVisible(locked);
    if (locked) {
        if (rateParam) {
            int idx = getParameterIndexRobust(rateParam); if (idx > 1) idx = 1; oversamplingRateBox.setSelectedId(idx + 1, juce::dontSendNotification);
        }
    } else { sync(oversamplingRateBox, rateParam); sync(oversamplingAlgoBox, algoParam); }
}

void ModularMultiFxAudioProcessorEditor::updateSlotsAndResize() {
    int slotsToShow = processorRef.getVisibleSlotCount();
    if (moduleSlots.size() < (size_t) slotsToShow) {
        for (int i = (int) moduleSlots.size(); i < slotsToShow; ++i) {
            moduleSlots.push_back(std::make_unique<ModuleSlot>(processorRef.apvts, i));
            addAndMakeVisible(*moduleSlots.back());
        }
    } else if (moduleSlots.size() > (size_t) slotsToShow) {
        moduleSlots.resize(slotsToShow);
    }

    int totalSlotHeight = 0;
    for (int row = 0; row < slotsToShow; row += LayoutConstants::NUM_COLS) {
        bool wide = false;
        for (int c = 0; c < LayoutConstants::NUM_COLS; ++c) {
            int si = row + c; if (si >= slotsToShow) break;
            if (auto* v = processorRef.apvts.getRawParameterValue("SLOT_" + juce::String(si + 1) + "_CHOICE")) {
                if ((int) v->load() == 7) { wide = true; break; }
            }
        }
        totalSlotHeight += wide ? LayoutConstants::WIDE_SLOT_ROW_HEIGHT : LayoutConstants::DEFAULT_SLOT_ROW_HEIGHT;
    }

    int targetH = LayoutConstants::HEADER_HEIGHT + totalSlotHeight + LayoutConstants::ADD_ROW_BUTTON_HEIGHT + LayoutConstants::BOTTOM_STRIP_HEIGHT;
    int targetW = LayoutConstants::PLUGIN_WIDTH + LayoutConstants::FADER_WIDTH * 2;
    addRowButton.setVisible(slotsToShow < ModularMultiFxAudioProcessor::maxSlots);
    if (!addRowButton.isVisible()) targetH -= LayoutConstants::ADD_ROW_BUTTON_HEIGHT;
    bool changed = (getWidth() != targetW || getHeight() != targetH);
    setSize(targetW, targetH);
    if (!changed) resized();
}

void ModularMultiFxAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    auto leftCol = bounds.removeFromLeft(LayoutConstants::FADER_WIDTH);
    auto rightCol = bounds.removeFromRight(LayoutConstants::FADER_WIDTH);
    auto header = bounds.removeFromTop(LayoutConstants::HEADER_HEIGHT).reduced(10, 2);

    // Preset bar row
    auto presetRow = header.removeFromTop(24);
    {
        auto row = presetRow;
        auto buttonsWidth = 45 + 40 + 30 + 50 + 8; // approximate widths
        auto buttonsArea = row.removeFromRight(buttonsWidth);
        savePresetButton.setBounds(buttonsArea.removeFromRight(45)); buttonsArea.removeFromRight(2);
        deletePresetButton.setBounds(buttonsArea.removeFromRight(40)); buttonsArea.removeFromRight(2);
        randomPresetButton.setBounds(buttonsArea.removeFromRight(30)); buttonsArea.removeFromRight(2);
        newPresetButton.setBounds(buttonsArea.removeFromRight(50));
        globalPresetBox.setBounds(row);
    }

    auto headerTop = header.removeFromTop(55);
    auto headerBottom = header;

    auto osArea = headerTop.removeFromLeft(250).reduced(0, 8);
    oversamplingAlgoBox.setBounds(osArea.removeFromLeft(140)); osArea.removeFromLeft(10); oversamplingRateBox.setBounds(osArea);
    autoGainButton.setBounds(headerTop.removeFromRight(120).reduced(0, 8));
    auto titleBounds = headerTop; titleLabel.setBounds(titleBounds.removeFromTop(24)); subtitleLabel.setBounds(titleBounds);
    juce::FlexBox fb; fb.justifyContent = juce::FlexBox::JustifyContent::center; fb.alignItems = juce::FlexBox::AlignItems::center; float knobSize = (float) headerBottom.getHeight(); fb.items.add(juce::FlexItem(responseTimeKnob).withWidth(knobSize).withHeight(knobSize)); fb.performLayout(headerBottom);

    auto bottomStrip = bounds.removeFromBottom(LayoutConstants::BOTTOM_STRIP_HEIGHT).reduced(20, 5);
    masterMixLabel.setBounds(bottomStrip.removeFromTop(20)); masterMixSlider.setBounds(bottomStrip);
    if (addRowButton.isVisible()) { auto addArea = bounds.removeFromBottom(LayoutConstants::ADD_ROW_BUTTON_HEIGHT); addRowButton.setBounds(addArea.withSizeKeepingCentre(40, 30)); }

    inputGainFader.setBounds(leftCol.withY(bounds.getY()).withHeight(bounds.getHeight()).reduced(5, 10));
    outputGainFader.setBounds(rightCol.withY(bounds.getY()).withHeight(bounds.getHeight()).reduced(5, 10));

    int visible = processorRef.getVisibleSlotCount();
    if (visible == 0 || moduleSlots.empty()) return;
    for (auto& s : moduleSlots) s->setVisible(false);

    auto slotArea = bounds;
    int idx = 0;
    while (idx < visible) {
        bool wide = false;
        for (int c = 0; c < LayoutConstants::NUM_COLS; ++c) {
            int si = idx + c; if (si >= visible) break;
            if (auto* v = processorRef.apvts.getRawParameterValue("SLOT_" + juce::String(si + 1) + "_CHOICE")) { if ((int)v->load() == 7) { wide = true; break; } }
        }
        int rowH = wide ? LayoutConstants::WIDE_SLOT_ROW_HEIGHT : LayoutConstants::DEFAULT_SLOT_ROW_HEIGHT;
        auto row = slotArea.removeFromTop(rowH);
        float slotW = (float)row.getWidth() / LayoutConstants::NUM_COLS;
        int col = 0;
        while (col < LayoutConstants::NUM_COLS && idx < visible) {
            int choice = 0; if (auto* vp = processorRef.apvts.getRawParameterValue("SLOT_" + juce::String(idx + 1) + "_CHOICE")) choice = (int)vp->load();
            auto info = getModuleInfo(choice); int span = info.slotsUsed; if (col + span > LayoutConstants::NUM_COLS) span = LayoutConstants::NUM_COLS - col; if (idx + span > visible) span = visible - idx;
            int startX = row.getX() + (int)std::round(col * slotW); int endX = row.getX() + (int)std::round((col + span) * slotW);
            auto r = juce::Rectangle<int>(startX, row.getY(), endX - startX, row.getHeight()).reduced(LayoutConstants::SLOT_MARGIN);
            if (idx < moduleSlots.size()) { moduleSlots[idx]->setBounds(r); moduleSlots[idx]->setVisible(true); }
            col += span; idx += span; }
    }
}

// Rename function to avoid stale ODR clash
void ModularMultiFxAudioProcessorEditor::refreshPresetBar(){
    globalPresetBox.clear(juce::dontSendNotification);
    if(auto* pm = processorRef.getPresetManager()){
        int id = 1;
        for(const auto& p : pm->getGlobalPresets())
            globalPresetBox.addItem(p.name, id++);
    }
}

// EOF