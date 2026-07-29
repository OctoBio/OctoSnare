#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::NormalisableRange<float> logRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout OctoSnareProcessor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using B  = juce::AudioParameterBool;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto add = [&] (const char* id, const char* name, juce::NormalisableRange<float> range, float def)
    {
        params.push_back (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def));
    };
    auto addOn = [&] (const char* id, const char* name, bool def)
    {
        params.push_back (std::make_unique<B> (juce::ParameterID { id, 1 }, name, def));
    };

    addOn ("pitchOn",  "Pitch actif",  true);
    add   ("pitch",    "Pitch down",   { -12.0f, 0.0f, 1.0f }, -3.0f);
    addOn ("crunchOn", "Crunch actif", true);
    add   ("bits",     "Crunch bits",  { 6.0f, 16.0f, 1.0f }, 12.0f);
    addOn ("vintOn",   "Vintage actif", true);
    add   ("cutoff",   "Vintage filtre", logRange (2500.0f, 18000.0f), 9000.0f);
    addOn ("satOn",    "Saturation active", true);
    add   ("sat",      "Saturation",   { 0.0f, 100.0f, 1.0f }, 20.0f);
    addOn ("crackOn",  "Crack actif",  true);
    add   ("crack",    "Crack",        { 0.0f, 100.0f, 1.0f }, 20.0f);
    addOn ("punchOn",  "Punch actif",  true);
    add   ("punch",    "Punch",        { 0.0f, 100.0f, 1.0f }, 35.0f);
    addOn ("boomOn",   "Boom actif",   true);
    add   ("boom",     "Boom",         { 0.0f, 12.0f, 0.1f }, 4.0f);
    addOn ("dustOn",   "Dust actif",   true);
    add   ("dust",     "Dust",         { 0.0f, 100.0f, 1.0f }, 15.0f);
    addOn ("roomOn",   "Room active",  true);
    add   ("room",     "Room",         { 0.0f, 100.0f, 1.0f }, 12.0f);
    add   ("output",   "Sortie",       { -12.0f, 12.0f, 0.1f }, 0.0f);

    return { params.begin(), params.end() };
}

OctoSnareProcessor::OctoSnareProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pPitchOn  = apvts.getRawParameterValue ("pitchOn");
    pPitch    = apvts.getRawParameterValue ("pitch");
    pCrunchOn = apvts.getRawParameterValue ("crunchOn");
    pBits     = apvts.getRawParameterValue ("bits");
    pVintOn   = apvts.getRawParameterValue ("vintOn");
    pCutoff   = apvts.getRawParameterValue ("cutoff");
    pSatOn    = apvts.getRawParameterValue ("satOn");
    pSat      = apvts.getRawParameterValue ("sat");
    pCrackOn  = apvts.getRawParameterValue ("crackOn");
    pCrack    = apvts.getRawParameterValue ("crack");
    pPunchOn  = apvts.getRawParameterValue ("punchOn");
    pPunch    = apvts.getRawParameterValue ("punch");
    pBoomOn   = apvts.getRawParameterValue ("boomOn");
    pBoom     = apvts.getRawParameterValue ("boom");
    pDustOn   = apvts.getRawParameterValue ("dustOn");
    pDust     = apvts.getRawParameterValue ("dust");
    pRoomOn   = apvts.getRawParameterValue ("roomOn");
    pRoom     = apvts.getRawParameterValue ("room");
    pOutput   = apvts.getRawParameterValue ("output");
}

