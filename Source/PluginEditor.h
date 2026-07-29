#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class OctoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OctoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;
};

class VizComponent : public juce::Component, private juce::Timer
{
public:
    explicit VizComponent (OctoSnareProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    OctoSnareProcessor& proc;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> (fftSize, 0.0f);
    int ringPos = 0;
    std::vector<float> fftData = std::vector<float> (fftSize * 2, 0.0f);
    std::vector<float> spectrum = std::vector<float> (48, 0.0f);
    std::vector<float> wave = std::vector<float> (73, 0.0f);
    float amp = 0.0f, phase = 0.0f;
};

struct EffectCell : public juce::Component
{
    EffectCell (OctoSnareProcessor& p, const juce::String& labelText,
                const juce::String& paramId, const juce::String& onId,
                const juce::String& suffix);
    void resized() override;

    juce::ToggleButton power;
    juce::Slider knob;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
};

class OctoSnareEditor : public juce::AudioProcessorEditor
{
public:
    explicit OctoSnareEditor (OctoSnareProcessor&);
    ~OctoSnareEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshPresetBox();
    void presetSelected();

    OctoSnareProcessor& proc;
    OctoLookAndFeel lnf;

    VizComponent viz;
    juce::OwnedArray<EffectCell> cells;

    juce::Label title;
    juce::ComboBox presetBox;
    juce::TextEditor presetName;
    juce::TextButton saveButton { "Sauver" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctoSnareEditor)
};
