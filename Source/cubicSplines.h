/*
  ==============================================================================

    cubicSplines.h
    Created: 3 Nov 2025 7:01:59pm
    Author:  matjo

  ==============================================================================
*/

#pragma once
#include<iostream>
#include<vector>
#include<algorithm>
#include<cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

using namespace std;

namespace ttvst::splines {


    using vec = std::vector<double>;
    using namespace std;

    struct splineSet {
        double a;
        double b;
        double c;
        double d;
        double x;
    };

    vector<double> createPositionVector(vector<splineSet> cs, vec x, vec y) {
        vec pos;
        for (int j = 0; j < x.size() - 1; j++) {
            auto spl = cs[j];
            for (int t = x[j]; t < x[j + 1]; t++) {
                int tj = t - spl.x;
                pos.push_back(spl.a + spl.b * tj + spl.c * pow(tj, 2) + spl.d * pow(tj, 3));
            }
        }
        return pos;
    }




    void save_vector_csv(const std::string& path,
        const std::vector<double>& v,
        int precision = 12)
    {
        std::ofstream ofs(path);
        if (!ofs) throw std::runtime_error("Failed to open " + path);
        ofs << std::fixed << std::setprecision(precision);
        for (double val : v) ofs << val << '\n';
        DBG("vector saved");
    }

    //we wish to find set of n splines S_i(x) for i = 0, ..., i = n - 1	
    vector<splineSet> spline(vec& x, vec& y) {
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

        vector<splineSet> output_set(n);
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