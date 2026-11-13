#include "PluginProcessor.h"

namespace
{
    constexpr int kChunk = 32;

    // Formantes (F1, F2, F3) para Ah, Eh, Ee, Oh, Oo
    const float kVowels[5][3] = {
        { 800.f, 1150.f, 2900.f }, { 400.f, 1600.f, 2700.f }, { 350.f, 1700.f, 2700.f },
        { 450.f,  800.f, 2830.f }, { 325.f,  700.f, 2700.f } };
    const float kQ[3]    = { 6.f, 8.f, 10.f };
    const float kGain[3] = { 1.0f, 0.7f, 0.4f };

    inline float polyBlep (float t, float dt)
    {
        if (t < dt)         { t /= dt;           return t + t - t * t - 1.f; }
        if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
}

ChoirCrowdProcessor::ChoirCrowdProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ChoirCrowdProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    using PID = juce::ParameterID;

    auto vowelText = [] (float v, int) {
        static const char* names[] = { "Ah", "Eh", "Ee", "Oh", "Oo" };
        return juce::String (names[juce::jlimit (0, 4, (int) std::round (v))]);
    };

    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterInt> (PID { "crowd", 1 }, "Crowd Size", 4, kMaxSingers, 20));
    l.add (std::make_unique<F> (PID { "vowel", 1 }, "Vowel", NR (0.f, 4.f), 3.f,
                                juce::AudioParameterFloatAttributes().withStringFromValueFunction (vowelText)));
    l.add (std::make_unique<F> (PID { "octave", 1 }, "Octave Layer", NR (0.f, 1.f), 0.4f));
    l.add (std::make_unique<F> (PID { "detune", 1 }, "Detune (cents)", NR (0.f, 50.f), 18.f));
    l.add (std::make_unique<F> (PID { "vibrato", 1 }, "Vibrato (cents)", NR (0.f, 40.f), 12.f));
    l.add (std::make_unique<F> (PID { "human", 1 }, "Humanize", NR (0.f, 1.f), 0.5f));
    l.add (std::make_unique<F> (PID { "spread", 1 }, "Stereo Spread", NR (0.f, 1.f), 0.8f));
    l.add (std::make_unique<F> (PID { "breath", 1 }, "Breath", NR (0.f, 1.f), 0.15f));
    l.add (std::make_unique<F> (PID { "attack", 1 }, "Attack (s)", NR (0.005f, 2.f, 0.f, 0.4f), 0.12f));
    l.add (std::make_unique<F> (PID { "release", 1 }, "Release (s)", NR (0.05f, 4.f, 0.f, 0.4f), 0.6f));
    l.add (std::make_unique<F> (PID { "chorus", 1 }, "Ensemble", NR (0.f, 1.f), 0.4f));
    l.add (std::make_unique<F> (PID { "rsize", 1 }, "Reverb Size", NR (0.f, 1.f), 0.7f));
    l.add (std::make_unique<F> (PID { "rmix", 1 }, "Reverb Mix", NR (0.f, 1.f), 0.3f));
    l.add (std::make_unique<F> (PID { "lowcut", 1 }, "Low Cut (Hz)", NR (20.f, 600.f, 0.f, 0.5f), 120.f));
    l.add (std::make_unique<F> (PID { "gain", 1 }, "Output (dB)", NR (-24.f, 12.f), 0.f));
    return l;
}

bool ChoirCrowdProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChoirCrowdProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (samplesPerBlock, 1), 2 };

    for (auto& f : formant)
    {
        f.prepare (spec);
        f.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        f.reset();
    }
    for (int i = 0; i < 3; ++i)
        formant[(size_t) i].setResonance (kQ[i]);

    lowCut.prepare (spec);
    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowCut.reset();

    chorus.prepare (spec);
    chorus.setRate (0.6f);
    chorus.setDepth (0.35f);
    chorus.setCentreDelay (14.f);
    chorus.setFeedback (0.f);
    chorus.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    vowelSmooth.reset (sampleRate, 0.05);
    vowelSmooth.setCurrentAndTargetValue (p ("vowel"));

    for (auto& v : voices) { v.active = false; v.held = false; }
}

void ChoirCrowdProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())            noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        for (auto& v : voices) v.held = false;
}

void ChoirCrowdProcessor::noteOn (int note, float velocity)
{
    Voice* vp = nullptr;
    for (auto& v : voices) if (! v.active) { vp = &v; break; }
    if (vp == nullptr)
    {
        vp = &voices[0];
        for (auto& v : voices) if (v.age < vp->age) vp = &v;
    }

    auto& v = *vp;
    v.active = true; v.held = true; v.note = note; v.vel = velocity; v.age = ++ageCounter;
    v.numSingers = juce::jlimit (1, kMaxSingers, (int) p ("crowd"));

    const float detune = p ("detune"), vib = p ("vibrato"), human = p ("human");
    const float spread = p ("spread"), octAmt = p ("octave");

    for (int k = 0; k < v.numSingers; ++k)
    {
        Singer s;
        s.phase     = rng.nextFloat();
        s.detune    = (rng.nextFloat() + rng.nextFloat() - 1.f) * detune;
        s.vibRate   = 4.2f + rng.nextFloat() * 2.8f;
        s.vibPhase  = rng.nextFloat();
        s.vibDepth  = vib * (0.5f + 0.5f * rng.nextFloat());
        const bool octave = (k % 3 == 2);
        s.octMul    = octave ? 0.5f : 1.f;
        s.level     = octave ? octAmt : 1.f;
        const float pan = 0.5f + (rng.nextFloat() * 2.f - 1.f) * 0.5f * spread;
        s.panL      = std::cos (pan * juce::MathConstants<float>::halfPi);
        s.panR      = std::sin (pan * juce::MathConstants<float>::halfPi);
        s.delay     = (int) (rng.nextFloat() * human * 0.08f * (float) sr);
        v.singers[(size_t) k] = s;
    }
}

