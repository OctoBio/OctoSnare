#include "PluginEditor.h"

namespace octo
{
    const juce::Colour bg        { 0xff0b1f1c };
    const juce::Colour panel     { 0xff04342c };
    const juce::Colour teal      { 0xff1d9e75 };
    const juce::Colour tealLight { 0xff9fe1cb };
    const juce::Colour tealMid   { 0xff5dcaa5 };
    const juce::Colour blue      { 0xff378add };
    const juce::Colour blueLight { 0xff85b7eb };
    const juce::Colour textDim   { 0xff7fa89f };
}

OctoLookAndFeel::OctoLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, octo::tealLight);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, octo::textDim);
    setColour (juce::ComboBox::backgroundColourId, octo::panel);
    setColour (juce::ComboBox::textColourId, octo::tealLight);
    setColour (juce::ComboBox::outlineColourId, octo::teal.withAlpha (0.4f));
    setColour (juce::ComboBox::arrowColourId, octo::tealMid);
    setColour (juce::PopupMenu::backgroundColourId, octo::panel);
    setColour (juce::PopupMenu::textColourId, octo::tealLight);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, octo::teal);
    setColour (juce::TextEditor::backgroundColourId, octo::panel);
    setColour (juce::TextEditor::textColourId, octo::tealLight);
    setColour (juce::TextEditor::outlineColourId, octo::teal.withAlpha (0.4f));
    setColour (juce::TextEditor::focusedOutlineColourId, octo::tealMid);
    setColour (juce::TextButton::buttonColourId, octo::panel);
    setColour (juce::TextButton::textColourOffId, octo::tealLight);
    setColour (juce::CaretComponent::caretColourId, octo::tealLight);
}

void OctoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float sliderPos, float startAngle, float endAngle,
                                        juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    const float thickness = 3.5f;
    const bool enabled = slider.isEnabled();

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (octo::panel.brighter (0.35f));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (enabled ? octo::tealMid : octo::textDim.withAlpha (0.4f));
    g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (enabled ? octo::panel.brighter (0.15f) : octo::panel);
    g.fillEllipse (juce::Rectangle<float> (radius * 1.3f, radius * 1.3f).withCentre (centre));

    juce::Point<float> tip (centre.x + std::cos (angle - juce::MathConstants<float>::halfPi) * radius * 0.55f,
                            centre.y + std::sin (angle - juce::MathConstants<float>::halfPi) * radius * 0.55f);
    g.setColour (enabled ? octo::tealLight : octo::textDim.withAlpha (0.5f));
    g.drawLine ({ centre, tip }, 2.5f);
}

void OctoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
    auto circle = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const bool on = b.getToggleState();

    g.setColour (on ? octo::teal : octo::panel.brighter (0.25f));
    g.fillEllipse (circle);
    if (highlighted)
    {
        g.setColour (octo::tealLight.withAlpha (0.3f));
        g.drawEllipse (circle, 1.5f);
    }
    g.setColour (on ? octo::tealLight : octo::textDim);
    auto c = circle.getCentre();
    auto ir = d * 0.28f;
    g.drawLine (c.x, c.y - ir, c.x, c.y - ir * 0.2f, 1.8f);
    juce::Path p;
    p.addCentredArc (c.x, c.y, ir, ir, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
    g.strokePath (p, juce::PathStrokeType (1.8f));
}

VizComponent::VizComponent (OctoSnareProcessor& p) : proc (p)
{
    setOpaque (true);
    startTimerHz (30);
}

