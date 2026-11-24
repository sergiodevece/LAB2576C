#include "PluginEditor.h"
#include "LAB2576CSkin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace vitaplug::lab2576c
{
namespace
{
constexpr int plateWidth = 1760;
constexpr int plateHeight = 716;
constexpr auto pi = juce::MathConstants<float>::pi;

const juce::Colour textInk { 0xfff0f6ef };
const juce::Colour screenInk { 0xff69ffe1 };
const juce::Colour selected { 0xff28f9a4 };
const juce::Colour screenWell { 0xff001409 };

juce::Font rackFont(float height)
{
    static const auto typeface = juce::Typeface::createSystemTypefaceFor(
        Lab2576CSkin::BarlowCondensedSemiBold_ttf,
        static_cast<std::size_t>(Lab2576CSkin::BarlowCondensedSemiBold_ttfSize));
    return juce::Font(juce::FontOptions(typeface).withHeight(height));
}

void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> bounds,
              float height, juce::Colour colour, juce::Justification alignment)
{
    g.setColour(colour);
    g.setFont(rackFont(height));
    g.drawFittedText(text, bounds, alignment, 1);
}

struct KnobLayout
{
    int x, y, radius, valueY, valueWidth, valueHeight;

    juce::Rectangle<int> dial() const { return { x - radius, y - radius, 2 * radius, 2 * radius }; }
    juce::Rectangle<int> value() const
    {
        return { x - valueWidth / 2, valueY - valueHeight / 2, valueWidth, valueHeight };
    }

    juce::Rectangle<int> bounds() const { return dial().expanded(5).getUnion(value()); }
};

// Positions on the approved 1760 × 716 artwork, ordered by PluginParameters.h.
constexpr std::array<KnobLayout, knobCount> layout {{
    { 225, 275, 42, 372,  99, 26 }, // Input
    { 360, 275, 42, 372,  99, 26 }, // Threshold
    { 488, 275, 42, 372,  99, 26 }, // Ratio
    {1304, 275, 42, 372,  99, 26 }, // Attack
    { 463, 597, 50, 510, 111, 26 }, // Release
    {1453, 275, 42, 372,  99, 26 }, // Knee
    {1445, 578, 72, 476, 123, 33 }, // Morph
    { 679, 597, 46, 510, 111, 26 }, // Sidechain high pass
    { 854, 597, 46, 510, 111, 26 }, // Sidechain tilt
    { 202, 633, 35, 574,  93, 26 }, // Link
    {1067, 597, 50, 510, 111, 26 }, // Mix
    {1624, 272, 46, 373,  99, 26 }, // Output
}};

class RackSlider final : public juce::Slider
{
public:
    explicit RackSlider(KnobLayout layoutToUse) : geometry(layoutToUse) {}
    const KnobLayout geometry;
};

juce::Point<float> radial(juce::Point<float> centre, float length, float angle)
{
    return { centre.x + length * std::sin(angle), centre.y - length * std::cos(angle) };
}

class LabLook final : public juce::LookAndFeel_V4
{
public:
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        if (const auto* dial = dynamic_cast<RackSlider*>(&slider))
        {
            const auto origin = dial->geometry.bounds().getPosition();
            return { dial->geometry.dial().translated(-origin.x, -origin.y),
                     dial->geometry.value().translated(-origin.x, -origin.y) };
        }
        return LookAndFeel_V4::getSliderLayout(slider);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(rackFont(slider.getComponentID() == "morph" ? 27.0f : 20.0f));
        label->setBorderSize(juce::BorderSize<int>(0, 3, 0, 3));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::TextEditor::backgroundColourId, screenWell);
        label->setColour(juce::TextEditor::textColourId, screenInk);
        label->setColour(juce::TextEditor::focusedOutlineColourId, selected);
        return label;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        const auto centre = bounds.getCentre();
        const auto radius = 0.5f * std::min(bounds.getWidth(), bounds.getHeight());
        const auto angle = startAngle + position * (endAngle - startAngle);
        // The artwork contains the spun metal and scale. Only the white index moves.
        g.setColour(textInk);
        g.drawLine({ radial(centre, 0.74f * radius, angle), radial(centre, 0.94f * radius, angle) },
                   radius > 58.0f ? 3.25f : 2.7f);

        if (slider.hasKeyboardFocus(true))
        {
            g.setColour(screenInk.withAlpha(0.86f));
            g.drawEllipse(bounds.reduced(0.8f), 1.35f);
        }
    }
};

