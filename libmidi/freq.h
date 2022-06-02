#ifndef __FREQ_H__
#define __FREQ_H__
extern float pitch_freq_table[129];
extern float pitch_freq_ub_table[129];
extern float pitch_freq_lb_table[129];
extern int chord_table[4][3][3];

extern float freq_fourier(Sample *sp, int *chord);
extern int assign_pitch_to_freq(float freq);

#define CHORD_MAJOR 0
#define CHORD_MINOR 3
#define CHORD_DIM 6
#define CHORD_FIFTH 9
#define LOWEST_PITCH 0
#define HIGHEST_PITCH 127

#endif /* __FREQ_H__ */
