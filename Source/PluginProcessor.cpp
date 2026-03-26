/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SineWave*> (sound) != nullptr;
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    adsr.setSampleRate(getSampleRate());
    
    /*
    Initialize filters to emulate the resonant frequency of the instrument's body.
     
    NOTE: makePeakFilter takes the samplerate, frequency, gain, bandwidth and gain. Bandwidth is wide from 0.1 - 1.0 and narrow as it goes higher.
    */
    headEq1.setCoefficients(juce::IIRCoefficients::makeHighPass(getSampleRate(), freq1, band1));
    headEq2.setCoefficients(juce::IIRCoefficients::makePeakFilter(getSampleRate(), freq2, 3.0, juce::Decibels::decibelsToGain(gain2 * bodyColour)));
    headEq3.setCoefficients(juce::IIRCoefficients::makePeakFilter(getSampleRate(), freq3, 4.0, juce::Decibels::decibelsToGain(gain3 * bodyColour)));
    headEq4.setCoefficients(juce::IIRCoefficients::makePeakFilter(getSampleRate(), freq4, 4.0, juce::Decibels::decibelsToGain(gain4 * bodyColour)));
    headEq5.setCoefficients(juce::IIRCoefficients::makePeakFilter(getSampleRate(), freq5, 4.0, juce::Decibels::decibelsToGain(gain5 * bodyColour)));
    
    // Get frequency (in hz) from MIDI message
    noteFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    
    // Notes will play about 4 cents sharper at full velocity
    float maxDetuneAmount = pow(2.0f, (maxDetune * (velocity))/1200.0f) - 1.0f;
    
    // The number of samples in the buffer determines the pitch of the note. Here, we randomize the frequency in a range that depends
    // on velocity (max velocity: +4 cents, min velocity: no change)
     delaySamples = (getSampleRate() / (noteFrequency * (juce::Random::getSystemRandom().nextFloat() * maxDetuneAmount + 1.0f)));
    
    // Clamp to the max buffer size permitted
    if (delaySamples > maxDelay - 1)
    {
        delaySamples = maxDelay - 1;
    }
    
    // Initialize the location of the read pointer, where we will read the information in the buffer
    readIndex = 0.0f;
    
    // Initialize the location of the write pointer, where we will write the delayed information
    writeIndex = (int)(readIndex + delaySamples) % maxDelay;
    
    // Fill the attack noise ('pluck') part of the buffer.
    for (int i = 0; i < (int)delaySamples; ++i)
    {
        // Random float between -1 and 1 (for noise)
        float n = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        
        // - Position in buffer relative to the end
        // - (ie: if delaySamples = 100 -> i = 0: t = 0, i = 99: t ~= 1
        // - The jmax clamp just prevents division by zero
        float t = (float)i / juce::jmax(1.0f, delaySamples - 1.0f);
        
        float pickEnv = juce::jlimit(0.0f, 1.0f, 1.0f - t * 3.0f);
        
        // Non-linearity makes sound brighter by adding harmonics!
        float nonlinear = n * pow(std::abs(n), 30.0);
        
        // Mix between linearity and non linearity using pickSharpness param
        float shaped = pickSharpness * n + (1.0f - pickSharpness) * nonlinear;
        
        // Mix it all together and store in buffer (with velocity determining loudness)
        delayBuffer[i] = shaped * pickEnv * velocity * 2.0f;
        // delayBuffer[i] = 0.5f;
    }

    // Trigger envelope
    isActive = true;
    
    // Reset KPS algorithm helpers
    lp_z1 = 0.0f;
    avg_z1 = 0.0f;

    // Scale ADSR parameters and apply them to ADSR
    adsrParams.attack = attack / 1000.0f;
    adsrParams.decay = decay / 1000.0f;
    adsrParams.sustain = sustain / 100.0f;
    adsrParams.release = release / 1000.0f;

    adsr.setParameters(adsrParams);
    adsr.noteOn();
    
    {
        float f = (float)noteFrequency;
        float base = 1.0f;
        float freqLoss = juce::jlimit(0.0f, 0.00015f, f * 0.0000000015f);
        damping = juce::jlimit(0.0f, 1.0f, base - freqLoss);
        brightness = juce::jlimit(0.05f, 0.65f, (float) brightFactor + 0.35f * velocity - 0.00004f * f);
    }
}

