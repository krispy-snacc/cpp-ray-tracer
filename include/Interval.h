#ifndef INTERVAL_H
#define INTERVAL_H

#include "Utils.h"

class Interval {
public:
    double min;
    double max;

    Interval() : min(-infinity), max(infinity) {}
    Interval(double min, double max) : min(min), max(max) {}

    double size() {
        return max - min;
    }

    bool contains(double x) {
        return min <= x && x <= max;
    }

    bool surrounds(double x) {
        return min < x && x < max;
    }

    double clamp(double x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    static const Interval Empty, Universe;
};

const Interval Interval::Empty = Interval(+infinity, -infinity);
const Interval Interval::Universe = Interval(-infinity, +infinity);


#endif