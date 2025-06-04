#ifndef COLOR_H
#define COLOR_H

#include "Vec3.h"

#include <iostream>

using Color = Vec3;


inline Color from_hsv(double h, double s, double v) {
    int i = int(h * 6);
    double f = h * 6 - i;
    double p = v * (1 - s);
    double q = v * (1 - f * s);
    double t = v * (1 - (1 - f) * s);

    switch (i % 6) {
    case 0: return Color(v, t, p);
    case 1: return Color(q, v, p);
    case 2: return Color(p, v, t);
    case 3: return Color(p, q, v);
    case 4: return Color(t, p, v);
    case 5: return Color(v, p, q);
    }
    return Color(1, 1, 1); // Fallback white
}


inline Color from_hsv(Vec3 hsv) {
    double h = hsv.x();
    double s = hsv.y();
    double v = hsv.z();
    int i = int(h * 6);
    double f = h * 6 - i;
    double p = v * (1 - s);
    double q = v * (1 - f * s);
    double t = v * (1 - (1 - f) * s);

    switch (i % 6) {
    case 0: return Color(v, t, p);
    case 1: return Color(q, v, p);
    case 2: return Color(p, v, t);
    case 3: return Color(p, q, v);
    case 4: return Color(t, p, v);
    case 5: return Color(v, p, q);
    }
    return Color(1, 1, 1); // Fallback white
}

#endif