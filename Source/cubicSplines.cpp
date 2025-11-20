/*
  ==============================================================================

    cubicSplines.cpp
    Created: 20 Nov 2025 10:01:14pm
    Author:  matjo

  ==============================================================================
*/
#pragma once
#include "cubicSplines.h"
#include<iostream>
#include<algorithm>
#include<cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <JuceHeader.h>
#include <filesystem> // C++17


namespace ttvst::splines {



    std::vector<double> createPositionVector(std::vector<splineSet> cs, vec x, vec y, int outN) {
        // must have at least two points and same length
        if (x.size() <= 1 || y.size() <= 1 || x.size() != y.size()) {
            return {}; // empty result: nothing to do
            DBG("splines: vectors empty or invalid");
        }

        vec pos;
        for (int i = 0; i < x.size() - 1; i++) {
            splineSet spl = cs[i];
            int seg_start = x[i];
            int seg_end = x[i + 1];
            if (seg_end > outN)
                seg_end = outN;
            for (int t = seg_start; t < seg_end; t++) {
                int xj = t - spl.x;
                pos.push_back(spl.a + spl.b * xj + spl.c * pow(xj, 2) + spl.d * pow(xj, 3));
            }

        }
        if (x[0] < 0) {
            pos.erase(pos.begin(), pos.begin() + std::abs(x[0]));
        }

        return pos;
    }




    void save_vector_csv(const std::string& path,
        const std::vector<double>& v,
        int precision)
    {
        std::ofstream ofs(path);
        if (!ofs) throw std::runtime_error("Failed to open " + path);
        ofs << std::fixed << std::setprecision(precision);
        for (double val : v) ofs << val << '\n';
        DBG("vector saved");
    }

    void append_vector_csv(const std::string& path,
        const std::vector<double>& v,
        int precision)
    {
        if (v.empty()) return; // nothing to append

        // If the file exists and is non-empty, check whether the last byte is '\n'.
        // If the file doesn't exist, ofstream with ios::app will create it.
        bool need_leading_newline = false;
        if (std::filesystem::exists(path)) {
            std::ifstream ifs(path, std::ios::binary);
            if (ifs) {
                ifs.seekg(0, std::ios::end);
                std::streampos sz = ifs.tellg();
                if (sz > 0) {
                    ifs.seekg(-1, std::ios::end);
                    char last = ifs.get();
                    if (last != '\n') need_leading_newline = true;
                }
            }
        }

        std::ofstream ofs(path, std::ios::app);
        if (!ofs) throw std::runtime_error("Failed to open " + path + " for appending");

        ofs << std::fixed << std::setprecision(precision);
        if (need_leading_newline) ofs << '\n';

        for (double val : v) ofs << val << '\n';
    }


    std::vector<splineSet> spline(vec& x, vec& y) {
        // must have at least two points and same length
        if (x.size() <= 1 || y.size() <= 1 || x.size() != y.size()) {
            return {}; // empty result: nothing to do
        }

        int n = x.size() - 1;
        vec a;

        a.insert(a.begin(), y.begin(), y.end());
        vec b(n);
        vec d(n);
        vec h;

        for (int i = 0; i < n; ++i)
            h.push_back(x[i + 1] - x[i]);

        vec alpha;
        alpha.push_back(0);
        for (int i = 1; i < n; ++i)
            alpha.push_back(3 * (a[i + 1] - a[i]) / h[i] - 3 * (a[i] - a[i - 1]) / h[i - 1]);

        vec c(n + 1);
        vec l(n + 1);
        vec mu(n + 1);
        vec z(n + 1);
        l[0] = 1;
        mu[0] = 0;
        z[0] = 0;

        for (int i = 1; i < n; ++i)
        {
            l[i] = 2 * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1];
            mu[i] = h[i] / l[i];
            z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
        }

        l[n] = 1;
        z[n] = 0;
        c[n] = 0;

        for (int j = n - 1; j >= 0; --j)
        {
            c[j] = z[j] - mu[j] * c[j + 1];
            b[j] = (a[j + 1] - a[j]) / h[j] - h[j] * (c[j + 1] + 2 * c[j]) / 3;
            d[j] = (c[j + 1] - c[j]) / 3 / h[j];
        }

        std::vector<splineSet> output_set(n);
        for (int i = 0; i < n; ++i)
        {
            output_set[i].a = a[i];
            output_set[i].b = b[i];
            output_set[i].c = c[i];
            output_set[i].d = d[i];
            output_set[i].x = x[i];
        }
        return output_set;
    }
}