void SynthVoice::stopNote (float velocity, bool allowTailOff)
{
    adsr.noteOff();
    
    if (!allowTailOff || !adsr.isActive())
    {
        clearCurrentNote();
        isActive = false;
    }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    // Check if the enveloppe is still active
    if (!isActive)
        return;

    // Loop through each sample
    for (int i = 0; i < numSamples; ++i)
    {
        // Read from delay using fractional index (read description above readDelay for more details)
        float delayedSample = readDelay();

        // --- KARPLUS STRONG ALGORITHM! --- //
        
        // - Averages current and previous sample to simulate the energy loss at the bridge and nut
        // - ie: avg = (current + previous) / 2
        
//        auto& random = juce::Random::getSystemRandom();
//
//        float avg = 0.0f;
//        if (random.nextFloat() < 0.5f) {
//            avg = 0.5f * (delayedSample + avg_z1);
//        } else {
//            avg = -0.5f * (delayedSample + avg_z1);
//        }
        float avg = 0.5f * (delayedSample + avg_z1);
        avg_z1 = delayedSample;
        
        // - LP filter that determines the brightness of the sound over time
        // - Mixes the avg with previous lp_z1 sample value
        float y = brightness * avg + (1.0f - brightness) * lp_z1;
        lp_z1 = y;
        
        // global damping
        float filtered = y * damping;
        filtered += 0.0005f * (juce::Random::getSystemRandom().nextFloat() - 0.5f);

        // Write back at fixed offset from read
        delayBuffer[writeIndex] = filtered;

        // Advance read by exactly one sample, wrap
        readIndex += 1.0f;
        if (readIndex >= maxDelay) readIndex -= maxDelay;
        // Keep writeIndex == readIndex + delaySamples (wrapped)
        writeIndex = (int)(readIndex + delaySamples);
        while (writeIndex >= maxDelay) writeIndex -= maxDelay;

        float env = adsr.getNextSample();
        
        float body =    headEq5.processSingleSampleRaw(
                        headEq4.processSingleSampleRaw(
                        headEq3.processSingleSampleRaw(
                        headEq2.processSingleSampleRaw(
                        headEq1.processSingleSampleRaw(filtered)))));

        float output = body * env * (float)outputTrim * 0.8;
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample(ch, startSample, output);

        startSample++;
    }
}

//==============================================================================
MUS307FinalAudioProcessor::MUS307FinalAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
            treeState(*this, nullptr, "PARAMS", createParameterLayout())
#endif
{
    // treeState.state = juce::ValueTree("saveParams");
    presets =
    {
        
        {
            "Acoustic Steel",
            {
                { PICK_SHARPNESS, 0.78f },
                { ATTACK,         1.0f  },
                { DECAY,          220.0f},
                { SUSTAIN,        55.0f },
                { RELEASE,        185.0f},
                { COLOUR,         4.5f  },
                { BRIGHTNESS,     0.133f},
                { MAX_DETUNE,     1.5f  }
            },
            /* freqs = */           { 100.0f, 240.0f, 480.0f, 1800.0f, 3500.0f },
            /* gains = */           { 3.0f, 6.0f, 4.0f, 5.0f, 3.0f },
            /* output gain = */     -6.0f
        },
        {
            "Nylon Guitar",
            {
                { PICK_SHARPNESS, 0.07f },
                { ATTACK,         1.0f  },
                { DECAY,          260.0f},
                { SUSTAIN,        68.0f },
                { RELEASE,        1200.0f},
                { COLOUR,         5.0f  },
                { BRIGHTNESS,     0.042f},
                { MAX_DETUNE,     2.0f  }
            },
            /* freqs = */           { 100.0f, 230.0f, 500.0f, 1600.0f, 3000.0f },
            /* gains = */           { 3.0f, 6.0f, 4.0f, 5.0f, 3.0f },
            /* gain = */            0.0f
        },
        
        {
            "12-String Acoustic",
            {
                { PICK_SHARPNESS, 0.80f },
                { ATTACK,         2.0f  },
                { DECAY,          240.0f},
                { SUSTAIN,        60.0f },
                { RELEASE,        1000.0f},
                { COLOUR,         4.0f  },
                { BRIGHTNESS,     0.130f},
                { MAX_DETUNE,     8.0f  }
            },
            /* freqs = */           { 100.0f, 240.0f, 480.0f, 1900.0f, 3800.0f },
            /* gains = */           { 3.0f, 6.0f, 4.0f, 5.0f, 3.0f },
            /* output gain = */     -8.0f
        },
        
        {
            "Mandolin",
            {
                { PICK_SHARPNESS, 1.0f },
                { ATTACK,         1.0f },
                { DECAY,          153.0f },
                { SUSTAIN,        39.0f },
                { RELEASE,        127.0f },
                { COLOUR,         5.0f },
                { BRIGHTNESS,     0.125f },
                { MAX_DETUNE,     0.0f }
            },
            /* freqs = */           { 100.0f, 350.0f, 480.0f, 1800.0f, 3500.0f },
            /* gains = */           { 3.0f, 4.0f, 4.0f, 5.0f, 3.0f },
            /* output gain = */     -6.0f
        },
        {
            "Banjo",
            {
                { PICK_SHARPNESS, 0.88f },
                { ATTACK,         1.0f },
                { DECAY,          180.0f },
                { SUSTAIN,        65.0f },
                { RELEASE,        490.0f },
                { COLOUR,         5.0f },
                { BRIGHTNESS,     0.120f },
                { MAX_DETUNE,     4.0f }
            },
            /* freqs = */           { 200.0f, 600.0f, 900.0f, 1500.0f, 2800.0f },
            /* gains = */           { 3.0f, 6.0f, 4.0f, 5.0f, 3.0f },
            /* output gain = */     -12.0f
        },
        {
            "Bass",
            {
                { PICK_SHARPNESS, 0.2f },
                { ATTACK,         10.0f },
                { DECAY,          515.0f },
                { SUSTAIN,        27.0f },
                { RELEASE,        588.0f },
                { COLOUR,         3.7f },
                { BRIGHTNESS,     0.0f },
                { MAX_DETUNE,     0.5f }
            },
            /* freqs = */           { 35.0f, 116.0f, 220.0f, 440.0f, 536.0f },
            /* gains = */           { 2.0f, 8.0f, 5.0f, 4.0f, 5.0f },
            /* output gain = */     0.0f
        }
    };

    for (int i = 0; i < 8; ++i)
        synth.addVoice(new SynthVoice());

    synth.addSound(new SineWave());
}

