#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "miditrace.h"
#include "aq.h"

enum trace_argtypes
{
    ARG_VOID,
    ARG_INT,
    ARG_INT_INT,
    ARG_CE,
    ARGTIME_VP,
};

typedef struct _MidiTraceList
{
    int32 start; /* Samples to start */
    int argtype; /* Argument type */

    union
    { /* Argument */
        int args[2];
        uint16 ui16;
        CtlEvent ce;
        void *v;
    } a;

    union
    { /* Handler function */
        void (*f0)(void);
        void (*f1)(int);
        void (*f2)(int, int);
        void (*fce)(CtlEvent *ce);
        void (*fv)(void *);
    } f;

    struct _MidiTraceList *next; /* Chain link next node */
} MidiTraceList;

MidiTrace midi_trace;

static MidiTraceList *new_trace_node(void)
{
    MidiTraceList *p;

    if (midi_trace.free_list == NULL)
        p = (MidiTraceList *)new_segment(&midi_trace.pool, sizeof(MidiTraceList));
    else
    {
        p = midi_trace.free_list;
        midi_trace.free_list = midi_trace.free_list->next;
    }
    return p;
}

static void reuse_trace_node(MidiTraceList *p)
{
    p->next = midi_trace.free_list;
    midi_trace.free_list = p;
}

static int32 trace_start_time(void)
{
    if (play_mode->flag & PF_CAN_TRACE)
        return current_sample;
    return -1;
}

static void run_midi_trace(MidiTraceList *p)
{
    if (!ctl->opened)
        return;

    switch (p->argtype)
    {
    case ARG_VOID:
        p->f.f0();
        break;
    case ARG_INT:
        p->f.f1(p->a.args[0]);
        break;
    case ARG_INT_INT:
        p->f.f2(p->a.args[0], p->a.args[1]);
        break;
    case ARGTIME_VP:
        p->f.fv(p->a.v);
        break;
    case ARG_CE:
        p->f.fce(&p->a.ce);
        break;
    }
}

static MidiTraceList *midi_trace_setfunc(MidiTraceList *node)
{
    MidiTraceList *p;

    if (!ctl->trace_playing || node->start < 0)
    {
        run_midi_trace(node);
        return NULL;
    }

    p = new_trace_node();
    *p = *node;
    p->next = NULL;

    if (midi_trace.head == NULL)
        midi_trace.head = midi_trace.tail = p;
    else
    {
        midi_trace.tail->next = p;
        midi_trace.tail = p;
    }

    return p;
}

void push_midi_trace0(void (*f)(void))
{
    MidiTraceList node;
    if (f == NULL)
        return;
    memset(&node, 0, sizeof(node));
    node.start = trace_start_time();
    node.argtype = ARG_VOID;
    node.f.f0 = f;
    midi_trace_setfunc(&node);
}

void push_midi_trace1(void (*f)(int), int arg1)
{
    MidiTraceList node;
    if (f == NULL)
        return;
    memset(&node, 0, sizeof(node));
    node.start = trace_start_time();
    node.argtype = ARG_INT;
    node.f.f1 = f;
    node.a.args[0] = arg1;
    midi_trace_setfunc(&node);
}

void push_midi_trace2(void (*f)(int, int), int arg1, int arg2)
{
    MidiTraceList node;
    if (f == NULL)
        return;
    memset(&node, 0, sizeof(node));
    node.start = trace_start_time();
    node.argtype = ARG_INT_INT;
    node.f.f2 = f;
    node.a.args[0] = arg1;
    node.a.args[1] = arg2;
    midi_trace_setfunc(&node);
}

void push_midi_trace_ce(void (*f)(CtlEvent *), CtlEvent *ce)
{
    MidiTraceList node;
    if (f == NULL)
        return;
    memset(&node, 0, sizeof(node));
    node.start = trace_start_time();
    node.argtype = ARG_CE;
    node.f.fce = f;
    node.a.ce = *ce;
    midi_trace_setfunc(&node);
}

void push_midi_time_vp(int32 start, void (*f)(void *), void *vp)
{
    MidiTraceList node;
    if (f == NULL)
        return;
    memset(&node, 0, sizeof(node));
    node.start = start;
    node.argtype = ARGTIME_VP;
    node.f.fv = f;
    node.a.v = vp;
    midi_trace_setfunc(&node);
}

int32 trace_loop(void)
{
    int32 cur;
    int ctl_update;
    static int lasttime = -1;

    if (midi_trace.trace_loop_hook != NULL)
        midi_trace.trace_loop_hook();

    if (midi_trace.head == NULL)
        return 0;

    if ((cur = current_trace_samples()) == -1 || !ctl->trace_playing)
        cur = 0x7fffffff; /* apply all trace event */

    ctl_update = 0;
    while (midi_trace.head && cur >= midi_trace.head->start && cur > 0) /* privent flying start */
    {
        MidiTraceList *p;

        p = midi_trace.head;
        run_midi_trace(p);
        if (p->argtype == ARG_CE)
            ctl_update = 1;
        midi_trace.head = midi_trace.head->next;
        reuse_trace_node(p);
    }

    if (ctl_update)
        ctl_mode_event(CTLE_REFRESH, 0, 0, 0);

    if (midi_trace.head == NULL)
    {
        midi_trace.tail = NULL;
        return 0; /* No more tracing */
    }

    if (!ctl_update)
    {
        if (lasttime == cur)
            midi_trace.head->start--; /* avoid infinite loop */
        lasttime = cur;
    }

    return 1; /* must call trace_loop() continued */
}

void trace_offset(int offset)
{
    midi_trace.offset = offset;
}

void trace_flush(void)
{
    midi_trace.flush_flag = 1;
    //wrd_midi_event(WRD_START_SKIP, WRD_NOARG);
    while (midi_trace.head)
    {
        MidiTraceList *p;

        p = midi_trace.head;
        run_midi_trace(p);
        midi_trace.head = midi_trace.head->next;
        reuse_trace_node(p);
    }
    //wrd_midi_event(WRD_END_SKIP, WRD_NOARG);
    reuse_mblock(&midi_trace.pool);
    midi_trace.head = midi_trace.tail = midi_trace.free_list = NULL;
    ctl_mode_event(CTLE_REFRESH, 0, 0, 0);
    midi_trace.flush_flag = 0;
}

void set_trace_loop_hook(void (*f)(void))
{
    midi_trace.trace_loop_hook = f;
}

int32 current_trace_samples(void)
{
    int32 sp;
    if ((sp = aq_samples()) == -1)
        return -1;
    return midi_trace.offset + aq_samples();
}

void init_midi_trace(void)
{
    memset(&midi_trace, 0, sizeof(midi_trace));
    init_mblock(&midi_trace.pool);
}

int32 trace_wait_samples(void)
{
    int32 s;

    if (midi_trace.head == NULL)
        return -1;
    if ((s = current_trace_samples()) == -1)
        return 0;
    s = midi_trace.head->start - s;
    if (s < 0)
        s = 0;
    return s;
}
