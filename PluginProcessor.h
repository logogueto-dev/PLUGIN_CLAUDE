#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class ChoirCrowdProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaxSingers = 32;
    static constexpr int kMaxVoices  = 8;

    ChoirCrowdProcessor();
    ~ChoirCrowdProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Choir Crowd"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Singer
    {
        float phase = 0.f, vibPhase = 0.f, inc = 0.f;
        float detune = 0.f, vibRate = 5.f, vibDepth = 0.f;
        float octMul = 1.f, level = 1.f, panL = 0.7f, panR = 0.7f, env = 0.f;
        int delay = 0;
    };

    struct Voice
    {
        bool active = false, held = false;
        int note = -1, numSingers = 0;
        float vel = 0.f;
        juce::uint64 age = 0;
        std::array<Singer, kMaxSingers> singers;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage&);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void updateIncrements (Voice&, int chunk);
    void renderVoices (juce::AudioBuffer<float>&, int start, int num);
    void applyFx (juce::AudioBuffer<float>&);
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    std::array<Voice, kMaxVoices> voices;
    juce::Random rng;
    juce::uint64 ageCounter = 0;
    double sr = 44100.0;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 3> formant;
    juce::dsp::StateVariableTPTFilter<float> lowCut;
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;
    juce::SmoothedValue<float> vowelSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirCrowdProcessor)
};
