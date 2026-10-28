#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

ArmNongMuangLA2AAudioProcessorEditor::ArmNongMuangLA2AAudioProcessorEditor(ArmNongMuangLA2AAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p) {
    background = juce::ImageCache::getFromMemory(BinaryData::LA2A_Interface_png, BinaryData::LA2A_Interface_pngSize);
    setSize(1152, 768);
    setResizable(true, true);
    setResizeLimits(768, 512, 1536, 1024);
    getConstrainer()->setFixedAspectRatio(1.5);

    setupRotary(gain, "GAIN"); setupRotary(peak, "PEAK REDUCTION");
    setupRotary(attack, "ATTACK"); setupRotary(release, "RELEASE");
    addAndMakeVisible(gain); addAndMakeVisible(peak); addAndMakeVisible(attack); addAndMakeVisible(release);
    gain.setRange(0, 100, 0.1); peak.setRange(0, 100, 0.1); attack.setRange(0.1, 10, 0.01); release.setRange(50, 5000, 1);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "gain", gain);
    peakAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "peak", peak);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "attack", attack);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "release", release);

    addHitButton(topPower, "top power"); topPower.setTooltip("Power: ON / OFF"); topPower.onClick = [this] { setPower(!powerOn); };
    addHitButton(preset, "preset menu"); preset.onClick = [this] {
        juce::PopupMenu menu; menu.addItem(1, "Default"); menu.addItem(2, "Save current settings to A"); menu.addItem(3, "Save current settings to B");
        menu.showMenuAsync(juce::PopupMenu::Options(), [this](int id) { if (id == 1) { processor.parameters.getParameter("gain")->setValueNotifyingHost(0.5f); processor.parameters.getParameter("peak")->setValueNotifyingHost(0.35f); } else if (id == 2) { saveSnapshot(0); status.setText("Current settings saved to A", juce::dontSendNotification); } else if (id == 3) { saveSnapshot(1); status.setText("Current settings saved to B", juce::dontSendNotification); } });
    };
    addHitButton(modelPicker, "load NAM model"); modelPicker.onClick = [this] { loadModelDialog(); };
    addHitButton(buttonA, "snapshot A"); buttonA.onClick = [this] { restoreSnapshot(0); };
    addHitButton(buttonB, "snapshot B"); buttonB.onClick = [this] { restoreSnapshot(1); };
    addHitButton(copyAB, "copy A to B"); copyAB.onClick = [this] { saveSnapshot(0); saveSnapshot(1); status.setText("Snapshot A copied to B", juce::dontSendNotification); };
    addHitButton(settings, "settings"); settings.onClick = [this] {
        juce::PopupMenu menu; menu.addItem(1, "Reset controls"); menu.addItem(2, "Unload NAM model");
        menu.showMenuAsync(juce::PopupMenu::Options(), [this](int id) {
            if (id == 1) { processor.parameters.getParameter("gain")->setValueNotifyingHost(0.5f); processor.parameters.getParameter("peak")->setValueNotifyingHost(0.35f); }
            if (id == 2) { processor.unloadNamFile(); status.setText(processor.getNamStatus(), juce::dontSendNotification); repaint(); }
        });
    };
    addHitButton(meterSwitch, "meter mode");
    meterSwitch.setLookAndFeel(&switchLookAndFeel);
    meterSwitch.setClickingTogglesState(true);
    meterSwitch.setTooltip("Pull UP: OUTPUT | Pull DOWN: GAIN REDUCTION");
    meterSwitch.onClick = [this] { cycleMeter(); };
    addHitButton(bottomPower, "bottom power switch");
    bottomPower.setLookAndFeel(&switchLookAndFeel);
    bottomPower.setClickingTogglesState(true);
    bottomPower.setTooltip("Pull UP: ON | Pull DOWN: OFF");
    bottomPower.onClick = [this] { setPower(!powerOn); bottomPower.setToggleState(powerOn, juce::dontSendNotification); };
    addHitButton(limitSwitch, "limit compress switch");
    limitSwitch.setLookAndFeel(&switchLookAndFeel);
    limitSwitch.setClickingTogglesState(true);
    limitSwitch.setTooltip("Pull UP: LIMIT | Pull DOWN: COMPRESS");
    limitSwitch.onClick = [this] { setLimit(!limitOn); limitSwitch.setToggleState(limitOn, juce::dontSendNotification); };

    addAndMakeVisible(status); status.setColour(juce::Label::textColourId, juce::Colours::black);
    status.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    status.setJustificationType(juce::Justification::centred);
    status.setText(processor.getNamStatus(), juce::dontSendNotification);
    powerOn = processor.parameters.getRawParameterValue("power")->load() >= 0.5f;
    limitOn = processor.parameters.getRawParameterValue("limit")->load() >= 0.5f;
    bottomPower.setToggleState(powerOn, juce::dontSendNotification);
    limitSwitch.setToggleState(limitOn, juce::dontSendNotification);
    meterSwitch.setToggleState(processor.getMeterMode() == 1, juce::dontSendNotification);
    saveSnapshot(0); saveSnapshot(1);
    startTimerHz(30);
}