class RackButton final : public juce::Button
{
public:
    explicit RackButton(const juce::String& name) : Button(name) { setWantsKeyboardFocus(true); }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto on = getToggleState();
        const auto bounds = getLocalBounds();
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(on ? lit : unlit, bounds.toFloat(), juce::RectanglePlacement::stretchToFit);
        drawText(g, getButtonText(), bounds.translated(0, down ? 1 : 0).reduced(8, 8),
                 19.0f, on ? juce::Colour(0xff071f12) : textInk, juce::Justification::centred);
        if (highlighted || hasKeyboardFocus(true))
        {
            g.setColour(screenInk.withAlpha(hasKeyboardFocus(true) ? 0.9f : 0.35f));
            g.drawRoundedRectangle(bounds.toFloat().reduced(5.0f, 7.0f), 3.1f, 1.3f);
        }
    }

private:
    const juce::Image lit = juce::ImageCache::getFromMemory(Lab2576CSkin::button_lit_png, Lab2576CSkin::button_lit_pngSize);
    const juce::Image unlit = juce::ImageCache::getFromMemory(Lab2576CSkin::button_unlit_png, Lab2576CSkin::button_unlit_pngSize);
};

class ScreenTab final : public juce::Button
{
public:
    explicit ScreenTab(const juce::String& name) : Button(name) { setWantsKeyboardFocus(true); }

    void paintButton(juce::Graphics& g, bool highlighted, bool) override
    {
        const auto active = getToggleState();
        const auto bounds = getLocalBounds().toFloat().reduced(0.9f);
        g.setColour(active ? selected : screenWell);
        g.fillRoundedRectangle(bounds, 2.5f);
        g.setColour((active || highlighted) ? selected : juce::Colour(0xff127e55));
        g.drawRoundedRectangle(bounds, 2.5f, hasKeyboardFocus(true) ? 2.1f : 1.2f);
        if (hasKeyboardFocus(true) && active)
        {
            g.setColour(screenWell);
            g.drawRoundedRectangle(bounds.reduced(3.0f), 1.4f, 1.0f);
        }
        drawText(g, getButtonText(), getLocalBounds(), 21.0f,
                 active ? screenWell : screenInk, juce::Justification::centred);
    }
};

class LabScreen final : public juce::Component
{
public:
    LabScreen()
    {
        addAndMakeVisible(levelsTab);
        addAndMakeVisible(grTab);
        levelsTab.setTitle("Input and output history");
        grTab.setTitle("Gain reduction history");
        levelsTab.onClick = [this] { select(false); };
        grTab.onClick = [this] { select(true); };
        select(true);
    }

    void resized() override
    {
        levelsTab.setBounds(26, 19, 154, 40);
        grTab.setBounds(195, 19, 126, 40);
    }

