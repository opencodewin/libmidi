#ifndef ___MIDITRACE_H_
#define ___MIDITRACE_H_
#include "mblock.h"
#include "controls.h"

typedef struct _MidiTrace
{
    int offset;     /* sample offset */
    int flush_flag; /* True while in trace_flush() */

    void (*trace_loop_hook)(void); /* Hook function for extension */

    /* Delayed event list  (The member is only access in miditrace.c) */
    struct _MidiTraceList *head;
    struct _MidiTraceList *tail;

    /* Memory buffer */
    struct _MidiTraceList *free_list;
    MBlockList pool;
} MidiTrace;

extern void init_midi_trace(void);

extern void push_midi_trace0(void (*f)(void));
extern void push_midi_trace1(void (*f)(int), int arg1);
extern void push_midi_trace2(void (*f)(int, int), int arg1, int arg2);
extern void push_midi_trace_ce(void (*f)(CtlEvent *), CtlEvent *ce);
extern void push_midi_time_vp(int32 start, void (*f)(void *), void *vp);

extern int32 trace_loop(void);
extern void trace_flush(void);
extern void trace_offset(int offset);
extern void trace_nodelay(int nodelay);
extern void set_trace_loop_hook(void (*f)(void));
extern int32 current_trace_samples(void);
extern int32 trace_wait_samples(void);

extern MidiTrace midi_trace;

#endif /* ___MIDITRACE_H_ */