ArmNongMuangLA2AAudioProcessorEditor::~ArmNongMuangLA2AAudioProcessorEditor() {
    for (auto* b : { &topPower, &preset, &modelPicker, &buttonA, &buttonB, &copyAB, &settings, &meterSwitch, &bottomPower, &limitSwitch }) b->setLookAndFeel(nullptr);
    for (auto* s : { &gain, &peak, &attack, &release }) s->setLookAndFeel(nullptr);
}

void ArmNongMuangLA2AAudioProcessorEditor::setupRotary(juce::Slider& slider, const juce::String& name) {
    slider.setName(name); slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setLookAndFeel(&pointerLookAndFeel);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
}
void ArmNongMuangLA2AAudioProcessorEditor::addHitButton(juce::TextButton& b, const juce::String& name) {
    b.setName(name); b.setButtonText(juce::String{}); b.setLookAndFeel(&invisibleLookAndFeel);
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    b.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    b.setColour(juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    addAndMakeVisible(b);
}
void ArmNongMuangLA2AAudioProcessorEditor::SwitchLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hovered, bool down) {
    // Draw a real, visibly moving two-position toggle lever over the PNG artwork.
    // Toggle state semantics: true = UP, false = DOWN.
    const auto r = b.getLocalBounds().toFloat();
    const bool up = b.getToggleState();
    const bool isMeter = b.getName() == "meter mode";
    const bool isPower = b.getName() == "bottom power switch";

    // Cover the static switch printed in the background image, but keep the
    // original faceplate around it untouched.
    g.setColour(juce::Colour(0xfff2f1ec));
    g.fillRoundedRectangle(r.reduced(1.0f), 3.0f);
    g.setColour(juce::Colour(0xff8a8983));
    g.drawRoundedRectangle(r.reduced(1.0f), 3.0f, 1.0f);

    const float trackW = juce::jmax(13.0f, r.getWidth() * 0.34f);
    const float trackH = r.getHeight() * 0.58f;
    auto track = juce::Rectangle<float>(r.getCentreX() - trackW * 0.5f,
        r.getCentreY() - trackH * 0.5f, trackW, trackH);
    g.setColour(juce::Colour(0xff292b2d));
    g.fillRoundedRectangle(track, trackW * 0.18f);
    g.setColour(juce::Colour(0xff777777));
    g.drawRoundedRectangle(track, trackW * 0.18f, 1.0f);

    // The lever pivots in the centre and visibly tilts toward the selected end.
    const juce::Point<float> pivot(track.getCentreX(), track.getCentreY() + trackH * 0.07f);
    const float leverLength = trackH * 0.37f;
    const float tilt = up ? -0.27f : 0.27f;
    const juce::Point<float> tip(pivot.x + std::sin(tilt) * leverLength,
        pivot.y + (up ? -1.0f : 1.0f) * std::cos(tilt) * leverLength);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawLine(pivot.x + 1.2f, pivot.y + 1.2f, tip.x + 1.2f, tip.y + 1.2f, trackW * 0.23f);
    g.setColour(down ? juce::Colour(0xffb9b9b5) : juce::Colour(0xffe6e5df));
    g.drawLine(pivot.x, pivot.y, tip.x, tip.y, trackW * 0.19f);

    const float ballD = juce::jmax(6.0f, trackW * 0.52f);
    auto ball = juce::Rectangle<float>(tip.x - ballD * 0.5f, tip.y - ballD * 0.5f, ballD, ballD);
    g.setColour(juce::Colour(0xffd8d8d3));
    g.fillEllipse(ball);
    g.setColour(juce::Colour(0xff343434));
    g.drawEllipse(ball, 1.1f);
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.fillEllipse(ball.withSizeKeepingCentre(ballD * 0.22f, ballD * 0.22f)
        .translated(-ballD * 0.12f, -ballD * 0.12f));
    g.setColour(juce::Colour(0xff151515));
    g.fillEllipse(pivot.x - 2.2f, pivot.y - 2.2f, 4.4f, 4.4f);

    juce::String upper, lower;
    if (isMeter) { upper = "OUTPUT"; lower = "GAIN REDUCTION"; }
    else if (isPower) { upper = "ON"; lower = "OFF"; }
    else { upper = "LIMIT"; lower = "COMPRESS"; }
    const float labelSize = juce::jlimit(7.0f, 10.0f, r.getWidth() * 0.12f);
    g.setFont(juce::Font(labelSize, juce::Font::bold));
    g.setColour(juce::Colour(0xff282828));
    g.drawFittedText(upper, juce::Rectangle<int>(juce::roundToInt(r.getX()), juce::roundToInt(r.getY() + 1.0f),
        juce::roundToInt(r.getWidth()), juce::roundToInt(track.getY() - r.getY() - 1.0f)), juce::Justification::centred, 1);
    g.drawFittedText(lower, juce::Rectangle<int>(juce::roundToInt(r.getX()), juce::roundToInt(track.getBottom()),
        juce::roundToInt(r.getWidth()), juce::roundToInt(r.getBottom() - track.getBottom() - 1.0f)), juce::Justification::centred, 1);

    if (hovered) {
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRoundedRectangle(r.reduced(1.0f), 3.0f);
    }
}