    void capture(MeterLevels levels, std::uint32_t generation)
    {
        if (count != 0 && generation == lastGeneration)
            return;

        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (count != 0 && now - sampleAt(count - 1).time > historyMs)
            count = 0;

        samples[next] = { now, juce::Decibels::gainToDecibels(levels.inputPeak, -90.0f),
                          juce::Decibels::gainToDecibels(levels.outputPeak, -90.0f), levels.reductionDb };
        next = (next + 1) % samples.size();
        count = std::min(count + 1, samples.size());
        lastGeneration = generation;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto front = count == 0 ? Sample {} : sampleAt(count - 1);
        drawReadout(g, front);
        drawBar(g, 517, front.inDb, false);
        drawBar(g, 555, front.outDb, false);
        drawBar(g, 593, front.grDb, true);

        auto span = showGr ? 12.0f : 60.0f;
        if (showGr && count > 0)
        {
            auto peak = 0.0f;
            for (std::size_t i = 0; i < count; ++i)
                if (front.time - sampleAt(i).time <= historyMs)
                    peak = std::max(peak, sampleAt(i).grDb);

            span = std::max(span, 12.0f * std::ceil(peak / 12.0f));
        }

        drawScale(g, span);
        if (count < 2)
            return;

        juce::Graphics::ScopedSaveState clipped(g);
        g.reduceClipRegion(plot.toNearestInt());
        if (showGr)
            drawTrace(g, front.time, span, Trace::reduction, juce::Colour(0xff2be78c));
        else
        {
            drawTrace(g, front.time, span, Trace::input, juce::Colour(0xff35cf77));
            drawTrace(g, front.time, span, Trace::output, juce::Colour(0xff8bffdc));
        }
    }

private:
    enum class Trace { input, output, reduction };
    struct Sample
    {
        double time = 0.0;
        float inDb = -90.0f;
        float outDb = -90.0f;
        float grDb = 0.0f;
    };

    void select(bool gainReduction)
    {
        showGr = gainReduction;
        levelsTab.setToggleState(!gainReduction, juce::dontSendNotification);
        grTab.setToggleState(gainReduction, juce::dontSendNotification);
        repaint();
    }

    const Sample& sampleAt(std::size_t index) const
    {
        return samples[(next + samples.size() - count + index) % samples.size()];
    }

    static juce::String peakText(float db)
    {
        return db <= -90.0f ? juce::String::charToString(0x2014) : juce::String(db, 1);
    }

    void drawReadout(juce::Graphics& g, Sample sample) const
    {
        if (showGr)
        {
            const auto value = sample.grDb >= 0.05f ? -sample.grDb : 0.0f;
            drawText(g, juce::String(value, 1) + " dB", { 331, 22, 287, 34 },
                     24.0f, screenInk, juce::Justification::centredRight);
        }
        else
        {
            const auto reading = "IN " + peakText(sample.inDb) + "   OUT " + peakText(sample.outDb) + " dBFS";
            drawText(g, reading, { 331, 22, 287, 34 }, 19.0f,
                     screenInk, juce::Justification::centredRight);
        }
    }

    static float meterFraction(float attenuation)
    {
        constexpr std::array<float, 5> marks { 0.0f, 6.0f, 12.0f, 24.0f, 48.0f };
        const auto db = juce::jlimit(0.0f, marks.back(), attenuation);
        for (std::size_t i = 1; i < marks.size(); ++i)
            if (db <= marks[i])
                return 0.25f * (static_cast<float>(i - 1) + (db - marks[i - 1]) / (marks[i] - marks[i - 1]));

        return 1.0f;
    }

    static void drawBar(juce::Graphics& g, int left, float db, bool reduction)
    {
        constexpr int segments = 22;
        const auto extent = meterFraction(reduction ? db : -db);
        const auto active = juce::roundToInt(segments * (reduction ? extent : 1.0f - extent));
        const auto first = reduction ? 0 : segments - active;
        const auto end = reduction ? active : segments;
        for (int i = first; i < end; ++i)
        {
            g.setColour(!reduction && i < 5 ? juce::Colour(0xffa2ee55) : juce::Colour(0xff39f47b));
            g.fillRoundedRectangle(static_cast<float>(left + 3), static_cast<float>(95 + i * 7),
                                   13.0f, 4.1f, 0.43f);
        }
    }

