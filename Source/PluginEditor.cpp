/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MUS307FinalAudioProcessorEditor::MUS307FinalAudioProcessorEditor (MUS307FinalAudioProcessor& p)

: AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (700, 700);
    
    setLookAndFeel(&customLAF);
    
    for (auto& k : knobs)
      initKnob(k);
    
    presetBox.setJustificationType(juce::Justification::centred);
    presetBox.setText("Instruments");

    const auto& presets = audioProcessor.getPresets();
    for (int i = 0; i < (int)presets.size(); ++i)
        presetBox.addItem(presets[(size_t)i].name, i + 1);


    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        audioProcessor.applyPreset(idx);
    };
    addAndMakeVisible(presetBox);
}

void MUS307FinalAudioProcessorEditor::initKnob(KnobPack& k)
{
    k.slider.setLookAndFeel(&customLAF);
    
    // Uniform slider look
    k.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setTextValueSuffix(k.suffix);

    customLAF.setLabelForSlider(&k.slider, k.labelText);
    addAndMakeVisible(k.slider);

    // Create parameter attachment
    k.attachment = std::make_unique<Attachment>(audioProcessor.treeState, k.paramId, k.slider);

}

MUS307FinalAudioProcessorEditor::~MUS307FinalAudioProcessorEditor()
{
    for (auto& k : knobs)
        k.slider.setLookAndFeel(nullptr);
    
    setLookAndFeel (nullptr);
}


//==============================================================================
void MUS307FinalAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (customLAF.getBackgroundColour());
}


void MUS307FinalAudioProcessorEditor::resized()
{
    auto margin = 10.0;
    auto bounds = getLocalBounds().reduced(margin);
    
    // --- TOP LEFT QUADRANT --- //
    
    // ATTACK
    auto topLeft = bounds   .withTrimmedRight(3.0 * (bounds.getWidth() / 4.0))
                            .withTrimmedBottom(3.0 * (bounds.getHeight() / 4.0));

    knobs[1].slider.setBounds(topLeft);

    // DECAY
    auto topRight =  bounds .withTrimmedLeft(bounds.getWidth() / 4.0)
                            .withTrimmedRight(bounds.getWidth() / 2.0)
                            .withTrimmedBottom(3.0 * (bounds.getHeight() / 4.0));

    knobs[2].slider.setBounds(topRight);

    // SUSTAIN
    auto btmLeft = bounds   .withTrimmedRight(3.0 * (bounds.getWidth() / 4.0))
                            .withTrimmedTop(bounds.getHeight() / 4.0)
                            .withTrimmedBottom(bounds.getHeight() / 2.0);

    knobs[3].slider.setBounds(btmLeft);

    // RELEASE
    auto btmRight =  bounds .withTrimmedLeft(bounds.getWidth() / 4.0)
                            .withTrimmedRight(bounds.getWidth() / 2.0)
                            .withTrimmedTop(bounds.getHeight() / 4.0)
                            .withTrimmedBottom(bounds.getHeight() / 2.0);

    knobs[4].slider.setBounds(btmRight);

    
    // --- TOP RIGHT QUADRANT --- //
    
    // PICK SHARPNESS
    topLeft =       bounds  .withTrimmedLeft(bounds.getWidth() / 2.0)
                            .withTrimmedRight(bounds.getWidth() / 4.0)
                            .withTrimmedBottom(3.0 * (bounds.getHeight() / 4.0));

    knobs[0].slider.setBounds(topLeft);
 
    // BODY COLOUR
    topRight =      bounds  .withTrimmedLeft(bounds.getWidth() * 0.75)
                            .withTrimmedBottom(3.0 * (bounds.getHeight() / 4.0));

    knobs[5].slider.setBounds(topRight);
    
    // BRIGHTNESS
    btmLeft =       bounds  .withTrimmedLeft(bounds.getWidth() / 2.0)
                            .withTrimmedRight(bounds.getWidth() / 4.0)
                            .withTrimmedTop(bounds.getHeight() / 4.0)
                            .withTrimmedBottom(bounds.getHeight() / 2.0);

    knobs[6].slider.setBounds(btmLeft);
    
    // MAX DETUNE
    btmRight =      bounds  .withTrimmedLeft(3.0 * (bounds.getWidth() / 4.0))
                            .withTrimmedTop(bounds.getHeight() / 4.0)
                            .withTrimmedBottom(bounds.getHeight() / 2.0);


    knobs[7].slider.setBounds(btmRight);
    
    bounds = getLocalBounds();
    auto topBar = bounds.withTrimmedRight(getWidth() / 2.0)
                        .withTrimmedTop(getHeight() / 2.0)
                        .withTrimmedBottom(300.0);
    
    
    presetBox.setBounds(topBar.reduced(10));
}
