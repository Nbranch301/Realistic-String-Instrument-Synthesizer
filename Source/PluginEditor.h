/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/


class CustomLook : public juce::LookAndFeel_V4
{
public:

    juce::Colour trackFillClr       {85, 111, 59};
    juce::Colour trackBackgroundClr {102, 71, 43};
    juce::Colour outerBackgroundClr {90, 59, 31};
    juce::Colour thumbClr           {203, 185, 157};
    juce::Colour backgroundClr      {139, 99, 65};

    CustomLook()
    {
        setColour(juce::Slider::thumbColourId, thumbClr);
        setColour(juce::Slider::backgroundColourId, trackBackgroundClr);
        setColour(juce::Slider::trackColourId, trackFillClr);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        
        setColour(juce::PopupMenu::backgroundColourId, trackBackgroundClr);
        setColour(juce::PopupMenu::textColourId, thumbClr);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, outerBackgroundClr);
        setColour(juce::PopupMenu::highlightedTextColourId, thumbClr);
        
        setColour(juce::ComboBox::backgroundColourId, trackBackgroundClr);
        setColour(juce::ComboBox::textColourId, thumbClr);
        setColour(juce::ComboBox::outlineColourId, outerBackgroundClr);
        setColour(juce::ComboBox::arrowColourId, thumbClr);
    }

    // --- BACKGROUND COLOUR --- //
    juce::Colour getBackgroundColour() { return backgroundClr; }

    // --- LABELS --- //
    void setLabelForSlider(const juce::Slider* slider, const juce::String& labelText)
    {
        labels.set(slider, labelText);
    }

    
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox& box,
                                                             juce::Label& label) override
    {
        return LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label)
            .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards)
            .withTargetComponent(&box);
    }
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos,
                          float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        constexpr float labelHeight = 20.0f;
        constexpr float valueHeight = 20.0f;

        auto bounds = juce::Rectangle<float>(x, y, width, height);

        auto labelArea = bounds.removeFromTop(labelHeight);
        auto valueArea = bounds.removeFromBottom(valueHeight);
        auto knobArea  = bounds;

        auto radius = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) * 0.5f - 2.0f;
        auto centre = knobArea.getCentre();

        float angle = startAngle + sliderPos * (endAngle - startAngle);

        drawKnobBackground(g, centre, radius);
        drawKnobArc(g, centre, radius, startAngle, angle);
        drawKnobPointer(g, centre, radius, angle);
        drawKnobText(g, slider, labelArea, valueArea);
    }
    
    void drawKnobBackground(juce::Graphics& g,
                            juce::Point<float> centre,
                            float radius)
    {
        constexpr float circleMargin = 5.0f;

        g.setColour(outerBackgroundClr);
        g.fillEllipse(centre.x - radius,
                      centre.y - radius,
                      radius * 2,
                      radius * 2);

        g.setColour(trackBackgroundClr);
        g.fillEllipse(centre.x - radius + circleMargin * 0.5f,
                      centre.y - radius + circleMargin * 0.5f,
                      radius * 2 - circleMargin,
                      radius * 2 - circleMargin);
    }
    
    void drawKnobArc(juce::Graphics& g,
                     juce::Point<float> centre,
                     float radius,
                     float startAngle,
                     float angle)
    {
        juce::Path arc;

        arc.addCentredArc(centre.x,
                          centre.y,
                          radius - 6.0f,
                          radius - 6.0f,
                          0.0f,
                          startAngle,
                          angle,
                          true);

        g.setColour(trackFillClr);
        g.strokePath(arc, juce::PathStrokeType(3.0f));
    }
    
    void drawKnobPointer(juce::Graphics& g,
                         juce::Point<float> centre,
                         float radius,
                         float angle)
    {
        constexpr float pointerThickness = 3.0f;
        float pointerLen = radius - 10.0f;

        juce::Path p;

        p.addRectangle(-pointerThickness * 0.5f,
                       -pointerLen,
                       pointerThickness,
                       pointerLen);

        p.applyTransform(
            juce::AffineTransform::rotation(angle)
                .translated(centre.x, centre.y)
        );

        g.setColour(thumbClr);
        g.fillPath(p);
    }
    
    void drawKnobText(juce::Graphics& g,
                      juce::Slider& slider,
                      juce::Rectangle<float> labelArea,
                      juce::Rectangle<float> valueArea)
    {
        if (labels.contains(&slider))
        {
            g.setColour(juce::Colours::white.withAlpha(0.9f));

            g.drawFittedText(labels[&slider],
                             labelArea.toNearestInt(),
                             juce::Justification::centred,
                             1);
        }

        g.setColour(juce::Colours::white);

        g.drawFittedText(slider.getTextFromValue(slider.getValue()),
                         valueArea.toNearestInt(),
                         juce::Justification::centred,
                         1);
    }

private:
    juce::HashMap<const juce::Slider*, juce::String> labels;
};


using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

struct KnobPack {
    juce::Slider slider;
    std::unique_ptr<Attachment> attachment;
    const char* paramId;
    const char* labelText;
    const char* suffix;
};

class MUS307FinalAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    MUS307FinalAudioProcessorEditor (MUS307FinalAudioProcessor&);
    ~MUS307FinalAudioProcessorEditor() override;
    
    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MUS307FinalAudioProcessor& audioProcessor;
    
    CustomLook customLAF;
    juce::ComboBox presetBox;
    
    // Define all knob packs in one place
    std::array<KnobPack, 8> knobs {{
        { {}, {}, PICK_SHARPNESS, "pick shape",     ""                              },
        { {}, {}, ATTACK,         "attack",         "ms"                            },
        { {}, {}, DECAY,          "decay",          "ms"                            },
        { {}, {}, SUSTAIN,        "sustain",        "%"                             },
        { {}, {}, RELEASE,        "release",        "ms"                            },
        { {}, {}, COLOUR,         "bodyColour",     ""                              },
        { {}, {}, BRIGHTNESS,     "brightness",     ""                              },
        { {}, {}, MAX_DETUNE,     "max detune",     "c"                             },
    }};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MUS307FinalAudioProcessorEditor)
    void initKnob(KnobPack& k);
};
