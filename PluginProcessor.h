#pragma once
#include <JuceHeader.h>
#include <array>
#include "Engine.h"

class GltchProcessor : public juce::AudioProcessor
{
public:
    GltchProcessor();
    ~GltchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLTCH-1024"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- accès pour l'éditeur
    void setParamValue (const juce::String& id, float realValue);
    float getParamValue (const juce::String& id) const;

    // presets (50 emplacements, persistants dans %APPDATA%/GLTCH-1024/presets.json)
    static constexpr int numPresets = 50;
    bool hasPreset (int i) const { return presets[(size_t) i].isObject(); }
    void storePreset (int i);
    void clearPreset (int i);
    bool applyPreset (int i);
    juce::String presetsAsJson() const;
    int importPresetsJson (const juce::String& text); // -1 si invalide, sinon nombre de presets
    void savePresetFile();

    juce::AudioProcessorValueTreeState apvts;
    gltch::Engine engine;
    gltch::Rng uiRng;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::var encodePreset() const;
    bool applyPresetVar (const juce::var& v);
    static bool presetOk (const juce::var& o);
    juce::File presetFile() const;
    void loadPresetFile();
    std::string encP() const;
    std::string encN() const;
    std::string encG() const;
    void decodePatterns (const juce::String& p, const juce::String& n, const juce::String& g);

    std::atomic<float>* pBpm = nullptr;
    std::atomic<float>* pDens = nullptr;
    std::atomic<float>* pChaos = nullptr;
    std::atomic<float>* pSw = nullptr;
    std::atomic<float>* pRoot = nullptr;
    std::atomic<float>* pSci = nullptr;
    std::atomic<float>* pAuto = nullptr;
    std::atomic<float>* pPlay = nullptr;
    std::atomic<float>* pFx[gltch::FX_COUNT] {};
    std::atomic<float>* pSnd[6] {};

    std::array<juce::var, numPresets> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GltchProcessor)
};
