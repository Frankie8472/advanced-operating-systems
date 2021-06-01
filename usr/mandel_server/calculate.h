#ifndef CALCULATE_H
#define CALCULATE_H

struct calc_request {
    double x, y, w, h;
    int max_iterations;
    int width, height;
};



void calculate(const struct calc_request *cr, int *ret);

#endif // CALCULATE_H