    void drawScale(juce::Graphics& g, float span) const
    {
        const auto colour = juce::Colour(0xff59be9e);
        drawText(g, "0", { 7, 86, 35, 17 }, 14.0f, colour, juce::Justification::centredRight);
        drawText(g, juce::String(-span * 0.5f, 0), { 4, 173, 38, 17 },
                 14.0f, colour, juce::Justification::centredRight);
        drawText(g, juce::String(-span, 0), { 4, 263, 38, 17 },
                 14.0f, colour, juce::Justification::centredRight);
    }

    void drawTrace(juce::Graphics& g, double rightTime, float span, Trace trace, juce::Colour colour) const
    {
        const auto area = plot.reduced(2.0f, 5.0f);
        juce::Path path;
        float leftX = 0.0f;
        float rightX = 0.0f;
        bool first = true;
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& sample = sampleAt(i);
            const auto age = rightTime - sample.time;
            if (age > historyMs)
                continue;

            const auto x = area.getRight() - area.getWidth() * static_cast<float>(age / historyMs);
            const auto db = trace == Trace::reduction ? sample.grDb : -(trace == Trace::input ? sample.inDb : sample.outDb);
            const auto y = area.getY() + area.getHeight() * juce::jlimit(0.0f, 1.0f, db / span);
            if (first)
            {
                leftX = x;
                path.startNewSubPath(x, y);
                first = false;
            }
            else
            {
                path.lineTo(x, y);
            }

            rightX = x;
        }

        if (first)
            return;

        if (trace == Trace::reduction)
        {
            auto fill = path;
            fill.lineTo(rightX, plot.getY());
            fill.lineTo(leftX, plot.getY());
            fill.closeSubPath();
            g.setColour(juce::Colour(0xff056b35).withAlpha(0.42f));
            g.fillPath(fill);
        }

        g.setColour(colour.withAlpha(0.27f));
        g.strokePath(path, juce::PathStrokeType(3.75f));
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(1.35f));
    }

    static constexpr double historyMs = 2000.0;
    const juce::Rectangle<float> plot { 51.0f, 87.0f, 411.0f, 189.0f };
    ScreenTab levelsTab { "IN/OUT" };
    ScreenTab grTab { "GR" };
    std::array<Sample, 90> samples {};
    std::size_t next = 0;
    std::size_t count = 0;
    std::uint32_t lastGeneration = 0;
    bool showGr = true;
};
}

class Editor::Plate final : public juce::Component
{
public:
    explicit Plate(juce::AudioProcessorValueTreeState& state)
    {
        setOpaque(true);
        setLookAndFeel(&look);
        for (std::size_t i = 0; i < knobs.size(); ++i)
        {
            const auto& spec = knobs[i];
            sliders[i] = std::make_unique<RackSlider>(layout[i]);
            auto& slider = *sliders[i];
            const auto name = juce::String::fromUTF8(spec.hostLabel);
            slider.setName(name);
            slider.setTitle(name);
            slider.setComponentID(spec.id);
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters(1.25f * pi, 2.75f * pi, true);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, layout[i].valueWidth, layout[i].valueHeight);
            slider.setMouseDragSensitivity(170);
            slider.setWantsKeyboardFocus(true);
            slider.setColour(juce::Slider::textBoxTextColourId, screenInk);
            slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible(slider);
            sliderAttachments[i] = std::make_unique<SliderAttachment>(state, spec.id, slider);
            slider.setDoubleClickReturnValue(true, spec.initial);
        }

        sliders[morph]->setTooltip("0% VCA, 100% FET. Both detectors keep running during the transition.");
        sliders[hpf]->setTooltip("High-pass filter for the internal detector.");
        sliders[tilt]->setTooltip("Reduce the low-frequency weight of the internal detector.");
        sliders[link]->setTooltip("At 100%, both channels receive the same gain reduction.");

        addAndMakeVisible(forward);
        addAndMakeVisible(feedback);
        addAndMakeVisible(effectIn);
        addAndMakeVisible(screen);
        forward.setTitle("VCA feed-forward");
        feedback.setTitle("VCA feedback");
        effectIn.setTitle("Compressor in");
        effectIn.setTooltip("Enable compression. When unlit, the compressor is bypassed.");

