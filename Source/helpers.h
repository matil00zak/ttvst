/*
  ==============================================================================

    helpers.h
    Created: 31 Oct 2025 4:06:01pm
    Author:  matjo

  ==============================================================================
*/

#pragma once

#include <optional>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

namespace ttvst::helps
{   
    using intPair = std::pair<int, int>;
    using pairVector = std::vector<intPair>;

    std::optional<juce::MidiMessageMetadata> getLastPitchWheelMessage(const juce::MidiBuffer& buffer);

    std::optional<juce::MidiMessageMetadata> getFirstPitchWheelMessage(const juce::MidiBuffer& buffer);

    std::optional<ttvst::helps::pairVector> getPitchWheelMsgPairVec(const juce::MidiBuffer& buffer);

    std::optional<std::vector<double>> getPitchWheelValueVector(const juce::MidiBuffer& buffer);

    std::optional<std::vector<double>> getPitchWheelOffsetsVector(const juce::MidiBuffer& buffer);

    bool hasPitchWheelMessage(const juce::MidiBuffer& buffer);

    std::vector<double> pitchWheelToSamplePositionVec(const std::vector<double>);

    double pitchWheelToSamplePosition(const double);

    std::vector<double> createRatiosVector(std::vector<double> Y, std::optional<double> preRenderValue);


} // namespace ttvst::midi
