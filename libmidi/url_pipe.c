#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "url.h"

#ifdef HAVE_POPEN
/* It is not supported command PIPE at Windows */

typedef struct _URL_pipe
{
    char common[sizeof(struct _URL)];
    FILE *fp;
} URL_pipe;

#define PIPE_FP(url) (((URL_pipe *)(url))->fp)

static int name_pipe_check(char *url_string);
static long url_pipe_read(URL url, void *buff, long n);
static char *url_pipe_gets(URL url, char *buff, int n);
static int url_pipe_fgetc(URL url);
static void url_pipe_close(URL url);

struct URL_module URL_module_pipe =
{
    URL_pipe_t,      /* type */
    name_pipe_check, /* URL checker */
    NULL,            /* initializer */
    url_pipe_open,   /* open */
    NULL             /* must be NULL */
};

/* url_string := "command|" */
static int name_pipe_check(char *url_string)
{
#ifdef PIPE_SCHEME_ENABLE
    char *p;
    p = strrchr(url_string, '|');
    if (p == NULL)
        return 0;
    p++;
    while (*p == ' ')
        p++;
    return *p == '\0';
#else
    return 0;
#endif
}

URL url_pipe_open(char *command)
{
    URL_pipe *url;
    char buff[BUFSIZ], *p;

    strncpy(buff, command, sizeof(buff));
    buff[sizeof(buff) - 1] = '\0';
    p = strrchr(buff, '|');
    if (p != NULL)
    {
        char *q;

        q = p + 1;
        while (*q == ' ')
            q++;
        if (*q == '\0')
        {
            p--;
            while (buff < p && *p == ' ')
                p--;
            if (buff == p)
            {
                errno = ENOENT;
                url_errno = URLERR_IURLF;
                return NULL;
            }
            p[1] = '\0';
        }
    }

    url = (URL_pipe *)alloc_url(sizeof(URL_pipe));
    if (url == NULL)
    {
        url_errno = errno;
        return NULL;
    }

    /* common members */
    URLm(url, type) = URL_pipe_t;
    URLm(url, url_read) = url_pipe_read;
    URLm(url, url_gets) = url_pipe_gets;
    URLm(url, url_fgetc) = url_pipe_fgetc;
    URLm(url, url_seek) = NULL;
    URLm(url, url_tell) = NULL;
    URLm(url, url_close) = url_pipe_close;

    /* private members */
    url->fp = NULL;

    if ((url->fp = popen(buff, "r")) == NULL)
    {
        url_pipe_close((URL)url);
        url_errno = errno;
        return NULL;
    }

    return (URL)url;
}

static long url_pipe_read(URL url, void *buff, long n)
{
    return (long)fread(buff, 1, n, PIPE_FP(url));
}

static char *url_pipe_gets(URL url, char *buff, int n)
{
    return fgets(buff, n, PIPE_FP(url));
}

static int url_pipe_fgetc(URL url)
{
#ifdef getc
    return getc(PIPE_FP(url));
#else
    return fgetc(PIPE_FP(url));
#endif /* getc */
}

static void url_pipe_close(URL url)
{
    int save_errno = errno;
    if (PIPE_FP(url) != NULL)
        pclose(PIPE_FP(url));
    free(url);
    errno = save_errno;
}

#else /* HAVE_POPEN */
struct URL_module URL_module_pipe =
    {
        URL_none_t, /* type */
        NULL,       /* URL checker */
        NULL,       /* initializer */
        NULL,       /* open */
        NULL        /* must be NULL */
};
URL url_pipe_open(char *command) { return NULL; } /* dmy */
#endif
