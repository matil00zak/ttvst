/*
  ==============================================================================

    cubicSplines.h
    Created: 3 Nov 2025 7:01:59pm
    Author:  matjo

  ==============================================================================
*/
/*
#pragma once
#include <vector>
#include <string>
#include <optional>


namespace ttvst::splines {


    using vec = std::vector<double>;

    struct splineSet {
        double a;
        double b;
        double c;
        double d;
        double x;
    };

    struct splineCondition {
        double alpha;
        double l;
        double mu;
        double z;
    };

    struct splineSetPlus {
        std::vector<splineSet> set;
        splineCondition spline_condition;
        splineSet jointSpline;

    };


    std::vector<double> createPositionVector(std::vector<splineSet> cs, vec x, vec y, int outN);

    std::vector<double> createSpeedVector(std::vector<splineSet> cs, int outN);


    void save_vector_csv(const std::string& path,
        const std::vector<double>& v,
        int precision);

    void append_vector_csv(const std::string& path,
        const std::vector<double>& v,
        int precision);


    std::vector<splineSet> spline(vec& x, vec& y);

    splineSetPlus splineSpecial(vec& x, vec& y, std::optional<splineCondition> spline_condition, int newCondIndex, const int outN);

}
*/