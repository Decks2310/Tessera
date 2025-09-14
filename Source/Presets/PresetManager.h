#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager
{
public:
    struct PresetInfo { juce::String name; juce::File file; };

    PresetManager(juce::AudioProcessorValueTreeState& s, juce::AudioProcessor& proc, const juce::String& pluginName)
        : apvts(s), processor(proc)
    {
        baseDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(pluginName).getChildFile("Presets");
        baseDir.createDirectory();
        globalDir = baseDir.getChildFile("Global"); globalDir.createDirectory();
        moduleDir = baseDir.getChildFile("Modules"); moduleDir.createDirectory();
        refreshGlobal();
    }

    //================ Global =================
    void refreshGlobal()
    {
        globalPresets.clear();
        if (globalDir.isDirectory())
        {
            for (auto f : globalDir.findChildFiles(juce::File::findFiles, false, "*.xml"))
                globalPresets.push_back({ f.getFileNameWithoutExtension(), f });
            std::sort(globalPresets.begin(), globalPresets.end(), [](auto& a, auto& b){ return a.name < b.name; });
        }
    }

    const std::vector<PresetInfo>& getGlobalPresets() const { return globalPresets; }

    bool saveGlobal(const juce::String& name)
    {
        auto file = globalDir.getChildFile(name + ".xml");
        juce::ValueTree state = apvts.copyState();
        if (auto xml = state.createXml(); xml != nullptr)
            return xml->writeTo(file);
        return false;
    }

    bool loadGlobal(const juce::String& name)
    {
        auto file = globalDir.getChildFile(name + ".xml");
        if (!file.existsAsFile()) return false;
        juce::XmlDocument doc(file);
        std::unique_ptr<juce::XmlElement> xml(doc.getDocumentElement());
        if (!xml) return false;
        juce::ValueTree vt = juce::ValueTree::fromXml(*xml);
        apvts.replaceState(vt);
        return true;
    }

    bool deleteGlobal(const juce::String& name)
    {
        auto file = globalDir.getChildFile(name + ".xml");
        return file.existsAsFile() ? file.deleteFile() : false;
    }

    //================ Module (slot-scoped) =================
    std::vector<PresetInfo> getModulePresets(int slot) const
    {
        std::vector<PresetInfo> list;
        auto dir = moduleDir.getChildFile("Slot" + juce::String(slot + 1));
        if (dir.isDirectory())
        {
            for (auto f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
                list.push_back({ f.getFileNameWithoutExtension(), f });
            std::sort(list.begin(), list.end(), [](auto& a, auto& b){ return a.name < b.name; });
        }
        return list;
    }

    bool saveModule(int slot, const juce::String& name)
    {
        auto dir = moduleDir.getChildFile("Slot" + juce::String(slot + 1)); dir.createDirectory();
        auto file = dir.getChildFile(name + ".xml");
        juce::ValueTree subset("Slot");
        subset.setProperty("slot", slot, nullptr);
        auto& state = apvts.state;
        auto prefix = juce::String("SLOT_") + juce::String(slot + 1) + "_";
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild(i);
            auto id = child.getType().toString();
            if (id.startsWith(prefix))
                subset.addChild(child.createCopy(), -1, nullptr);
        }
        if (auto xml = subset.createXml(); xml != nullptr)
            return xml->writeTo(file);
        return false;
    }

    bool loadModule(int slot, const juce::String& name)
    {
        auto dir = moduleDir.getChildFile("Slot" + juce::String(slot + 1));
        auto file = dir.getChildFile(name + ".xml");
        if (!file.existsAsFile()) return false;
        juce::XmlDocument doc(file);
        std::unique_ptr<juce::XmlElement> xml(doc.getDocumentElement());
        if (!xml) return false;
        juce::ValueTree vt = juce::ValueTree::fromXml(*xml);
        auto prefix = juce::String("SLOT_") + juce::String(slot + 1) + "_";
        for (int i = 0; i < vt.getNumChildren(); ++i)
        {
            auto child = vt.getChild(i);
            auto id = child.getType().toString();
            if (auto* p = apvts.getParameter(id))
            {
                auto norm = (float)child.getProperty("value", p->getValue());
                p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
            }
        }
        return true;
    }

    void randomizeGlobal()
    {
        juce::Random r;
        for (auto* p : processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            {
                juce::String pid = rp->getParameterID();
                if (pid.contains("OUTPUT_GAIN") || pid.contains("INPUT_GAIN")) continue;
                if (pid.contains("MIX") || pid.contains("MASTER_MIX")) continue;
                rp->setValueNotifyingHost(r.nextFloat());
            }
        }
    }

    void randomizeModule(int slot)
    {
        juce::Random r; juce::String prefix = "SLOT_" + juce::String(slot + 1) + "_";
        for (auto* p : processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
                if (rp->getParameterID().startsWith(prefix))
                    rp->setValueNotifyingHost(r.nextFloat());
        }
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioProcessor& processor;
    juce::File baseDir, globalDir, moduleDir;
    std::vector<PresetInfo> globalPresets;
};
