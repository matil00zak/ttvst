/*
  ==============================================================================

    helpers.h
    Created: 31 Oct 2025 4:06:01pm
    Author:  matjo

  ==============================================================================
*/

#pragma once

#include <optional>
#include <juce_audio_basics/juce_audio_basics.h>

namespace ttvst::midi
{
    [[nodiscard]] inline std::optional<juce::MidiMessageMetadata>
        getLastPitchWheelMessage(const juce::MidiBuffer& buffer) noexcept
    {
        std::optional<juce::MidiMessageMetadata> last;

        for (auto metadata : buffer)
        {
            const auto& m = metadata.getMessage();
            if (m.isPitchWheel())
                last = metadata;
        }

        return last;
    }


    [[nodiscard]] inline std::optional<juce::MidiMessageMetadata>
        getFirstPitchWheelMessage(const juce::MidiBuffer& buffer) noexcept
    {
        for (auto metadata : buffer)
        {
            if (metadata.getMessage().isPitchWheel()) {
                
                return metadata;
            }
        }   
        return std::nullopt;
    }

    


} // namespace ttvst::midi