bool OctoSnareProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto in  = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void OctoSnareProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());

    pitcher.prepare (sampleRate, numCh);
    transient.prepare (sampleRate);
    crackle.prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };
    ladder.prepare (spec);
    ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
    ladder.setResonance (0.15f);
    ladder.setDrive (1.05f);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing ((size_t) samplesPerBlock);

    parallelComp.prepare (spec);
    parallelComp.setThreshold (-35.0f);
    parallelComp.setRatio (12.0f);
    parallelComp.setAttack (1.0f);
    parallelComp.setRelease (60.0f);
    parallelBuffer.setSize (numCh, samplesPerBlock);

    reverb.setSampleRate (sampleRate);

    auto aa1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 11700.0f, 0.54f);
    auto aa2 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 11700.0f, 1.31f);
    for (int c = 0; c < 2; ++c)
    {
        aaFilter1[c].coefficients = aa1;
        aaFilter2[c].coefficients = aa2;
        aaFilter1[c].reset();
        aaFilter2[c].reset();
        shelfFilter[c].reset();
        midCutFilter[c].reset();
    }
    lastBoom = -999.0f;
    srPhase = 0.0f;
    srHold[0] = srHold[1] = 0.0f;

    setLatencySamples ((int) oversampler->getLatencyInSamples());
}

void OctoSnareProcessor::pushViz (const juce::AudioBuffer<float>& b)
{
    const int n = b.getNumSamples();
    int s1, n1, s2, n2;
    vizFifo.prepareToWrite (n, s1, n1, s2, n2);
    const float* l = b.getReadPointer (0);
    const float* r = b.getNumChannels() > 1 ? b.getReadPointer (1) : l;
    int idx = 0;
    for (int i = 0; i < n1; ++i, ++idx) vizBuffer[(size_t) (s1 + i)] = 0.5f * (l[idx] + r[idx]);
    for (int i = 0; i < n2; ++i, ++idx) vizBuffer[(size_t) (s2 + i)] = 0.5f * (l[idx] + r[idx]);
    vizFifo.finishedWrite (n1 + n2);
}

void OctoSnareProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numSamples == 0 || numCh == 0) return;

    if (pPitchOn->load() > 0.5f && pPitch->load() < -0.01f)
    {
        pitcher.setSemitones (pPitch->load());
        pitcher.process (buffer);
    }

    if (pCrunchOn->load() > 0.5f)
    {
        const float bits = pBits->load();
        const float step = std::pow (2.0f, bits - 1.0f);
        const float targetRate = 26040.0f;
        const float inc = targetRate / (float) currentSampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            srPhase += inc;
            const bool sample = srPhase >= 1.0f;
            if (sample) srPhase -= 1.0f;
            for (int c = 0; c < numCh; ++c)
            {
                float x = buffer.getSample (c, i);
                x = aaFilter2[c].processSample (aaFilter1[c].processSample (x));
                if (sample)
                    srHold[c] = std::round (x * step) / step;
                buffer.setSample (c, i, srHold[c]);
            }
        }
    }

    if (pVintOn->load() > 0.5f)
    {
        ladder.setCutoffFrequencyHz (pCutoff->load());
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        ladder.process (ctx);
    }

    if (pSatOn->load() > 0.5f && pSat->load() > 0.5f)
    {
        const float drive = 1.0f + pSat->load() / 100.0f * 14.0f;
        const float norm  = 1.0f / std::tanh (drive);
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampler->processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            float* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = std::tanh (drive * d[i]) * norm;
        }
        oversampler->processSamplesDown (block);
    }
    else
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampler->processSamplesUp (block);
        oversampler->processSamplesDown (block);
    }

    {
        const float boom = pBoomOn->load() > 0.5f ? pBoom->load() : 0.0f;
        if (std::abs (boom - lastBoom) > 0.01f)
        {
            lastBoom = boom;
            auto sc = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                currentSampleRate, 90.0f, 0.7f, juce::Decibels::decibelsToGain (boom));
            auto mc = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                currentSampleRate, 300.0f, 1.0f, juce::Decibels::decibelsToGain (-boom * 0.5f));
            for (int c = 0; c < 2; ++c)
            {
                shelfFilter[c].coefficients  = sc;
                midCutFilter[c].coefficients = mc;
            }
        }
        if (std::abs (lastBoom) > 0.01f)
            for (int c = 0; c < numCh; ++c)
            {
                float* d = buffer.getWritePointer (c);
                for (int i = 0; i < numSamples; ++i)
                    d[i] = midCutFilter[c].processSample (shelfFilter[c].processSample (d[i]));
            }
    }

    if (pCrackOn->load() > 0.5f && pCrack->load() > 0.5f)
        transient.process (buffer, pCrack->load() / 100.0f);

    if (pPunchOn->load() > 0.5f && pPunch->load() > 0.5f)
    {
        for (int c = 0; c < numCh; ++c)
            parallelBuffer.copyFrom (c, 0, buffer, c, 0, numSamples);
        const float makeup = juce::Decibels::decibelsToGain (14.0f);
        const float blend  = pPunch->load() / 100.0f;
        for (int i = 0; i < numSamples; ++i)
            for (int c = 0; c < numCh; ++c)
            {
                float y = parallelComp.processSample (c, parallelBuffer.getSample (c, i)) * makeup;
                buffer.addSample (c, i, y * blend);
            }
    }

    if (pRoomOn->load() > 0.5f && pRoom->load() > 0.5f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.28f;
        rp.damping  = 0.6f;
        rp.width    = 0.7f;
        rp.wetLevel = pRoom->load() / 100.0f * 0.45f;
        rp.dryLevel = 1.0f;
        reverb.setParameters (rp);
        if (numCh > 1)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
        else
            reverb.processMono (buffer.getWritePointer (0), numSamples);
    }

    if (pDustOn->load() > 0.5f && pDust->load() > 0.5f)
    {
        const float level = pDust->load() / 100.0f * 0.25f;
        for (int i = 0; i < numSamples; ++i)
        {
            float d = crackle.next() * level;
            for (int c = 0; c < numCh; ++c)
                buffer.addSample (c, i, d);
        }
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (pOutput->load()));
    pushViz (buffer);
}

void OctoSnareProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OctoSnareProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::File OctoSnareProcessor::getPresetDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("OctoSnare").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::StringArray OctoSnareProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (auto& f : getPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

void OctoSnareProcessor::saveUserPreset (const juce::String& name)
{
    if (auto xml = apvts.copyState().createXml())
        xml->writeTo (getPresetDirectory().getChildFile (name + ".xml"));
}

void OctoSnareProcessor::loadUserPreset (const juce::String& name)
{
    auto f = getPresetDirectory().getChildFile (name + ".xml");
    if (! f.existsAsFile()) return;
    if (auto xml = juce::parseXML (f))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::StringArray OctoSnareProcessor::getFactoryPresetNames()
{
    return { "Classic 90s", "Dusty tape", "Hard NY", "Init (neutre)" };
}

void OctoSnareProcessor::applyFactoryPreset (int index)
{
    struct V { const char* id; float v; };
    static const std::vector<std::vector<V>> presets {
        { {"pitch",-3},{"bits",12},{"cutoff",9000},{"sat",20},{"crack",20},{"punch",35},{"boom",4},{"dust",15},{"room",12},{"output",0} },
        { {"pitch",-5},{"bits",10},{"cutoff",6000},{"sat",45},{"crack",10},{"punch",25},{"boom",3},{"dust",45},{"room",20},{"output",0} },
        { {"pitch",-1},{"bits",12},{"cutoff",12000},{"sat",30},{"crack",55},{"punch",70},{"boom",5},{"dust",8},{"room",8},{"output",0} },
        { {"pitch",0},{"bits",16},{"cutoff",18000},{"sat",0},{"crack",0},{"punch",0},{"boom",0},{"dust",0},{"room",0},{"output",0} }
    };
    if (index < 0 || index >= (int) presets.size()) return;

    static const char* onIds[] = { "pitchOn","crunchOn","vintOn","satOn","crackOn","punchOn","boomOn","dustOn","roomOn" };
    for (auto* id : onIds)
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (1.0f);
            p->endChangeGesture();
        }
    for (auto& pv : presets[(size_t) index])
        if (auto* p = apvts.getParameter (pv.id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (pv.v));
            p->endChangeGesture();
        }
}

juce::AudioProcessorEditor* OctoSnareProcessor::createEditor()
{
    return new OctoSnareEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OctoSnareProcessor();
}