void ArmNongMuangLA2AAudioProcessorEditor::PointerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) {
    juce::ignoreUnused(start, end);
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float angle = juce::MathConstants<float>::pi * (2.0f / 3.0f + (5.0f / 3.0f) * pos);
    const float radius = juce::jmin(w, h) * 0.31f;
    const float ex = cx + std::cos(angle) * radius, ey = cy + std::sin(angle) * radius;
    g.setColour(juce::Colours::black.withAlpha(0.85f)); g.drawLine(cx + 1.2f, cy + 1.2f, ex + 1.2f, ey + 1.2f, 5.0f);
    g.setColour(juce::Colours::white); g.drawLine(cx, cy, ex, ey, 2.4f);
    g.setColour(juce::Colours::black); g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
}

void ArmNongMuangLA2AAudioProcessorEditor::setPower(bool on) {
    powerOn = on; processor.parameters.getParameter("power")->setValueNotifyingHost(on ? 1.0f : 0.0f); repaint();
}
void ArmNongMuangLA2AAudioProcessorEditor::setLimit(bool on) {
    limitOn = on; processor.parameters.getParameter("limit")->setValueNotifyingHost(on ? 1.0f : 0.0f); repaint();
}
void ArmNongMuangLA2AAudioProcessorEditor::cycleMeter() {
    // Two-position LA-2A meter switch: DOWN = Gain Reduction, UP = Output.
    // AudioParameterChoice stores choice indices as normalized values: index / (numChoices - 1).
    const float normalized = processor.parameters.getParameter("meter")->getValue();
    const int currentIndex = juce::jlimit(0, 2, juce::roundToInt(normalized * 2.0f));
    const int nextIndex = (currentIndex == 1) ? 2 : 1;
    processor.parameters.getParameter("meter")->setValueNotifyingHost(static_cast<float>(nextIndex) / 2.0f);
    meterSwitch.setToggleState(nextIndex == 1, juce::dontSendNotification);
    repaint();
}
void ArmNongMuangLA2AAudioProcessorEditor::loadModelDialog() {
    chooser = std::make_unique<juce::FileChooser>("Choose NAM model", juce::File{}, "*.nam");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto file = fc.getResult(); if (!file.existsAsFile()) return;
            juce::String message;
            processor.loadNamFile(file, message);
            status.setText(message, juce::dontSendNotification); repaint();
        });
}
void ArmNongMuangLA2AAudioProcessorEditor::saveSnapshot(int slot) {
    auto state = processor.parameters.copyState();
    if (slot == 0) snapshotA = state.createCopy(); else snapshotB = state.createCopy();
}
void ArmNongMuangLA2AAudioProcessorEditor::restoreSnapshot(int slot) {
    const auto& state = (slot == 0 ? snapshotA : snapshotB);
    if (state.isValid()) processor.parameters.replaceState(state.createCopy());
    powerOn = processor.parameters.getRawParameterValue("power")->load() >= 0.5f;
    limitOn = processor.parameters.getRawParameterValue("limit")->load() >= 0.5f;
    bottomPower.setToggleState(powerOn, juce::dontSendNotification);
    limitSwitch.setToggleState(limitOn, juce::dontSendNotification);
    meterSwitch.setToggleState(processor.getMeterMode() == 1, juce::dontSendNotification);
}
void ArmNongMuangLA2AAudioProcessorEditor::timerCallback() {
    powerOn = processor.parameters.getRawParameterValue("power")->load() >= 0.5f;
    limitOn = processor.parameters.getRawParameterValue("limit")->load() >= 0.5f;
    bottomPower.setToggleState(powerOn, juce::dontSendNotification);
    limitSwitch.setToggleState(limitOn, juce::dontSendNotification);
    meterSwitch.setToggleState(processor.getMeterMode() == 1, juce::dontSendNotification);
    repaint();
}