juce::AudioProcessorValueTreeState::ParameterLayout MUS307FinalAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(PICK_SHARPNESS,
                                                    "pickSharpness",
                                                    juce::NormalisableRange<float>
                                                                  (0.0f, 1.0f, 0.01f), 0.6f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(ATTACK,
                                                    "attack",
                                                    juce::NormalisableRange<float>
                                                                  (1.0f, 5000.0f, 1.0f, 0.7f), 1.0f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(DECAY,
                                                    "decay",
                                                    juce::NormalisableRange<float>
                                                                  (3.0f, 5000.0f, 1.0f, 0.7f), 230.0f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(SUSTAIN,
                                                    "sustain",
                                                    juce::NormalisableRange<float>
                                                                  (0.0f, 100.0f, 1.0f, 1.0f), 100.0f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(RELEASE,
                                                    "release",
                                                    juce::NormalisableRange<float>
                                                                  (1.0f, 5000.0f, 1.0f, 0.7f), 931.0f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(COLOUR,
                                                    "bodyColour",
                                                    juce::NormalisableRange<float>
                                                                  (0.0f, 5.0f, 0.1f), 3.5f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(BRIGHTNESS,
                                                    "brightFactor",
                                                    juce::NormalisableRange<float>
                                                                  (0.0f, 0.3f, 0.001f), 0.105f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(MAX_DETUNE,
                                                    "maxDetune",
                                                    juce::NormalisableRange<float>
                                                                  (0.0f, 100.0f, 0.1f, 0.5f), 4.0f));

    return { params.begin(), params.end() };
}

MUS307FinalAudioProcessor::~MUS307FinalAudioProcessor()
{
}

//==============================================================================
const juce::String MUS307FinalAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MUS307FinalAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MUS307FinalAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MUS307FinalAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MUS307FinalAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MUS307FinalAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MUS307FinalAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MUS307FinalAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MUS307FinalAudioProcessor::getProgramName (int index)
{
    return {};
}

void MUS307FinalAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}


void MUS307FinalAudioProcessor::initVoiceParams(const VoiceParams& vp)
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            voice->setParams(vp);
}


//==============================================================================
void MUS307FinalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void MUS307FinalAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MUS307FinalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MUS307FinalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    readAPVTS();
    buffer.clear();
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

void MUS307FinalAudioProcessor::readAPVTS()
{
    VoiceParams vp;
    vp.pickSharpness = *treeState.getRawParameterValue(PICK_SHARPNESS);
    vp.attack        = *treeState.getRawParameterValue(ATTACK);
    vp.decay         = *treeState.getRawParameterValue(DECAY);
    vp.sustain       = *treeState.getRawParameterValue(SUSTAIN);
    vp.release       = *treeState.getRawParameterValue(RELEASE);
    vp.bodyColour    = *treeState.getRawParameterValue(COLOUR);
    vp.brightFactor  = *treeState.getRawParameterValue(BRIGHTNESS);
    vp.maxDetune     = *treeState.getRawParameterValue(MAX_DETUNE);
    
    vp.freq1 = currentBodyFreqs.f1;
    vp.freq2 = currentBodyFreqs.f2;
    vp.freq3 = currentBodyFreqs.f3;
    vp.freq4 = currentBodyFreqs.f4;
    vp.freq5 = currentBodyFreqs.f5;
    
    vp.band1 = currentBodyGains.b1;
    vp.gain2 = currentBodyGains.g2;
    vp.gain3 = currentBodyGains.g3;
    vp.gain4 = currentBodyGains.g4;
    vp.gain5 = currentBodyGains.g5;
    
    vp.outputTrim = currentOutputTrimLinear;

    initVoiceParams(vp);
}

//==============================================================================
bool MUS307FinalAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MUS307FinalAudioProcessor::createEditor()
{
    return new MUS307FinalAudioProcessorEditor (*this);
}

//==============================================================================
void MUS307FinalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml(treeState.state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MUS307FinalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> theParams(getXmlFromBinary(data, sizeInBytes));
    if (theParams != nullptr)
    {
        // If xml has name "saveParams"
        if(theParams -> hasTagName(treeState.state.getType()))
        {
            treeState.state = juce::ValueTree::fromXml(*theParams);
            
            // Restore non-parameter EQ state
            restoreNonParameterState();
        }
    }
}



void MUS307FinalAudioProcessor::applyPreset(int index)
{
    if (index < 0 || index >= (int)presets.size()) return;
    const auto& p = presets[(size_t)index];

    // 1) Public parameters (notify host & update attachments)
    juce::ScopedValueSetter<bool> guard (isNonAutomatedPresetChange, true);
    for (const auto& [paramID, targetValue] : p.values)
        if (auto* param = treeState.getParameter(paramID))
        {
            auto range = param->getNormalisableRange();
            auto norm  = range.convertTo0to1(targetValue);
            param->beginChangeGesture();
            param->setValueNotifyingHost(norm);
            param->endChangeGesture();
        }

    currentBodyFreqs = p.freqs;
    
    currentBodyGains = p.gains;
    
    currentOutputTrimLinear = juce::Decibels::decibelsToGain(p.gainDb);

    auto& s = treeState.state;               // APVTS root ValueTree
    s.setProperty("freq1",  p.freqs.f1, nullptr);
    s.setProperty("freq2",  p.freqs.f2, nullptr);
    s.setProperty("freq3",  p.freqs.f3, nullptr);
    s.setProperty("freq4",  p.freqs.f4, nullptr);
    s.setProperty("freq5",  p.freqs.f5, nullptr);

    s.setProperty("band1",  p.gains.b1, nullptr);
    s.setProperty("gain2",  p.gains.g2, nullptr);
    s.setProperty("gain3",  p.gains.g3, nullptr);
    s.setProperty("gain4",  p.gains.g4, nullptr);
    s.setProperty("gain5",  p.gains.g5, nullptr);

    s.setProperty("outputTrimDb", p.gainDb, nullptr); // store in dB, convert to linear when using
}

void MUS307FinalAudioProcessor::restoreNonParameterState()
{
    auto& s = treeState.state;
    auto get = [&](const juce::Identifier& id, float fallback)
    {
        return s.hasProperty(id) ? (float) s.getProperty(id) : fallback;
    };

    // Fall back to current members so we don’t stomp defaults if properties are absent (older sessions)
    currentBodyFreqs = {
        get("freq1", currentBodyFreqs.f1),
        get("freq2", currentBodyFreqs.f2),
        get("freq3", currentBodyFreqs.f3),
        get("freq4", currentBodyFreqs.f4),
        get("freq5", currentBodyFreqs.f5)
    };

    currentBodyGains = {
        get("band1", currentBodyGains.b1),
        get("gain2", currentBodyGains.g2),
        get("gain3", currentBodyGains.g3),
        get("gain4", currentBodyGains.g4),
        get("gain5", currentBodyGains.g5)
    };

    if (s.hasProperty("outputTrimDb"))
        currentOutputTrimLinear = juce::Decibels::decibelsToGain((float)s.getProperty("outputTrimDb"));
}




//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MUS307FinalAudioProcessor();
}
