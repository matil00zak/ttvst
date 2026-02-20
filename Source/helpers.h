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

    std::optional<std::vector<double>> getPitchWheelValueVector(const juce::MidiBuffer& buffer, int maxMessages);

    std::optional<std::vector<double>> getPitchWheelOffsetsVector(const juce::MidiBuffer& buffer, int maxMessages);
    // this function writes to preallocated vectors offsets and values
// max count is the vectors reserved size
// base offset is the local time at which the buffers start time is percieved in the process block 
// (for the lookahead buffer this is outN)
    void extractPitchWheelData(const juce::MidiBuffer& buffer, int& count, double* offsets, double* values, int maxCount, double baseOffset);

    bool hasPitchWheelMessage(const juce::MidiBuffer& buffer);

    std::vector<double> pitchWheelToSamplePositionVec(const std::vector<double>, float scale);

    double pitchWheelToSamplePosition(const double);

    std::vector<double> createRatiosVector(std::vector<double> Y, std::optional<double> preRenderValue);


    //creates pair of speeds and offsets from the full values and offsets pairs
    //the pairs are only for the messages int THIS buffer and one lookahead message
    //product of this function is supposed to be interpolated
    //for the interpolation to cover the whole buffer, spline set needs inserting the last
    //spline saved from previous iteration 
    // is meant to be used with last generated spline to complete the current buffer on its beggining
    vectorPairDbl positionsToSpeed(std::vector<double> positions, std::vector<double> offsets, int outN, int lookahead);
    vectorPairDbl positionsToSpeedWrapped(std::vector<double> positions, std::vector<double> offsets, int outN, int lookahead);
    void appendNewPitchWheelSpeeds(std::vector<double>& speeds,
        std::vector<double>& speeds_offsets,
        std::vector<double> positions,
        std::vector<double> offsets,
        int outN);
    void deleteOldPitchWheelSpeeds(std::vector<double>& speeds, std::vector<double>& speed_offsets, int outN);

    void catchSpeedOutliers(std::vector<double>& speeds, double maxSpeedAbs);
    void insertBaseSpeed(std::vector<double>& speeds, std::vector<double>& offsets, double baseSpeed);
    void insertLastSpeed(std::vector<double>& speeds, std::vector<double>& offsets, double& lastSpeed, double& lastOffset, int outN);

    

    void wrapPlayhead(double& playhead, long srcLength);

    
    int appendPitchWheelMetadata(
        const juce::MidiBuffer& buffer,
        int outN,
        std::vector<double>& positions,
        std::vector<double>& offsets);

    void repairPitchWheelMetadata(int outN, std::vector<double>& positions, std::vector<double>& offsets, bool forceOneMsg);


} // namespace ttvst::midi
