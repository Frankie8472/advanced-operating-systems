#include "calculate.h"

#include </usr/lib/gcc-cross/aarch64-linux-gnu/9/include/arm_neon.h>


void calculate(const struct calc_request *cr, float *ret)
{
    for (long j = 0; j < cr->height; j++) {
        float64_t yv = (float64_t) (cr->y) + (float64_t)(j) * (float64_t)(cr->h / cr->height);
        for (long i = 0; i < cr->width; i += 2) {
            float64_t xv[] = {
                (float64_t) (cr->x + (float64_t) (i) * cr->w / cr->width),
                (float64_t) (cr->x + (float64_t) (i + 1) * cr->w / cr->width),
            };
            float64x2_t x0 = vld1q_f64(xv);
            float64x2_t y0 = vmovq_n_f64(yv);

            uint64x2_t increments = vmovq_n_u64(1);
            uint64x2_t counter = vmovq_n_u64(0);
            float64x2_t threshold = vmovq_n_f64(4);

            float64x2_t x = x0;
            float64x2_t y = y0;

            for (int k = 0; k < cr->max_iterations; k++) {
                float64x2_t xsq = vmulq_f64(x, x);
                float64x2_t ysq = vmulq_f64(y, y);
                float64x2_t xy = vmulq_f64(x, y);
                float64x2_t two_xy = vaddq_f64(xy, xy);

                uint64x2_t not_escaped = vcleq_f64(vaddq_f64(xsq, ysq), threshold);
                if (k % 16 == 0 && vaddvq_u64(not_escaped) == 0) {
                    break;
                }

                increments = vandq_u64(increments, not_escaped);
                counter = vaddq_u64(counter, increments);

                x = vaddq_f64(vsubq_f64(xsq, ysq), x0);
                y = vaddq_f64(two_xy, y0);
            }
        }
    }
}