void VizComponent::timerCallback()
{
    int s1, n1, s2, n2;
    const int ready = proc.vizFifo.getNumReady();
    proc.vizFifo.prepareToRead (ready, s1, n1, s2, n2);
    auto pull = [this] (int start, int num)
    {
        for (int i = 0; i < num; ++i)
        {
            ring[(size_t) ringPos] = proc.vizBuffer[(size_t) (start + i)];
            ringPos = (ringPos + 1) & (fftSize - 1);
        }
    };
    pull (s1, n1);
    pull (s2, n2);
    proc.vizFifo.finishedRead (n1 + n2);

    float peak = 0.0f;
    for (int i = 0; i < fftSize; i += 8)
        peak = juce::jmax (peak, std::abs (ring[(size_t) i]));
    amp = amp * 0.82f + peak * 0.18f;
    phase += 0.06f + amp * 0.5f;

    for (size_t i = 0; i < wave.size(); ++i)
    {
        int idx = (ringPos - fftSize + (int) ((float) i / (float) (wave.size() - 1) * (fftSize - 1)) + 2 * fftSize) & (fftSize - 1);
        wave[i] = wave[i] * 0.5f + ring[(size_t) idx] * 0.5f;
    }

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) & (fftSize - 1))];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const int numBars = (int) spectrum.size();
    for (int b = 0; b < numBars; ++b)
    {
        float frac = std::pow ((float) b / (float) numBars, 2.0f);
        int bin = juce::jlimit (1, fftSize / 2 - 1, (int) (frac * (float) fftSize * 0.35f) + 1);
        float mag = fftData[(size_t) bin] / (float) fftSize * 8.0f;
        float db = juce::Decibels::gainToDecibels (mag, -60.0f);
        float v = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
        v = juce::jlimit (0.0f, 1.0f, v);
        spectrum[(size_t) b] = juce::jmax (v, spectrum[(size_t) b] * 0.86f);
    }
    repaint();
}

void VizComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (octo::panel);
    g.fillRoundedRectangle (r, 8.0f);

    const float cx = r.getWidth() * 0.26f;
    const float cy = r.getHeight() * 0.5f;
    const float base = r.getHeight() * 0.22f + amp * r.getHeight() * 0.3f;
    const int N = (int) wave.size() - 1;

    juce::Path blob;
    for (int i = 0; i <= N; ++i)
    {
        float a = (float) i / (float) N * juce::MathConstants<float>::twoPi;
        float rad = base + wave[(size_t) i] * base * 1.6f * (0.4f + amp)
                    + std::sin (a * 3.0f + phase) * 4.0f
                    + std::sin (a * 5.0f - phase * 1.6f) * 3.0f;
        float px = cx + std::cos (a) * rad;
        float py = cy + std::sin (a) * rad * 0.92f;
        if (i == 0) blob.startNewSubPath (px, py); else blob.lineTo (px, py);
    }
    blob.closeSubPath();
    g.setColour (octo::teal);
    g.fillPath (blob);
    g.setColour (octo::tealLight);
    g.strokePath (blob, juce::PathStrokeType (2.0f));

    juce::Path inner;
    for (int i = 0; i <= N; ++i)
    {
        float a = (float) i / (float) N * juce::MathConstants<float>::twoPi;
        float rad = base * 0.55f + wave[(size_t) ((i + 20) % N)] * base * (0.4f + amp)
                    + std::sin (a * 4.0f - phase * 1.3f) * 3.0f;
        float px = cx + std::cos (a) * rad;
        float py = cy + std::sin (a) * rad;
        if (i == 0) inner.startNewSubPath (px, py); else inner.lineTo (px, py);
    }
    inner.closeSubPath();
    g.setColour (octo::tealMid.withAlpha (0.55f));
    g.fillPath (inner);

    const float bx = r.getWidth() * 0.5f;
    const float bw = r.getWidth() * 0.46f;
    const float by0 = r.getHeight() * 0.82f;
    const float bh = r.getHeight() * 0.66f;
    const int numBars = (int) spectrum.size();
    const float barW = bw / (float) numBars;
    for (int i = 0; i < numBars; ++i)
    {
        float v = spectrum[(size_t) i];
        auto colour = i < numBars / 4 ? octo::blue : (i < numBars * 3 / 5 ? octo::tealMid : octo::blueLight);
        g.setColour (colour.withAlpha (0.35f + v * 0.65f));
        g.fillRect (bx + (float) i * barW, by0 - v * bh, barW - 1.5f, v * bh);
    }
    g.setColour (octo::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::plain));
    g.drawText ("20 Hz", (int) bx, (int) by0 + 3, 50, 12, juce::Justification::left);
    g.drawText ("1 kHz", (int) (bx + bw * 0.55f), (int) by0 + 3, 50, 12, juce::Justification::left);
    g.drawText ("16 kHz", (int) (bx + bw - 44.0f), (int) by0 + 3, 50, 12, juce::Justification::left);
}

EffectCell::EffectCell (OctoSnareProcessor& p, const juce::String& labelText,
                        const juce::String& paramId, const juce::String& onId,
                        const juce::String& suffix)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    knob.setTextValueSuffix (suffix);
    addAndMakeVisible (knob);
    knobAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, knob);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f, juce::Font::plain));
    addAndMakeVisible (label);

    if (onId.isNotEmpty())
    {
        addAndMakeVisible (power);
        powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, onId, power);
        auto sync = [this] { knob.setEnabled (power.getToggleState());
                             label.setAlpha (power.getToggleState() ? 1.0f : 0.45f); };
        power.onStateChange = sync;
        sync();
    }
}

