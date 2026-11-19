#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class GltchEditor;

// Grille de 64 pas (16 colonnes x 4 lignes) : voie glitch ou piste
class StepGrid : public juce::Component
{
public:
    StepGrid (GltchEditor& e, bool glitchLane) : ed (e), isGlitch (glitchLane) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> cellRect (int i) const;
    int cellAt (juce::Point<int> p) const;
    void applyCell (int c);

    GltchEditor& ed;
    bool isGlitch;
    int paintVal = 0, lastCell = -1;
};

// Pad FX : maintenu (ou LATCH) -> paramètre automatisable fxN
class FxPad : public juce::Component
{
public:
    FxPad (GltchEditor& e, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool isOn() const;
    void setOn (bool on);
    GltchEditor& ed;
    int idx;
    juce::RangedAudioParameter* par;
};

class PresetGrid : public juce::Component
{
public:
    explicit PresetGrid (GltchEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> cellRect (int i) const;
    GltchEditor& ed;
};

class GltchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GltchEditor (GltchProcessor& p);
    ~GltchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    GltchProcessor& proc;
    int page = 0, brush = 1, trk = 0, pcur = -1;
    bool follow = true, latch = false;

    void presetClick (int i);

private:
    void timerCallback() override;
    int soundIdx (int r) const;
    void setSound (int r, int idx);
    void stepSound (int d);
    void preview();
    void cycleParam (const juce::String& id, int count);
    void doExport();
    void doImport();

    juce::LookAndFeel_V4 laf;

    juce::Slider bpmS, densS, chaosS, swS;
    juce::Label bpmL, densL, chaosL, swL;
    juce::TextButton playB, rgB, rpB, muB, rnB, raB, cxB, auB, laB, scB, rtB, foB;
    juce::TextButton sbb, sp, sn, sx, sff, sr, sa, pmsB, pmdB, pexB, pimB;
    juce::TextButton pageB[16], palB[16], tabB[6];

    StepGrid glGrid { *this, true };
    StepGrid trGrid { *this, false };
    PresetGrid pgGrid { *this };
    std::unique_ptr<FxPad> fxPad[gltch::FX_COUNT];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmA, densA, chaosA, swA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playA, autoA;

    juce::Rectangle<int> lblGlitch, lblTrack, lblHint, lblFx, lblPreset;
    juce::String presetMsg;
    int pmode = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GltchEditor)
};
