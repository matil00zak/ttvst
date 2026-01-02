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
    using vectorPairDbl = std::pair<std::vector<double>, std::vector<double>>;

    std::optional<juce::MidiMessageMetadata> getLastPitchWheelMessage(const juce::MidiBuffer& buffer);

    std::optional<juce::MidiMessageMetadata> getFirstPitchWheelMessage(const juce::MidiBuffer& buffer);

    std::optional<ttvst::helps::pairVector> getPitchWheelMsgPairVec(const juce::MidiBuffer& buffer);

    std::optional<std::vector<double>> getPitchWheelValueVector(const juce::MidiBuffer& buffer);

    std::optional<std::vector<double>> getPitchWheelOffsetsVector(const juce::MidiBuffer& buffer);

    bool hasPitchWheelMessage(const juce::MidiBuffer& buffer);

    std::vector<double> pitchWheelToSamplePositionVec(const std::vector<double>);

    double pitchWheelToSamplePosition(const double);

    std::vector<double> createRatiosVector(std::vector<double> Y, std::optional<double> preRenderValue);


    //creates pair of speeds and offsets from the full values and offsets pairs
    //the pairs are only for the messages int THIS buffer and one lookahead message
    //product of this function is supposed to be interpolated
    //for the interpolation to cover the whole buffer, spline set needs inserting the last
    //spline saved from previous iteration 
    // is meant to be used with last generated spline to complete the current buffer on its beggining
    vectorPairDbl positionsToSpeed(std::vector<double> positions, std::vector<double> offsets, int outN, int lookahead);

    void catchSpeedOutliers(std::vector<double>& speeds, double maxSpeedAbs);
    void insertBaseSpeed(std::vector<double>& speeds, std::vector<double>& offsets, double baseSpeed);
} // namespace ttvst::midi