void EffectCell::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (16);
    if (powerAtt != nullptr)
        power.setBounds (top.removeFromRight (18));
    label.setBounds (top);
    knob.setBounds (r);
}

OctoSnareEditor::OctoSnareEditor (OctoSnareProcessor& p)
    : AudioProcessorEditor (p), proc (p), viz (p)
{
    setLookAndFeel (&lnf);

    title.setText ("OCTOSNARE", juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, octo::tealLight);
    addAndMakeVisible (title);

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("- preset -");
    presetBox.onChange = [this] { presetSelected(); };
    refreshPresetBox();

    presetName.setTextToShowWhenEmpty ("nom du preset", octo::textDim);
    addAndMakeVisible (presetName);

    saveButton.onClick = [this]
    {
        auto name = presetName.getText().trim();
        if (name.isEmpty())
            name = "Preset " + juce::String (proc.getUserPresetNames().size() + 1);
        proc.saveUserPreset (name);
        presetName.clear();
        refreshPresetBox();
    };
    addAndMakeVisible (saveButton);

    addAndMakeVisible (viz);

    struct Def { const char* label; const char* id; const char* onId; const char* suffix; };
    static const Def defs[] = {
        { "Pitch down", "pitch",  "pitchOn",  " st" },
        { "Crunch",     "bits",   "crunchOn", " b"  },
        { "Vintage",    "cutoff", "vintOn",   " Hz" },
        { "Saturation", "sat",    "satOn",    " %"  },
        { "Crack",      "crack",  "crackOn",  " %"  },
        { "Punch",      "punch",  "punchOn",  " %"  },
        { "Boom",       "boom",   "boomOn",   " dB" },
        { "Dust",       "dust",   "dustOn",   " %"  },
        { "Room",       "room",   "roomOn",   " %"  },
        { "Sortie",     "output", "",         " dB" }
    };
    for (auto& d : defs)
    {
        auto* cell = new EffectCell (proc, d.label, d.id, d.onId, d.suffix);
        cells.add (cell);
        addAndMakeVisible (cell);
    }

    setSize (760, 560);
}

OctoSnareEditor::~OctoSnareEditor()
{
    setLookAndFeel (nullptr);
}

void OctoSnareEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    auto factory = OctoSnareProcessor::getFactoryPresetNames();
    int id = 1;
    for (auto& n : factory)
        presetBox.addItem (n, id++);
    presetBox.addSeparator();
    for (auto& n : proc.getUserPresetNames())
        presetBox.addItem (n + " (perso)", id++);
}

void OctoSnareEditor::presetSelected()
{
    const int sel = presetBox.getSelectedId();
    if (sel <= 0) return;
    const int numFactory = OctoSnareProcessor::getFactoryPresetNames().size();
    if (sel <= numFactory)
        proc.applyFactoryPreset (sel - 1);
    else
    {
        auto users = proc.getUserPresetNames();
        int idx = sel - numFactory - 1;
        if (idx >= 0 && idx < users.size())
            proc.loadUserPreset (users[idx]);
    }
}

void OctoSnareEditor::paint (juce::Graphics& g)
{
    g.fillAll (octo::bg);
}

void OctoSnareEditor::resized()
{
    auto r = getLocalBounds().reduced (14);

    auto header = r.removeFromTop (30);
    title.setBounds (header.removeFromLeft (150));
    saveButton.setBounds (header.removeFromRight (70));
    header.removeFromRight (6);
    presetName.setBounds (header.removeFromRight (140));
    header.removeFromRight (6);
    presetBox.setBounds (header.removeFromRight (180));

    r.removeFromTop (10);
    viz.setBounds (r.removeFromTop (200));
    r.removeFromTop (12);

    const int cols = 5;
    const int rows = 2;
    const int cw = r.getWidth() / cols;
    const int chh = r.getHeight() / rows;
    for (int i = 0; i < cells.size(); ++i)
    {
        int col = i % cols, row = i / cols;
        cells[i]->setBounds (r.getX() + col * cw, r.getY() + row * chh, cw, chh);
    }
}
