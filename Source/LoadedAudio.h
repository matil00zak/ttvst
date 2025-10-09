#pragma once

// Minimal JUCE include for AudioBuffer and basic types.
// If you use the Unity build, this can be just <JuceHeader.h>.
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

/**
 * LoadedAudio
 * -----------
 * Immutable container for fully-decoded audio held in RAM.
 * - sampleRate: Hz of the decoded buffer
 * - buffer: interleaved-by-channel, non-owning to the outside (we don't expose non-const access)
 *
 * Intended to be shared across threads via std::shared_ptr<const LoadedAudio>.
 */
struct LoadedAudio
{
    /// Construct an empty/invalid container.
    LoadedAudio() = default;

    /// Construct from a buffer copy and its sample rate.
    LoadedAudio(double sr, const juce::AudioBuffer<float>& src)
        : sampleRate(sr), buffer(src) {
    }

    /// Move-enabled for efficiency.
    LoadedAudio(double sr, juce::AudioBuffer<float>&& src)
        : sampleRate(sr), buffer(std::move(src)) {
    }

    /// True if there is audio data available.
    bool isValid() const noexcept { return buffer.getNumSamples() > 0 && sampleRate > 0.0; }

    /// Number of channels in the buffer.
    int getNumChannels() const noexcept { return buffer.getNumChannels(); }

    /// Number of samples per channel.
    int getNumSamples() const noexcept { return buffer.getNumSamples(); }

    /// Length in seconds.
    double getLengthSeconds() const noexcept
    {
        return (sampleRate > 0.0) ? static_cast<double>(buffer.getNumSamples()) / sampleRate : 0.0;
    }

    /// Sample rate (Hz) of this audio.
    double sampleRate = 0.0;

    /// The decoded audio data. Keep this const to encourage read-only use.
    juce::AudioBuffer<float> buffer;
};

// Handy alias for the shared, read-only handle you pass around the processor/engine.
using LoadedAudioPtr = std::shared_ptr<const LoadedAudio>;
