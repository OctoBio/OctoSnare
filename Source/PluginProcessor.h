#pragma once
#include <JuceHeader.h>

struct PitchShifter
{
    void prepare (double sampleRate, int numChannels)
    {
        bufLen = 1 << 16;
        buf.clear();
        for (int c = 0; c < numChannels; ++c)
            buf.emplace_back (bufLen, 0.0f);
        writePos = 0;
        phase = 0.0f;
        window = (float) (0.020 * sampleRate);
    }

    void setSemitones (float st) { ratio = std::pow (2.0f, st / 12.0f); }

    float readInterp (int ch, float delaySamples) const
    {
        float pos = (float) writePos - delaySamples;
        while (pos < 0.0f) pos += (float) bufLen;
        int i0 = ((int) pos) & (bufLen - 1);
        int i1 = (i0 + 1) & (bufLen - 1);
        float f = pos - std::floor (pos);
        return buf[(size_t) ch][(size_t) i0] * (1.0f - f) + buf[(size_t) ch][(size_t) i1] * f;
    }

    void process (juce::AudioBuffer<float>& b)
    {
        const int n  = b.getNumSamples();
        const int ch = juce::jmin ((int) buf.size(), b.getNumChannels());
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < ch; ++c)
                buf[(size_t) c][(size_t) writePos] = b.getSample (c, i);

            float d1 = phase;
            float d2 = std::fmod (phase + window * 0.5f, window);
            float g1 = std::sin (juce::MathConstants<float>::pi * d1 / window);
            float g2 = std::sin (juce::MathConstants<float>::pi * d2 / window);

            for (int c = 0; c < ch; ++c)
                b.setSample (c, i, readInterp (c, d1) * g1 + readInterp (c, d2) * g2);

            phase += (1.0f - ratio);
            if (phase >= window) phase -= window;
            if (phase < 0.0f)    phase += window;
            writePos = (writePos + 1) & (bufLen - 1);
        }
    }

    std::vector<std::vector<float>> buf;
    int bufLen = 0, writePos = 0;
    float phase = 0.0f, window = 2048.0f, ratio = 1.0f;
};

struct TransientShaper
{
    void prepare (double sampleRate)
    {
        aFast = (float) std::exp (-1.0 / (0.0002 * sampleRate));
        aSlow = (float) std::exp (-1.0 / (0.015  * sampleRate));
        aRel  = (float) std::exp (-1.0 / (0.06   * sampleRate));
        envFast = envSlow = 0.0f;
    }

    void process (juce::AudioBuffer<float>& b, float amount)
    {
        const int n  = b.getNumSamples();
        const int ch = b.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            float x = 0.0f;
            for (int c = 0; c < ch; ++c)
                x = juce::jmax (x, std::abs (b.getSample (c, i)));

            envFast = x > envFast ? aFast * envFast + (1.0f - aFast) * x
                                  : aRel  * envFast + (1.0f - aRel)  * x;
            envSlow = x > envSlow ? aSlow * envSlow + (1.0f - aSlow) * x
                                  : aRel  * envSlow + (1.0f - aRel)  * x;

            float diff = juce::jmax (0.0f, envFast - envSlow);
            float g = 1.0f + amount * 6.0f * diff;
            for (int c = 0; c < ch; ++c)
                b.setSample (c, i, b.getSample (c, i) * g);
        }
    }

    float aFast = 0, aSlow = 0, aRel = 0, envFast = 0, envSlow = 0;
};

struct CrackleGen
{
    void prepare (double sampleRate)
    {
        double w0 = juce::MathConstants<double>::twoPi * 4200.0 / sampleRate;
        double alpha = std::sin (w0) / (2.0 * 0.6);
        double cosw = std::cos (w0);
        double a0 = 1.0 + alpha;
        b0 = (float) (alpha / a0);
        b2 = (float) (-alpha / a0);
        a1 = (float) (-2.0 * cosw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
        x1 = x2 = y1 = y2 = 0.0f;
    }

    float next()
    {
        float x = rng.nextFloat() * 2.0f - 1.0f;
        float in = x * 0.012f;
        if (rng.nextFloat() < 0.00022f)
            in += (rng.nextFloat() * 2.0f - 1.0f) * 1.4f;
        float y = b0 * in + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = in; y2 = y1; y1 = y;
        return y;
    }

    juce::Random rng;
    float b0 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

class OctoSnareProcessor : public juce::AudioProcessor
{
public:
    OctoSnareProcessor();
    ~OctoSnareProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OctoSnare"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    juce::File getPresetDirectory() const;
    juce::StringArray getUserPresetNames() const;
    void saveUserPreset (const juce::String& name);
    void loadUserPreset (const juce::String& name);
    void applyFactoryPreset (int index);
    static juce::StringArray getFactoryPresetNames();

    juce::AbstractFifo vizFifo { 1 << 14 };
    std::vector<float> vizBuffer = std::vector<float> (1 << 14, 0.0f);
    void pushViz (const juce::AudioBuffer<float>& b);

private:
    double currentSampleRate = 44100.0;

    PitchShifter pitcher;
    TransientShaper transient;
    CrackleGen crackle;

    juce::dsp::LadderFilter<float> ladder;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::Compressor<float> parallelComp;
    juce::AudioBuffer<float> parallelBuffer;
    juce::Reverb reverb;

    using IIRFilter = juce::dsp::IIR::Filter<float>;
    IIRFilter aaFilter1[2], aaFilter2[2];
    IIRFilter shelfFilter[2], midCutFilter[2];
    float lastBoom = -999.0f;

    float srPhase = 0.0f;
    float srHold[2] = { 0.0f, 0.0f };

    std::atomic<float>* pPitchOn;  std::atomic<float>* pPitch;
    std::atomic<float>* pCrunchOn; std::atomic<float>* pBits;
    std::atomic<float>* pVintOn;   std::atomic<float>* pCutoff;
    std::atomic<float>* pSatOn;    std::atomic<float>* pSat;
    std::atomic<float>* pCrackOn;  std::atomic<float>* pCrack;
    std::atomic<float>* pPunchOn;  std::atomic<float>* pPunch;
    std::atomic<float>* pBoomOn;   std::atomic<float>* pBoom;
    std::atomic<float>* pDustOn;   std::atomic<float>* pDust;
    std::atomic<float>* pRoomOn;   std::atomic<float>* pRoom;
    std::atomic<float>* pOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctoSnareProcessor)
};