void ArmNongMuangLA2AAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::white);
    if (background.isValid()) g.drawImage(background, getLocalBounds().toFloat());
    const float sx = getWidth() / 1536.0f, sy = getHeight() / 1024.0f;
    // Actual moving VU needle: scale is calibrated to the -20 ... +3 markings in the supplied PNG.
    const float db = juce::jlimit(-20.0f, 3.0f, processor.getVuValueDb());
    const float norm = (db + 20.0f) / 23.0f;
    const float angle = -2.74f + norm * 2.21f;
    const float cx = 790.0f * sx, cy = 548.0f * sy, radius = 139.0f * sx;
    const float ex = cx + std::cos(angle) * radius, ey = cy + std::sin(angle) * radius;
    g.setColour(juce::Colours::black.withAlpha(0.65f)); g.drawLine(cx + 1.2f*sx, cy + 1.2f*sy, ex + 1.2f*sx, ey + 1.2f*sy, 5.0f*sx);
    g.setColour(juce::Colour(0xff17120b)); g.drawLine(cx, cy, ex, ey, 3.0f*sx);
    g.setColour(juce::Colour(0xffb8a16f)); g.fillEllipse(cx - 4.5f*sx, cy - 4.5f*sy, 9.0f*sx, 9.0f*sy);
    // A live status line sits in the white margin below the faceplate and does not alter the PNG artwork.
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    juce::String meterLabel = processor.getMeterMode() == 0 ? "INPUT" : (processor.getMeterMode() == 1 ? "OUTPUT" : "GAIN REDUCTION");
    const juce::String live = processor.isNamLoaded() ? ("NAM: " + processor.getLoadedNamName()) : "BUILT-IN COMPRESSOR";
    g.setFont(juce::Font(12.0f * sx, juce::Font::bold));
    g.drawFittedText(live + "  |  METER: " + meterLabel + "  |  GR: " + juce::String(processor.getGainReductionDb(), 1) + " dB",
        juce::Rectangle<int>(static_cast<int>(540*sx), static_cast<int>(958*sy), static_cast<int>(600*sx), static_cast<int>(24*sy)), juce::Justification::centred, 1);
    // Power indicator over the lamp printed in the reference PNG.
    g.setColour(powerOn ? juce::Colour(0xffffb52e) : juce::Colour(0xff777777));
    g.fillEllipse(1213*sx, 783*sy, 30*sx, 30*sy);
    g.setColour(juce::Colours::white.withAlpha(powerOn ? 0.75f : 0.25f));
    g.fillEllipse(1220*sx, 788*sy, 10*sx, 10*sy);
}

void ArmNongMuangLA2AAudioProcessorEditor::resized() {
    const float sx = getWidth() / 1536.0f, sy = getHeight() / 1024.0f;
    auto place = [&](juce::Component& c, float x, float y, float w, float h) {
        c.setBounds(juce::roundToInt(x*sx), juce::roundToInt(y*sy), juce::roundToInt(w*sx), juce::roundToInt(h*sy));
    };
    // Hit areas are registered against the original 1536 x 1024 PNG coordinate space.
    place(gain, 240, 432, 152, 152);       // Gain knob centre ~316,508
    place(peak, 1119, 432, 152, 152);      // Peak Reduction centre ~1195,508
    place(attack, 558, 722, 105, 105);     // Attack centre ~610,774
    place(release, 843, 722, 105, 105);    // Release centre ~895,774
    place(topPower, 28, 108, 58, 44);
    place(preset, 318, 108, 444, 44);
    place(modelPicker, 782, 108, 350, 44);
    place(buttonA, 1161, 108, 53, 44);
    place(buttonB, 1217, 108, 56, 44);
    place(copyAB, 1280, 108, 96, 44);
    place(settings, 1464, 108, 48, 44);
    place(meterSwitch, 166, 740, 77, 80);
    place(bottomPower, 1350, 758, 68, 82);
    place(limitSwitch, 1412, 461, 68, 89);
    place(status, 530, 925, 650, 26);
}
