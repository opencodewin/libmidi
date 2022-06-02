#ifndef __FFT4G_H__
#define __FFT4G_H__

extern void cdft(int, int, float *, int *, float *);
extern void rdft(int, int, float *, int *, float *);
extern void ddct(int, int, float *, int *, float *);
extern void ddst(int, int, float *, int *, float *);
extern void dfct(int, float *, float *, int *, float *);
extern void dfst(int, float *, float *, int *, float *);

#endif /* __FFT4G_H__ */