void ChoirCrowdProcessor::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == note) v.held = false;
}

void ChoirCrowdProcessor::updateIncrements (Voice& v, int chunk)
{
    const float base = 440.f * std::exp2 ((float) (v.note - 69) / 12.f);
    for (int k = 0; k < v.numSingers; ++k)
    {
        auto& s = v.singers[(size_t) k];
        s.vibPhase += s.vibRate * (float) chunk / (float) sr;
        s.vibPhase -= std::floor (s.vibPhase);
        const float cents = s.detune + s.vibDepth * std::sin (s.vibPhase * juce::MathConstants<float>::twoPi);
        s.inc = juce::jmin (0.45f, base * s.octMul * std::exp2 (cents / 1200.f) / (float) sr);
    }
}

void ChoirCrowdProcessor::renderVoices (juce::AudioBuffer<float>& buffer, int start, int num)
{
    if (num <= 0) return;
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    const float atkInc = 1.f / (juce::jmax (0.001f, p ("attack")) * (float) sr);
    const float relInc = 1.f / (juce::jmax (0.001f, p ("release")) * (float) sr);
    const float breath = p ("breath");

    int done = 0;
    while (done < num)
    {
        const int chunk = juce::jmin (kChunk, num - done);
        for (auto& v : voices) if (v.active) updateIncrements (v, chunk);

        for (int i = 0; i < chunk; ++i)
        {
            float l = 0.f, r = 0.f, act = 0.f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const float vg = v.vel * 0.5f / std::sqrt ((float) v.numSingers);
                for (int k = 0; k < v.numSingers; ++k)
                {
                    auto& s = v.singers[(size_t) k];
                    if (s.delay > 0) --s.delay;
                    const float target = (v.held && s.delay <= 0) ? 1.f : 0.f;
                    if (s.env < target)      s.env = juce::jmin (target, s.env + atkInc);
                    else if (s.env > target) s.env = juce::jmax (target, s.env - relInc);
                    if (s.env <= 0.f) continue;

                    s.phase += s.inc;
                    if (s.phase >= 1.f) s.phase -= 1.f;
                    const float saw = 2.f * s.phase - 1.f - polyBlep (s.phase, s.inc);
                    const float a = saw * s.env * s.level * vg;
                    l += a * s.panL;
                    r += a * s.panR;
                    act += s.env * s.level * vg;
                }
            }
            const float nz = breath * act * 0.6f;
            l += nz * (rng.nextFloat() * 2.f - 1.f);
            r += nz * (rng.nextFloat() * 2.f - 1.f);
            L[start + done + i] += l;
            R[start + done + i] += r;
        }

        for (auto& v : voices)
        {
            if (! v.active || v.held) continue;
            bool silent = true;
            for (int k = 0; k < v.numSingers; ++k)
                if (v.singers[(size_t) k].env > 0.f) { silent = false; break; }
            if (silent) v.active = false;
        }
        done += chunk;
    }
}

void ChoirCrowdProcessor::applyFx (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    vowelSmooth.setTargetValue (p ("vowel"));
    const float vowel = vowelSmooth.skip (n);
    const int vi = juce::jlimit (0, 3, (int) vowel);
    const float vf = vowel - (float) vi;
    for (size_t b = 0; b < 3; ++b)
        formant[b].setCutoffFrequency (juce::jmap (vf, kVowels[vi][b], kVowels[vi + 1][b]));

    lowCut.setCutoffFrequency (p ("lowcut"));

    for (int i = 0; i < n; ++i)
    {
        float in[2] = { L[i], R[i] };
        float out[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            float y = 0.f;
            for (size_t b = 0; b < 3; ++b)
                y += kGain[b] * formant[b].processSample (ch, in[ch]) / kQ[b];
            out[ch] = lowCut.processSample (ch, 3.f * y + 0.15f * in[ch]);
        }
        L[i] = out[0];
        R[i] = out[1];
    }

    chorus.setMix (p ("chorus") * 0.5f);
    juce::dsp::AudioBlock<float> block (buffer);
    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));

    juce::Reverb::Parameters rp;
    rp.roomSize = p ("rsize");
    rp.damping = 0.4f;
    rp.width = 1.f;
    rp.wetLevel = p ("rmix") * 0.5f;
    rp.dryLevel = 1.f - p ("rmix") * 0.3f;
    reverb.setParameters (rp);
    reverb.processStereo (L, R, n);

    const float g = juce::Decibels::decibelsToGain (p ("gain"));
    for (int i = 0; i < n; ++i)
    {
        L[i] = std::tanh (L[i] * g);
        R[i] = std::tanh (R[i] * g);
    }
}

void ChoirCrowdProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (pos, n, meta.samplePosition);
        renderVoices (buffer, pos, t - pos);
        pos = t;
        handleMidi (meta.getMessage());
    }
    renderVoices (buffer, pos, n - pos);
    applyFx (buffer);
}

juce::AudioProcessorEditor* ChoirCrowdProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void ChoirCrowdProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void ChoirCrowdProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChoirCrowdProcessor();
}