        modeAttachment = std::make_unique<juce::ParameterAttachment>(
            *state.getParameter(vcaModeId), [this](float value)
            {
                const auto isFeedback = value >= 0.5f;
                forward.setToggleState(!isFeedback, juce::dontSendNotification);
                feedback.setToggleState(isFeedback, juce::dontSendNotification);
            });
        auto* bypassParameter = state.getParameter(bypassId);
        bypassAttachment = std::make_unique<juce::ParameterAttachment>(
            *bypassParameter, [this](float value)
            {
                effectIn.setToggleState(value < 0.5f, juce::dontSendNotification);
            });
        forward.onClick = [this] { modeAttachment->setValueAsCompleteGesture(0.0f); };
        feedback.onClick = [this] { modeAttachment->setValueAsCompleteGesture(1.0f); };
        effectIn.onClick = [this, bypassParameter]
        {
            bypassAttachment->setValueAsCompleteGesture(bypassParameter->getValue() >= 0.5f ? 0.0f : 1.0f);
        };
        modeAttachment->sendInitialUpdate();
        bypassAttachment->sendInitialUpdate();
        setSize(plateWidth, plateHeight);
    }

    ~Plate() override { setLookAndFeel(nullptr); }

    void setLevels(MeterLevels levels, std::uint32_t generation) { screen.capture(levels, generation); }

    void resized() override
    {
        for (auto& slider : sliders)
            slider->setBounds(slider->geometry.bounds());

        effectIn.setBounds(56, 270, 110, 60);
        forward.setBounds(70, 463, 133, 60);
        feedback.setBounds(203, 463, 132, 60);
        screen.setBounds(557, 42, 642, 309);
    }

    void paint(juce::Graphics& g) override
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(faceplate, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    const juce::Image faceplate = juce::ImageCache::getFromMemory(Lab2576CSkin::lab_panel_png, Lab2576CSkin::lab_panel_pngSize);
    LabLook look;
    std::array<std::unique_ptr<RackSlider>, knobCount> sliders;
    RackButton forward { "FEED-FWD" };
    RackButton feedback { "FEEDBACK" };
    RackButton effectIn { "IN" };
    LabScreen screen;
    std::array<std::unique_ptr<SliderAttachment>, knobCount> sliderAttachments;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> bypassAttachment;
};

Editor::Editor(Processor& owner)
    : AudioProcessorEditor(owner), ownerProcessor(owner), plate(std::make_unique<Plate>(owner.parameters()))
{
    const auto width = ownerProcessor.editorWidth();
    const auto heightFor = [](int w) { return juce::roundToInt(static_cast<double>(w) * plateHeight / plateWidth); };
    addAndMakeVisible(*plate);
    setResizable(true, true);
    setResizeLimits(minimumEditorWidth, heightFor(minimumEditorWidth), maximumEditorWidth, heightFor(maximumEditorWidth));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(static_cast<double>(plateWidth) / plateHeight);
    setSize(width, heightFor(width));
    startTimerHz(30);
}

Editor::~Editor() { stopTimer(); }

void Editor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff070c08)); }

void Editor::resized()
{
    ownerProcessor.setEditorWidth(getWidth());
    const auto scale = juce::jmin(static_cast<float>(getWidth()) / plateWidth,
                                  static_cast<float>(getHeight()) / plateHeight);
    const auto x = 0.5f * (static_cast<float>(getWidth()) - plateWidth * scale);
    const auto y = 0.5f * (static_cast<float>(getHeight()) - plateHeight * scale);
    plate->setTransform(juce::AffineTransform::scale(scale).translated(x, y));
}

void Editor::timerCallback()
{
    const auto generation = ownerProcessor.meterGeneration();
    plate->setLevels(ownerProcessor.meterLevels(), generation);
}
}
