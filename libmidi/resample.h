#ifndef ___RESAMPLE_H_
#define ___RESAMPLE_H_

typedef int32 resample_t;

enum
{
    RESAMPLE_CSPLINE,
    RESAMPLE_LAGRANGE,
    RESAMPLE_GAUSS,
    RESAMPLE_NEWTON,
    RESAMPLE_LINEAR,
    RESAMPLE_NONE
};

extern int get_current_resampler(void);
extern int set_current_resampler(int type);
extern void initialize_resampler_coeffs(void);
extern int set_resampler_parm(int val);
extern void free_gauss_table(void);

typedef struct resample_rec
{
    splen_t loop_start;
    splen_t loop_end;
    splen_t data_length;
} resample_rec_t;

extern resample_t do_resamplation(sample_t *src, splen_t ofs, resample_rec_t *rec);
extern resample_t *resample_voice(int v, int32 *countptr);
extern void pre_resample(Sample *sp);

#endif /* ___RESAMPLE_H_ */
