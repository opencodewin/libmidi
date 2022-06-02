#include <unistd.h>

#include "common.h"
#include "controls.h"

/* Minimal control mode */
extern ControlMode dumb_control_mode;

#ifdef IA_NCURSES
extern ControlMode ncurses_control_mode;
#ifndef DEFAULT_CONTROL_MODE
#define DEFAULT_CONTROL_MODE &ncurses_control_mode
#endif
#endif

#ifndef DEFAULT_CONTROL_MODE
#define DEFAULT_CONTROL_MODE &dumb_control_mode
#endif

ControlMode *ctl_list[] = {
#ifdef IA_NCURSES
    &ncurses_control_mode,
#endif
    &dumb_control_mode,
    0};

ControlMode *ctl = DEFAULT_CONTROL_MODE;

int std_write(int fd, const void *buffer, int size)
{
    /* redirect stdout writes */
    if (fd == 1 && ctl->write)
        return ctl->write((char *)buffer, size);
    return write(fd, buffer, size);
}
