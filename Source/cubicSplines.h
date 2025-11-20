/*
  ==============================================================================

    cubicSplines.h
    Created: 3 Nov 2025 7:01:59pm
    Author:  matjo

  ==============================================================================
*/

#pragma once
#include<vector>
#include <string>



namespace ttvst::splines {


    using vec = std::vector<double>;

    struct splineSet {
        double a;
        double b;
        double c;
        double d;
        double x;
    };

    std::vector<double> createPositionVector(std::vector<splineSet> cs, vec x, vec y, int outN);


    void save_vector_csv(const std::string& path,
        const std::vector<double>& v,
        int precision);

    void append_vector_csv(const std::string& path,
        const std::vector<double>& v,
        int precision);


    std::vector<splineSet> spline(vec& x, vec& y);
}