#ifndef ___MIX_H_
#define ___MIX_H_
extern void mix_voice(int32 *, int, int32);
extern int recompute_envelope(int);
extern int apply_envelope_to_amp(int);
extern int recompute_modulation_envelope(int);
extern int apply_modulation_envelope(int);
/* time (ms) for full vol note to sustain */
extern int min_sustain_time;
#endif /* ___MIX_H_ */
