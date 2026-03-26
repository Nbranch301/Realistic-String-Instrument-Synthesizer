/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define PICK_SHARPNESS "pickSharpness"
#define ATTACK "attack"
#define DECAY "decay"
#define SUSTAIN "sustain"
#define RELEASE "release"
#define COLOUR "bodyColour"
#define BRIGHTNESS "brightFactor"
#define MAX_DETUNE "maxDetune"

struct Preset
{
    juce::String name;
    std::map<juce::String, float> values;
    
    struct BodyFreqs { float f1, f2, f3, f4, f5; } freqs;
    struct BodyGains { float b1, g2, g3, g4, g5; } gains;
    
    double gainDb { 0.0 };
};

struct VoiceParams
{
    double pickSharpness { 0.6 };
    double attack        { 1.0 };   // ms
    double decay         { 230.0 }; // ms
    double sustain       { 100.0 };    // %
    double release       { 931.0 };   // ms
    double bodyColour    { 3.5 };
    double brightFactor  { 0.105 };
    double maxDetune     { 4.0 };
    
    
    double freq1        { 200.0 };
    double freq2        { 600.0 };
    double freq3        { 900.0 };
    double freq4        { 1500.0 };
    double freq5        { 2800.0 };
    
    double band1        { 3.0 };
    double gain2        { 3.0 };
    double gain3        { 4.0 };
    double gain4        { 4.0 };
    double gain5        { 4.0 };
    
    double outputTrim   { 1.0 };
};

//==============================================================================
/**
*/
class SynthVoice: public juce::SynthesiserVoice
{
    public:
    //==============================================================================

    void setParams(const VoiceParams& p)
    {
        pickSharpness   = p.pickSharpness;
        attack          = p.attack;
        decay           = p.decay;
        sustain         = p.sustain;
        release         = p.release;
        bodyColour      = p.bodyColour;
        brightFactor    = p.brightFactor;
        maxDetune       = p.maxDetune;
        
        freq1 = p.freq1;
        freq2 = p.freq2;
        freq3 = p.freq3;
        freq4 = p.freq4;
        freq5 = p.freq5;
        
        band1 = p.band1;
        gain2 = p.gain2;
        gain3 = p.gain3;
        gain4 = p.gain4;
        gain5 = p.gain5;
        
        outputTrim = p.outputTrim;

    }
    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float velocity, bool allowTailOff) override;
    
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    
    // - Since changing delay length changes pitch, we need to ensure we are calculating non-integer
    // indices to properly approximate pitch, since the number of samples is an integer.
    // - For example, if we are calculating buffer[105.4], we do:
    //
    // -> buffer[105.4] = buffer[105] + 0.4 * (buffer[106] - buffer[105])
    inline float readDelay()
    {
        int indexA = (int)readIndex;
        int indexB = (indexA + 1) % maxDelay;

        float frac = readIndex - indexA;

        float sampleA = delayBuffer[indexA];
        float sampleB = delayBuffer[indexB];

        return sampleA + frac * (sampleB - sampleA);
    }

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    
    private:
    //==============================================================================
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    
    double pickSharpness = 0.5;
    double attack = 10.0;
    double decay = 100.0;
    double sustain = 50.0;
    double release = 30.0;
    double bodyColour = 0.0;
    double brightness = 1.0;
    double brightFactor = 0.0;
    double maxDetune = 4.0;
    double outputTrim = 1.0;
    
    static constexpr int maxDelay = 48000;
    std::array<float, maxDelay> delayBuffer;
    
    float delaySamples = 0.0f;
    int writeIndex = 0;
    float readIndex = 0.0f;
    
    float damping = 0.997f;
    
    double noteFrequency = 440.0;
    float lp_z1 = 0.0f;
    float avg_z1 = 0.0f;
    
    bool isActive = false;
    
    double freq1 = 200.0;
    double freq2 = 600.0;
    double freq3 = 900.0;
    double freq4 = 1500.0;
    double freq5 = 2800.0;
    
    double band1 = 3.0;
    double gain2 = 3.0;
    double gain3 = 4.0;
    double gain4 = 4.0;
    double gain5 = 4.0;
    
//    static constexpr int mandoDelayMax = 4800; // about 10 ms @ 48k
//    std::array<float, mandoDelayMax> mandoDelayBuffer {};
//    int mandoWrite = 0;
//    int mandoDelaySamples = 0;
    
    juce::IIRFilter headEq1, headEq2, headEq3, headEq4, headEq5;
};

class SineWave: public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class MUS307FinalAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MUS307FinalAudioProcessor();
    ~MUS307FinalAudioProcessor() override;
    
    juce::AudioProcessorValueTreeState treeState;

    //==============================================================================
    void initVoiceParams(const VoiceParams& vp);
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void readAPVTS();

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    
    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    void applyPreset(int index);
    void restoreNonParameterState();
    
private:
    std::vector<Preset> presets;
    bool isNonAutomatedPresetChange { false };
    
    juce::Synthesiser synth;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    Preset::BodyFreqs currentBodyFreqs { 200.f, 600.f, 900.f, 1500.f, 2800.f };
    Preset::BodyGains currentBodyGains { 3.f, 3.f, 4.f, 4.f, 4.f };
    float currentOutputTrimLinear { 1.0f };
        
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MUS307FinalAudioProcessor)
};
