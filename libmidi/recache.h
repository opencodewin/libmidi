#ifndef ___RECACHE_H_
#define ___RECACHE_H_

struct cache_hash
{
    /* cache key */
    int note;
    Sample *sp;

    int32 cnt;			/* counter */
    double r;			/* size/refcnt */
    struct _Sample *resampled;
    struct cache_hash *next;
};

extern void resamp_cache_reset(void);
extern void resamp_cache_refer_on(Voice *vp, int32 sample_start);
extern void resamp_cache_refer_off(int ch, int note, int32 sample_end);
extern void resamp_cache_refer_alloff(int ch, int32 sample_end);
extern void resamp_cache_create(void);
extern struct cache_hash *resamp_cache_fetch(struct _Sample *sp, int note);
extern void free_cache_data(void);

extern int32 allocate_cache_size;

#endif /* ___RECACHE_H_ */
