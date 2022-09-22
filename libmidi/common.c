#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "common.h"
#include "output.h"
#include "controls.h"
#include "strtab.h"
#include "tables.h"
#include "instrum.h"
#include "quantity.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* #define MIME_CONVERSION */

char *program_name, current_filename[1024];
MBlockList tmpbuffer;
char *output_text_code = NULL;
int open_file_noise_mode = OF_NORMAL;

#ifdef DEFAULT_PATH
/* The paths in this list will be tried whenever we're reading a file */
static PathList defaultpathlist = {DEFAULT_PATH, 0};
static PathList *pathlist = &defaultpathlist; /* This is a linked list */
#else
static PathList *pathlist = 0;
#endif

const char *note_name[] =
    {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

#ifndef TMP_MAX
#define TMP_MAX 238328
#endif

int tmdy_mkstemp(char *tmpl)
{
    char *XXXXXX;
    static uint32 value;
    uint32 random_time_bits;
    int count, fd = -1;
    int save_errno = errno;

    /* These are the characters used in temporary filenames.  */
    static const char letters[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    /* This is where the Xs start.  */
    XXXXXX = strstr(tmpl, "XXXXXX");
    if (XXXXXX == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    /* Get some more or less random data.  */
#if HAVE_GETTIMEOFDAY
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_time_bits = (uint32)((tv.tv_usec << 16) ^ tv.tv_sec);
    }
#else
    random_time_bits = (uint32)time(NULL);
#endif

    value += random_time_bits ^ getpid();

    for (count = 0; count < TMP_MAX; value += 7777, ++count)
    {
        uint32 v = value;

        /* Fill in the random bits.  */
        XXXXXX[0] = letters[v % 62];
        v /= 62;
        XXXXXX[1] = letters[v % 62];
        v /= 62;
        XXXXXX[2] = letters[v % 62];

        v = (v << 16) ^ value;
        XXXXXX[3] = letters[v % 62];
        v /= 62;
        XXXXXX[4] = letters[v % 62];
        v /= 62;
        XXXXXX[5] = letters[v % 62];

#if defined(_MSC_VER) || defined(__DMC__)
#define S_IRUSR 0
#define S_IWUSR 0
#endif

        fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL | O_BINARY, S_IRUSR | S_IWUSR);

        if (fd >= 0)
        {
            errno = save_errno;
            return fd;
        }
        if (errno != EEXIST)
            return -1;
    }

    /* We got out of the loop because we ran out of combinations to try.  */
    errno = EEXIST;
    return -1;
}

static char *
url_dumpfile(URL url, const char *ext)
{
    char filename[1024];
    char *tmpdir;
    int fd;
    FILE *fp;
    int n;
    char buff[BUFSIZ];

#ifdef TMPDIR
    tmpdir = TMPDIR;
#else
    tmpdir = getenv("TMPDIR");
#endif
    if (tmpdir == NULL || strlen(tmpdir) == 0)
        tmpdir = PATH_STRING "tmp" PATH_STRING;
    if (IS_PATH_SEP(tmpdir[strlen(tmpdir) - 1]))
        snprintf(filename, sizeof(filename), "%sXXXXXX.%s", tmpdir, ext);
    else
        snprintf(filename, sizeof(filename), "%s" PATH_STRING "XXXXXX.%s", tmpdir, ext);

    fd = tmdy_mkstemp(filename);

    if (fd == -1)
        return NULL;

    if ((fp = fdopen(fd, "w")) == NULL)
    {
        close(fd);
        unlink(filename);
        return NULL;
    }

    while ((n = url_read(url, buff, sizeof(buff))) > 0)
    {
        size_t dummy = fwrite(buff, 1, n, fp);
        ++dummy;
    }
    fclose(fp);
    return safe_strdup(filename);
}

/* Try to open a file for reading. If the filename ends in one of the
   defined compressor extensions, pipe the file through the decompressor */
struct midi_file *try_to_open(char *name, int decompress)
{
    struct midi_file *tf;
    URL url;
    int len;

    //if ((url = url_arc_open(name)) == NULL)
    if ((url = url_open(name)) == NULL)
        return NULL;

    tf = (struct midi_file *)safe_malloc(sizeof(struct midi_file));
    tf->url = url;
    tf->tmpname = NULL;

    len = strlen(name);
#if defined(DECOMPRESSOR_LIST)
    if (decompress)
    {
        static char *decompressor_list[] = DECOMPRESSOR_LIST, **dec;
        char tmp[1024];

        /* Check if it's a compressed file */
        for (dec = decompressor_list; *dec; dec += 2)
        {
            if (!check_file_extension(name, *dec, 0))
                continue;

            tf->tmpname = url_dumpfile(tf->url, *dec);
            if (tf->tmpname == NULL)
            {
                close_file(tf);
                return NULL;
            }

            url_close(tf->url);
            snprintf(tmp, sizeof(tmp), *(dec + 1), tf->tmpname);
            if ((tf->url = url_pipe_open(tmp)) == NULL)
            {
                close_file(tf);
                return NULL;
            }

            break;
        }
    }
#endif /* DECOMPRESSOR_LIST */

#if defined(PATCH_CONVERTERS)
    if (decompress == 2)
    {
        static char *decompressor_list[] = PATCH_CONVERTERS, **dec;
        char tmp[1024];

        /* Check if it's a compressed file */
        for (dec = decompressor_list; *dec; dec += 2)
        {
            if (!check_file_extension(name, *dec, 0))
                continue;

            tf->tmpname = url_dumpfile(tf->url, *dec);
            if (tf->tmpname == NULL)
            {
                close_file(tf);
                return NULL;
            }

            url_close(tf->url);
            sprintf(tmp, *(dec + 1), tf->tmpname);
            if ((tf->url = url_pipe_open(tmp)) == NULL)
            {
                close_file(tf);
                return NULL;
            }

            break;
        }
    }
#endif /* PATCH_CONVERTERS */

    return tf;
}

int is_url_prefix(const char *name)
{
    int i;

    static char *url_proto_names[] =
        {
            "file:",
            "mime:",
            NULL};
    for (i = 0; url_proto_names[i]; i++)
        if (strncmp(name, url_proto_names[i], strlen(url_proto_names[i])) == 0)
            return 1;
    return 0;
}

static int is_abs_path(const char *name)
{
    if (IS_PATH_SEP(name[0]))
        return 1;
    if (is_url_prefix(name))
        return 1; /* assuming relative notation is excluded */
    return 0;
}

struct midi_file *open_with_mem(char *mem, int32 memlen, int noise_mode)
{
    URL url;
    struct midi_file *tf;

    errno = 0;
    if ((url = url_mem_open(mem, memlen, 0)) == NULL)
    {
        if (noise_mode >= 2)
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Can't open.");
        return NULL;
    }
    tf = (struct midi_file *)safe_malloc(sizeof(struct midi_file));
    tf->url = url;
    tf->tmpname = NULL;
    return tf;
}

/*
 * This is meant to find and open files for reading, possibly piping
 * them through a decompressor.
 */
struct midi_file *open_file(char *name, int decompress, int noise_mode)
{
    struct midi_file *tf;
    PathList *plp = pathlist;
    int l;

    open_file_noise_mode = noise_mode;
    if (!name || !(*name))
    {
        if (noise_mode)
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Attempted to open nameless file.");
        return 0;
    }
    /* First try the given name */
    strncpy(current_filename, url_unexpand_home_dir(name), 1023);
    current_filename[1023] = '\0';
    if (noise_mode)
        ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);
    if ((tf = try_to_open(current_filename, decompress)))
        return tf;
    if (errno && errno != ENOENT)
    {
        if (noise_mode)
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename, strerror(errno));
        return 0;
    }
    if (!is_abs_path(name))
        while (plp)
        { /* Try along the path then */
            *current_filename = 0;
            if ((l = strlen(plp->path)))
            {
                strncpy(current_filename, plp->path,
                        sizeof(current_filename));
                if (!IS_PATH_SEP(current_filename[l - 1]) && current_filename[l - 1] != '#' && name[0] != '#')
                    strncat(current_filename, PATH_STRING,
                            sizeof(current_filename) - strlen(current_filename) - 1);
            }
            strncat(current_filename, name, sizeof(current_filename) - strlen(current_filename) - 1);
            if (noise_mode)
                ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);
            if ((tf = try_to_open(current_filename, decompress)))
                return tf;
            if (errno && errno != ENOENT)
            {
                if (noise_mode)
                    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename, strerror(errno));
                return 0;
            }
            plp = plp->next;
        }
    /* Nothing could be opened. */
    *current_filename = 0;
    if (noise_mode >= 2)
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", name, (errno) ? strerror(errno) : "Can't open file");
    return 0;
}

/*
 * This is meant to find and open regular files for reading, possibly
 * piping them through a decompressor.
 */
struct midi_file *open_file_r(char *name, int decompress, int noise_mode)
{
    struct stat st;
    struct midi_file *tf;
    PathList *plp = pathlist;
    int l;

    open_file_noise_mode = noise_mode;
    if (!name || !(*name))
    {
        if (noise_mode)
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Attempted to open nameless file.");
        return 0;
    }
    /* First try the given name */
    strncpy(current_filename, url_unexpand_home_dir(name), 1023);
    current_filename[1023] = '\0';
    if (noise_mode)
        ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);
    if (!stat(current_filename, &st))
        if (!S_ISDIR(st.st_mode))
            if ((tf = try_to_open(current_filename, decompress)))
                return tf;
    if (errno && errno != ENOENT)
    {
        if (noise_mode)
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename, strerror(errno));
        return 0;
    }
    if (!is_abs_path(name))
        while (plp)
        { /* Try along the path then */
            *current_filename = 0;
            if ((l = strlen(plp->path)))
            {
                strncpy(current_filename, plp->path,
                        sizeof(current_filename));
                if (!IS_PATH_SEP(current_filename[l - 1]) && current_filename[l - 1] != '#' && name[0] != '#')
                    strncat(current_filename, PATH_STRING,
                            sizeof(current_filename) - strlen(current_filename) - 1);
            }
            strncat(current_filename, name, sizeof(current_filename) - strlen(current_filename) - 1);
            if (noise_mode)
                ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);
            if (!stat(current_filename, &st))
                if (!S_ISDIR(st.st_mode))
                    if ((tf = try_to_open(current_filename, decompress)))
                        return tf;
            if (errno && errno != ENOENT)
            {
                if (noise_mode)
                    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename, strerror(errno));
                return 0;
            }
            plp = plp->next;
        }
    /* Nothing could be opened. */
    *current_filename = 0;
    if (noise_mode >= 2)
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", name, (errno) ? strerror(errno) : "Can't open file");
    return 0;
}

/* This closes files opened with open_file */
void close_file(struct midi_file *tf)
{
    int save_errno = errno;
    if (tf->url != NULL)
    {
        if (tf->tmpname != NULL)
        {
            int i;
            /* dispose the pipe garbage */
            for (i = 0; tf_getc(tf) != EOF && i < 0xFFFF; i++)
                ;
        }
        url_close(tf->url);
    }
    if (tf->tmpname != NULL)
    {
        unlink(tf->tmpname); /* remove temporary file */
        free(tf->tmpname);
    }
    free(tf);
    errno = save_errno;
}

/* This is meant for skipping a few bytes. */
void skip(struct midi_file *tf, size_t len)
{
    url_skip(tf->url, (long)len);
}

char *tf_gets(char *buff, int n, struct midi_file *tf)
{
    return url_gets(tf->url, buff, n);
}

long tf_read(void *buff, int32 size, int32 nitems, struct midi_file *tf)
{
    return url_nread(tf->url, buff, size * nitems) / size;
}

long tf_seek(struct midi_file *tf, long offset, int whence)
{
    long prevpos;

    prevpos = url_seek(tf->url, offset, whence);
    if (prevpos == -1)
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Warning: Can't seek file position");
    return prevpos;
}

long tf_tell(struct midi_file *tf)
{
    long pos;

    pos = url_tell(tf->url);
    if (pos == -1)
    {
        ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Warning: Can't get current file position");
        return (long)tf->url->nread;
    }

    return pos;
}

void safe_exit(int status)
{
    if (play_mode->fd != -1)
    {
        play_mode->acntl(PM_REQ_DISCARD, NULL);
        play_mode->close_output();
    }
    ctl->close();
    //wrdt->close();
    exit(status);
    /*NOTREACHED*/
}

/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
    void *p;
    static int errflag = 0;

    if (errflag)
        safe_exit(10);
    if (count > MAX_SAFE_MALLOC_SIZE)
    {
        errflag = 1;
        ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Strange, I feel like allocating %d bytes. This must be a bug.", count);
    }
    else
    {
        if (count == 0)
            /* Some malloc routine return NULL if count is zero, such as
             * malloc routine from libmalloc.a of Solaris.
             * But it doesn't want to return NULL even if count is zero.
             */
            count = 1;
        if ((p = (void *)malloc(count)) != NULL)
            return p;
        errflag = 1;
        ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Sorry. Couldn't malloc %d bytes.", count);
    }
#ifdef ABORT_AT_FATAL
    abort();
#endif /* ABORT_AT_FATAL */
    safe_exit(10);
    /*NOTREACHED*/
    return 0;
}

void *safe_large_malloc(size_t count)
{
    void *p;
    static int errflag = 0;

    if (errflag)
        safe_exit(10);
    if (count == 0)
        /* Some malloc routine return NULL if count is zero, such as
         * malloc routine from libmalloc.a of Solaris.
         * But it doesn't want to return NULL even if count is zero.
         */
        count = 1;
    if ((p = (void *)malloc(count)) != NULL)
        return p;
    errflag = 1;
    ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Sorry. Couldn't malloc %d bytes.", count);

#ifdef ABORT_AT_FATAL
    abort();
#endif /* ABORT_AT_FATAL */
    safe_exit(10);
    /*NOTREACHED*/
    return 0;
}

void *safe_realloc(void *ptr, size_t count)
{
    void *p;
    static int errflag = 0;

    if (errflag)
        safe_exit(10);
    if (count > MAX_SAFE_MALLOC_SIZE)
    {
        errflag = 1;
        ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Strange, I feel like allocating %d bytes. This must be a bug.", count);
    }
    else
    {
        if (ptr == NULL)
            return safe_malloc(count);
        if (count == 0)
            /* Some malloc routine return NULL if count is zero, such as
             * malloc routine from libmalloc.a of Solaris.
             * But it doesn't want to return NULL even if count is zero.
             */
            count = 1;
        if ((p = (void *)realloc(ptr, count)) != NULL)
            return p;
        errflag = 1;
        ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Sorry. Couldn't malloc %d bytes.", count);
    }
#ifdef ABORT_AT_FATAL
    abort();
#endif /* ABORT_AT_FATAL */
    safe_exit(10);
    /*NOTREACHED*/
    return 0;
}

/* This'll allocate memory or die. */
char *safe_strdup(const char *s)
{
    char *p;
    static int errflag = 0;

    if (errflag)
        safe_exit(10);

    if (s == NULL)
        p = strdup("");
    else
        p = strdup(s);
    if (p != NULL)
        return p;
    errflag = 1;
    ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Sorry. Couldn't alloc memory.");
#ifdef ABORT_AT_FATAL
    abort();
#endif /* ABORT_AT_FATAL */
    safe_exit(10);
    /*NOTREACHED*/
    return 0;
}

/* free ((void **)ptr_list)[0..count-1] and ptr_list itself */
void free_ptr_list(void *ptr_list, int count)
{
    int i;
    for (i = 0; i < count; i++)
        free(((void **)ptr_list)[i]);
    free(ptr_list);
}

static int atoi_limited(const char *string, int v_min, int v_max)
{
    int value = atoi(string);

    if (value <= v_min)
        value = v_min;
    else if (value > v_max)
        value = v_max;
    return value;
}

int string_to_7bit_range(const char *string_, int *start, int *end)
{
    const char *string = string_;

    if (isdigit(*string))
    {
        *start = atoi_limited(string, 0, 127);
        while (isdigit(*++string))
            ;
    }
    else
        *start = 0;
    if (*string == '-')
    {
        string++;
        *end = isdigit(*string) ? atoi_limited(string, 0, 127) : 127;
        if (*start > *end)
            *end = *start;
    }
    else
        *end = *start;
    return string != string_;
}

/* This adds a directory to the path list */
void add_to_pathlist(char *s)
{
    PathList *cur, *prev, *plp;

    /* Check duplicated path in the pathlist. */
    plp = prev = NULL;
    for (cur = pathlist; cur; prev = cur, cur = cur->next)
        if (pathcmp(s, cur->path, 0) == 0)
        {
            plp = cur;
            break;
        }

    if (plp) /* found */
    {
        if (prev == NULL) /* first */
            pathlist = pathlist->next;
        else
            prev->next = plp->next;
    }
    else
    {
        /* Allocate new path */
        plp = safe_malloc(sizeof(PathList));
        plp->path = safe_strdup(s);
    }

    plp->next = pathlist;
    pathlist = plp;
}

void clean_up_pathlist(void)
{
    PathList *cur, *next;

    cur = pathlist;
    while (cur)
    {
        next = cur->next;
#ifdef DEFAULT_PATH
        if (cur == &defaultpathlist)
        {
            cur = next;
            continue;
        }
#endif
        free(cur->path);
        free(cur);
        cur = next;
    }

#ifdef DEFAULT_PATH
    pathlist = &defaultpathlist;
#else
    pathlist = NULL;
#endif
}

/*ARGSUSED*/
int volatile_touch(void *dmy) { return 1; }

/* code converters */
static unsigned char
    w2k[] = {
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        225, 226, 247, 231, 228, 229, 246, 250, 233, 234, 235, 236, 237, 238, 239, 240,
        242, 243, 244, 245, 230, 232, 227, 254, 251, 253, 255, 249, 248, 252, 224, 241,
        193, 194, 215, 199, 196, 197, 214, 218, 201, 202, 203, 204, 205, 206, 207, 208,
        210, 211, 212, 213, 198, 200, 195, 222, 219, 221, 223, 217, 216, 220, 192, 209
    };

static void code_convert_cp1251(char *in, char *out, int maxlen)
{
    int i;
    if (out == NULL)
        out = in;
    for (i = 0; i < maxlen && in[i]; i++)
    {
        if (in[i] & 0200)
            out[i] = w2k[in[i] & 0177];
        else
            out[i] = in[i];
    }
    out[i] = '\0';
}

static void code_convert_dump(char *in, char *out, int maxlen, char *ocode)
{
    if (ocode == NULL)
        ocode = output_text_code;

    if (ocode != NULL && ocode != (char *)-1 && (strstr(ocode, "ASCII") || strstr(ocode, "ascii")))
    {
        int i;

        if (out == NULL)
            out = in;
        for (i = 0; i < maxlen && in[i]; i++)
            if (in[i] < ' ' || in[i] >= 127)
                out[i] = '.';
            else
                out[i] = in[i];
        out[i] = '\0';
    }
    else /* "NOCNV" */
    {
        if (out == NULL)
            return;
        strncpy(out, in, maxlen);
        out[maxlen] = '\0';
    }
}

void code_convert(char *in, char *out, int outsiz, char *icode, char *ocode)
{
    if (ocode != NULL && ocode != (char *)-1)
    {
        if (strcasecmp(ocode, "nocnv") == 0)
        {
            if (out == NULL)
                return;
            outsiz--;
            strncpy(out, in, outsiz);
            out[outsiz] = '\0';
            return;
        }

        if (strcasecmp(ocode, "ascii") == 0)
        {
            code_convert_dump(in, out, outsiz - 1, "ASCII");
            return;
        }

        if (strcasecmp(ocode, "1251") == 0)
        {
            code_convert_cp1251(in, out, outsiz - 1);
            return;
        }
    }

    code_convert_dump(in, out, outsiz - 1, ocode);
}

/* EAW -- insert stuff from playlist files
 *
 * Tue Apr 6 1999: Modified by Masanao Izumo <mo@goice.co.jp>
 *                 One pass implemented.
 */
static char **expand_file_lists(char **files, int *nfiles_in_out)
{
    int nfiles;
    int i;
    char input_line[256];
    char *pfile;
    static const char *testext = ".m3u.pls.asx.M3U.PLS.ASX.tpl";
    struct midi_file *list_file;
    char *one_file[1];
    int one;

    /* Recusive global */
    static StringTable st;
    static int error_outflag = 0;
    static int depth = 0;

    if (depth >= 16)
    {
        if (!error_outflag)
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Probable loop in playlist files");
            error_outflag = 1;
        }
        return NULL;
    }

    if (depth == 0)
    {
        error_outflag = 0;
        init_string_table(&st);
    }
    nfiles = *nfiles_in_out;

    /* Expand playlist recursively */
    for (i = 0; i < nfiles; i++)
    {
        /* extract the file extension */
        pfile = strrchr(files[i], '.');

        if (*files[i] == '@' || (pfile != NULL && strstr(testext, pfile)))
        {
            /* Playlist file */
            if (*files[i] == '@')
                list_file = open_file(files[i] + 1, 1, 1);
            else
                list_file = open_file(files[i], 1, 1);
            if (list_file)
            {
                while (tf_gets(input_line, sizeof(input_line), list_file) != NULL)
                {
                    if (*input_line == '\n' || *input_line == '\r')
                        continue;
                    if ((pfile = strchr(input_line, '\r')))
                        *pfile = '\0';
                    if ((pfile = strchr(input_line, '\n')))
                        *pfile = '\0';
                    one_file[0] = input_line;
                    one = 1;
                    depth++;
                    expand_file_lists(one_file, &one);
                    depth--;
                }
                close_file(list_file);
            }
        }
        else /* Other file */
            put_string_table(&st, files[i], strlen(files[i]));
    }

    if (depth)
        return NULL;
    *nfiles_in_out = st.nstring;
    return make_string_array(&st);
}

#ifdef RAND_MAX
int int_rand(int n)
{
    if (n < 0)
    {
        if (n == -1)
            srand(time(NULL));
        else
            srand(-n);
        return n;
    }
    return (int)(n * (double)rand() * (1.0 / (RAND_MAX + 1.0)));
}
#else
int int_rand(int n)
{
    static unsigned long rnd_seed = 0xabcd0123;

    if (n < 0)
    {
        if (n == -1)
            rnd_seed = time(NULL);
        else
            rnd_seed = -n;
        return n;
    }

    rnd_seed *= 69069UL;
    return (int)(n * (double)(rnd_seed & 0xffffffff) * (1.0 / (0xffffffff + 1.0)));
}
#endif /* RAND_MAX */

int check_file_extension(char *filename, char *ext, int decompress)
{
    int len, elen, i;
#if defined(DECOMPRESSOR_LIST)
    char *dlist[] = DECOMPRESSOR_LIST;
#endif /* DECOMPRESSOR_LIST */

    len = strlen(filename);
    elen = strlen(ext);
    if (len > elen && strncasecmp(filename + len - elen, ext, elen) == 0)
        return 1;

    if (decompress)
    {
        /* Check gzip'ed file name */

        if (len > 3 + elen &&
            strncasecmp(filename + len - elen - 3, ext, elen) == 0 &&
            strncasecmp(filename + len - 3, ".gz", 3) == 0)
            return 1;

#if defined(DECOMPRESSOR_LIST)
        for (i = 0; dlist[i]; i += 2)
        {
            int dlen;

            dlen = strlen(dlist[i]);
            if (len > dlen + elen &&
                strncasecmp(filename + len - elen - dlen, ext, elen) == 0 &&
                strncasecmp(filename + len - dlen, dlist[i], dlen) == 0)
                return 1;
        }
#endif /* DECOMPRESSOR_LIST */
    }
    return 0;
}

void randomize_string_list(char **strlist, int n)
{
    int i, j;
    char *tmp;
    for (i = 0; i < n; i++)
    {
        j = int_rand(n - i);
        tmp = strlist[j];
        strlist[j] = strlist[n - i - 1];
        strlist[n - i - 1] = tmp;
    }
}

int pathcmp(const char *p1, const char *p2, int ignore_case)
{
    int c1, c2;

    do
    {
        c1 = *p1++ & 0xff;
        c2 = *p2++ & 0xff;
        if (ignore_case)
        {
            c1 = tolower(c1);
            c2 = tolower(c2);
        }
        if (IS_PATH_SEP(c1))
            c1 = *p1 ? 0x100 : 0;
        if (IS_PATH_SEP(c2))
            c2 = *p2 ? 0x100 : 0;
    } while (c1 == c2 && c1 /* && c2 */);

    return c1 - c2;
}

static int pathcmp_qsort(const char **p1,
                         const char **p2)
{
    return pathcmp(*(const char **)p1, *(const char **)p2, 1);
}

void sort_pathname(char **files, int nfiles)
{
    qsort(files, nfiles, sizeof(char *),
          (int (*)(const void *, const void *))pathcmp_qsort);
}

char *pathsep_strchr(const char *path)
{
#ifdef PATH_SEP2
    while (*path)
    {
        if (*path == PATH_SEP || *path == PATH_SEP2)
            return (char *)path;
        path++;
    }
    return NULL;
#else
    return strchr(path, PATH_SEP);
#endif
}

char *pathsep_strrchr(const char *path)
{
#ifdef PATH_SEP2
    char *last_sep = NULL;
    while (*path)
    {
        if (*path == PATH_SEP || *path == PATH_SEP2)
            last_sep = (char *)path;
        path++;
    }
    return last_sep;
#else
    return strrchr(path, PATH_SEP);
#endif
}

int str2mID(char *str)
{
    int val;

    if (strncasecmp(str, "gs", 2) == 0)
        val = 0x41;
    else if (strncasecmp(str, "xg", 2) == 0)
        val = 0x43;
    else if (strncasecmp(str, "gm", 2) == 0)
        val = 0x7e;
    else
    {
        int i, v;
        val = 0;
        for (i = 0; i < 2; i++)
        {
            v = str[i];
            if ('0' <= v && v <= '9')
                v = v - '0';
            else if ('A' <= v && v <= 'F')
                v = v - 'A' + 10;
            else if ('a' <= v && v <= 'f')
                v = v - 'a' + 10;
            else
                return 0;
            val = (val << 4 | v);
        }
    }
    return val;
}

int load_table(char *file)
{
    FILE *fp;
    char tmp[1024], *value;
    int i = 0;

    if ((fp = fopen(file, "r")) == NULL)
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Can't read %s %s\n", file, strerror(errno));
        return -1;
    }
    while (fgets(tmp, sizeof(tmp), fp))
    {
        if (strchr(tmp, '#'))
            continue;
        if (!(value = strtok(tmp, ", \n")))
            continue;
        do
        {
            freq_table_zapped[i++] = atoi(value);
            if (i == 128)
            {
                fclose(fp);
                return 0;
            }
        } while ((value = strtok(NULL, ", \n")));
    }
    fclose(fp);
    return 0;
}

static char *expand_variables(char *string, MBlockList *varbuf, const char *basedir)
{
    char *p, *expstr;
    const char *copystr;
    int limlen, copylen, explen, varlen, braced;

    if ((p = strchr(string, '$')) == NULL)
        return string;
    varlen = strlen(basedir);
    explen = limlen = 0;
    expstr = NULL;
    copystr = string;
    copylen = p - string;
    string = p;
    for (;;)
    {
        if (explen + copylen + 1 > limlen)
        {
            limlen += copylen + 128;
            expstr = memcpy(new_segment(varbuf, limlen), expstr, explen);
        }
        memcpy(&expstr[explen], copystr, copylen);
        explen += copylen;
        if (*string == '\0')
            break;
        else if (*string == '$')
        {
            braced = *++string == '{';
            if (braced)
            {
                if ((p = strchr(string + 1, '}')) == NULL)
                    p = string; /* no closing brace */
                else
                    string++;
            }
            else
                for (p = string; isalnum(*p) || *p == '_'; p++)
                    ;
            if (p == string) /* empty */
            {
                copystr = "${";
                copylen = 1 + braced;
            }
            else
            {
                if (p - string == 7 && memcmp(string, "basedir", 7) == 0)
                {
                    copystr = basedir;
                    copylen = varlen;
                }
                else /* undefined variable */
                    copylen = 0;
                string = p + braced;
            }
        }
        else /* search next */
        {
            p = strchr(string, '$');
            if (p == NULL)
                copylen = strlen(string);
            else
                copylen = p - string;
            copystr = string;
            string += copylen;
        }
    }
    expstr[explen] = '\0';
    return expstr;
}

/* string[0] should not be '#' */
static int strip_trailing_comment(char *string, int next_token_index)
{
    if (string[next_token_index - 1] == '#' /* strip \1 in /^\S+(#*[ \t].*)/ */
        && (string[next_token_index] == ' ' || string[next_token_index] == '\t'))
    {
        string[next_token_index] = '\0'; /* new c-string terminator */
        while (string[--next_token_index - 1] == '#')
            ;
    }
    return next_token_index;
}

typedef struct
{
    const char *name;
    int mapid, isdrum;
} MapNameEntry;

static int mapnamecompare(const void *name, const void *entry)
{
    return strcmp((const char *)name, ((const MapNameEntry *)entry)->name);
}

static int mapname2id(char *name, int *isdrum)
{
    static const MapNameEntry data[] = {
        /* sorted in alphabetical order */
        {"gm2", GM2_TONE_MAP, 0},
        {"gm2drum", GM2_DRUM_MAP, 1},
        {"sc55", SC_55_TONE_MAP, 0},
        {"sc55drum", SC_55_DRUM_MAP, 1},
        {"sc88", SC_88_TONE_MAP, 0},
        {"sc8850", SC_8850_TONE_MAP, 0},
        {"sc8850drum", SC_8850_DRUM_MAP, 1},
        {"sc88drum", SC_88_DRUM_MAP, 1},
        {"sc88pro", SC_88PRO_TONE_MAP, 0},
        {"sc88prodrum", SC_88PRO_DRUM_MAP, 1},
        {"xg", XG_NORMAL_MAP, 0},
        {"xgdrum", XG_DRUM_MAP, 1},
        {"xgsfx126", XG_SFX126_MAP, 1},
        {"xgsfx64", XG_SFX64_MAP, 0}};
    const MapNameEntry *found;

    found = (MapNameEntry *)bsearch(name, data, sizeof data / sizeof data[0], sizeof data[0], mapnamecompare);
    if (found != NULL)
    {
        *isdrum = found->isdrum;
        return found->mapid;
    }
    return -1;
}

static float *config_parse_tune(const char *cp, int *num)
{
    const char *p;
    float *tune_list;
    int i;

    /* count num */
    *num = 1, p = cp;
    while ((p = strchr(p, ',')) != NULL)
        (*num)++, p++;
    /* alloc */
    tune_list = (float *)safe_malloc((*num) * sizeof(float));
    /* regist */
    for (i = 0, p = cp; i < *num; i++, p++)
    {
        tune_list[i] = atof(p);
        if (!(p = strchr(p, ',')))
            break;
    }
    return tune_list;
}

static int16 *config_parse_int16(const char *cp, int *num)
{
    const char *p;
    int16 *list;
    int i;

    /* count num */
    *num = 1, p = cp;
    while ((p = strchr(p, ',')) != NULL)
        (*num)++, p++;
    /* alloc */
    list = (int16 *)safe_malloc((*num) * sizeof(int16));
    /* regist */
    for (i = 0, p = cp; i < *num; i++, p++)
    {
        list[i] = atoi(p);
        if (!(p = strchr(p, ',')))
            break;
    }
    return list;
}

static int **config_parse_envelope(const char *cp, int *num)
{
    const char *p, *px;
    int **env_list;
    int i, j;

    /* count num */
    *num = 1, p = cp;
    while ((p = strchr(p, ',')) != NULL)
        (*num)++, p++;
    /* alloc */
    env_list = (int **)safe_malloc((*num) * sizeof(int *));
    for (i = 0; i < *num; i++)
        env_list[i] = (int *)safe_malloc(6 * sizeof(int));
    /* init */
    for (i = 0; i < *num; i++)
        for (j = 0; j < 6; j++)
            env_list[i][j] = -1;
    /* regist */
    for (i = 0, p = cp; i < *num; i++, p++)
    {
        px = strchr(p, ',');
        for (j = 0; j < 6; j++, p++)
        {
            if (*p == ':')
                continue;
            env_list[i][j] = atoi(p);
            if (!(p = strchr(p, ':')))
                break;
            if (px && p > px)
                break;
        }
        if (!(p = px))
            break;
    }
    return env_list;
}

static Quantity **config_parse_modulation(const char *name, int line, const char *cp, int *num, int mod_type)
{
    const char *p, *px, *err;
    char buf[128], *delim;
    Quantity **mod_list;
    int i, j;
    static const char *qtypestr[] = {"tremolo", "vibrato"};
    static const uint16 qtypes[] = {
        QUANTITY_UNIT_TYPE(TREMOLO_SWEEP), QUANTITY_UNIT_TYPE(TREMOLO_RATE), QUANTITY_UNIT_TYPE(DIRECT_INT),
        QUANTITY_UNIT_TYPE(VIBRATO_SWEEP), QUANTITY_UNIT_TYPE(VIBRATO_RATE), QUANTITY_UNIT_TYPE(DIRECT_INT)};

    /* count num */
    *num = 1, p = cp;
    while ((p = strchr(p, ',')) != NULL)
        (*num)++, p++;
    /* alloc */
    mod_list = (Quantity **)safe_malloc((*num) * sizeof(Quantity *));
    for (i = 0; i < *num; i++)
        mod_list[i] = (Quantity *)safe_malloc(3 * sizeof(Quantity));
    /* init */
    for (i = 0; i < *num; i++)
        for (j = 0; j < 3; j++)
            INIT_QUANTITY(mod_list[i][j]);
    buf[sizeof buf - 1] = '\0';
    /* regist */
    for (i = 0, p = cp; i < *num; i++, p++)
    {
        px = strchr(p, ',');
        for (j = 0; j < 3; j++, p++)
        {
            if (*p == ':')
                continue;
            if ((delim = strpbrk(strncpy(buf, p, sizeof buf - 1), ":,")) != NULL)
                *delim = '\0';
            if (*buf != '\0' && (err = string_to_quantity(buf, &mod_list[i][j], qtypes[mod_type * 3 + j])) != NULL)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: %s: parameter %d of item %d: %s (%s)", name, line, qtypestr[mod_type], j + 1, i + 1, err, buf);
                free_ptr_list(mod_list, *num);
                mod_list = NULL;
                *num = 0;
                return NULL;
            }
            if (!(p = strchr(p, ':')))
                break;
            if (px && p > px)
                break;
        }
        if (!(p = px))
            break;
    }
    return mod_list;
}

static int set_gus_patchconf_opts(char *name, int line, char *opts, ToneBankElement *tone)
{
    char *cp;
    int k;

    if (!(cp = strchr(opts, '=')))
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s", name, line, opts);
        return 1;
    }
    *cp++ = 0;
    if (!strcmp(opts, "amp"))
    {
        k = atoi(cp);
        if ((k < 0 || k > MAX_AMPLIFICATION) || (*cp < '0' || *cp > '9'))
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: amplification must be between 0 and %d", name, line, MAX_AMPLIFICATION);
            return 1;
        }
        tone->amp = k;
    }
    else if (!strcmp(opts, "note"))
    {
        k = atoi(cp);
        if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9'))
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: note must be between 0 and 127", name, line);
            return 1;
        }
        tone->note = k;
        tone->scltune = config_parse_int16("100", &tone->scltunenum);
    }
    else if (!strcmp(opts, "pan"))
    {
        if (!strcmp(cp, "center"))
            k = 64;
        else if (!strcmp(cp, "left"))
            k = 0;
        else if (!strcmp(cp, "right"))
            k = 127;
        else
        {
            k = ((atoi(cp) + 100) * 100) / 157;
            if ((k < 0 || k > 127) || (k == 0 && *cp != '-' && (*cp < '0' || *cp > '9')))
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: panning must be left, right, center, or between -100 and 100", name, line);
                return 1;
            }
        }
        tone->pan = k;
    }
    else if (!strcmp(opts, "tune"))
        tone->tune = config_parse_tune(cp, &tone->tunenum);
    else if (!strcmp(opts, "rate"))
        tone->envrate = config_parse_envelope(cp, &tone->envratenum);
    else if (!strcmp(opts, "offset"))
        tone->envofs = config_parse_envelope(cp, &tone->envofsnum);
    else if (!strcmp(opts, "keep"))
    {
        if (!strcmp(cp, "env"))
            tone->strip_envelope = 0;
        else if (!strcmp(cp, "loop"))
            tone->strip_loop = 0;
        else
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: keep must be env or loop", name, line);
            return 1;
        }
    }
    else if (!strcmp(opts, "strip"))
    {
        if (!strcmp(cp, "env"))
            tone->strip_envelope = 1;
        else if (!strcmp(cp, "loop"))
            tone->strip_loop = 1;
        else if (!strcmp(cp, "tail"))
            tone->strip_tail = 1;
        else
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: strip must be env, loop, or tail", name, line);
            return 1;
        }
    }
    else if (!strcmp(opts, "tremolo"))
    {
        if ((tone->trem = config_parse_modulation(name, line, cp, &tone->tremnum, 0)) == NULL)
            return 1;
    }
    else if (!strcmp(opts, "vibrato"))
    {
        if ((tone->vib = config_parse_modulation(name, line, cp, &tone->vibnum, 1)) == NULL)
            return 1;
    }
    else if (!strcmp(opts, "sclnote"))
        tone->sclnote = config_parse_int16(cp, &tone->sclnotenum);
    else if (!strcmp(opts, "scltune"))
        tone->scltune = config_parse_int16(cp, &tone->scltunenum);
    else if (!strcmp(opts, "comm"))
    {
        char *p;

        if (tone->comment)
            free(tone->comment);
        p = tone->comment = safe_strdup(cp);
        while (*p)
        {
            if (*p == ',')
                *p = ' ';
            p++;
        }
    }
    else if (!strcmp(opts, "modrate"))
        tone->modenvrate = config_parse_envelope(cp, &tone->modenvratenum);
    else if (!strcmp(opts, "modoffset"))
        tone->modenvofs = config_parse_envelope(cp, &tone->modenvofsnum);
    else if (!strcmp(opts, "envkeyf"))
        tone->envkeyf = config_parse_envelope(cp, &tone->envkeyfnum);
    else if (!strcmp(opts, "envvelf"))
        tone->envvelf = config_parse_envelope(cp, &tone->envvelfnum);
    else if (!strcmp(opts, "modkeyf"))
        tone->modenvkeyf = config_parse_envelope(cp, &tone->modenvkeyfnum);
    else if (!strcmp(opts, "modvelf"))
        tone->modenvvelf = config_parse_envelope(cp, &tone->modenvvelfnum);
    else if (!strcmp(opts, "trempitch"))
        tone->trempitch = config_parse_int16(cp, &tone->trempitchnum);
    else if (!strcmp(opts, "tremfc"))
        tone->tremfc = config_parse_int16(cp, &tone->tremfcnum);
    else if (!strcmp(opts, "modpitch"))
        tone->modpitch = config_parse_int16(cp, &tone->modpitchnum);
    else if (!strcmp(opts, "modfc"))
        tone->modfc = config_parse_int16(cp, &tone->modfcnum);
    else if (!strcmp(opts, "fc"))
        tone->fc = config_parse_int16(cp, &tone->fcnum);
    else if (!strcmp(opts, "q"))
        tone->reso = config_parse_int16(cp, &tone->resonum);
    else if (!strcmp(opts, "fckeyf")) /* filter key-follow */
        tone->key_to_fc = atoi(cp);
    else if (!strcmp(opts, "fcvelf")) /* filter velocity-follow */
        tone->vel_to_fc = atoi(cp);
    else if (!strcmp(opts, "qvelf")) /* resonance velocity-follow */
        tone->vel_to_resonance = atoi(cp);
    else
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s", name, line, opts);
        return 1;
    }
    return 0;
}

static void reinit_tone_bank_element(ToneBankElement *tone)
{
    free_tone_bank_element(tone);
    tone->note = tone->pan = -1;
    tone->strip_loop = tone->strip_envelope = tone->strip_tail = -1;
    tone->amp = -1;
    tone->rnddelay = 0;
    tone->loop_timeout = 0;
    tone->legato = tone->damper_mode = tone->key_to_fc = tone->vel_to_fc = 0;
    tone->reverb_send = tone->chorus_send = tone->delay_send = -1;
    tone->tva_level = -1;
    tone->play_note = -1;
}

#define SET_GUS_PATCHCONF_COMMENT
static int set_gus_patchconf(char *name, int line,
                             ToneBankElement *tone, char *pat, char **opts)
{
    int j;
#ifdef SET_GUS_PATCHCONF_COMMENT
    char *old_name = NULL;

    if (tone != NULL && tone->name != NULL)
        old_name = safe_strdup(tone->name);
#endif
    reinit_tone_bank_element(tone);

    if (strcmp(pat, "%font") == 0) /* Font extention */
    {
        /* %font filename bank prog [note-to-use]
         * %font filename 128 bank key
         */

        if (opts[0] == NULL || opts[1] == NULL || opts[2] == NULL ||
            (atoi(opts[1]) == 128 && opts[3] == NULL))
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Syntax error", name, line);
#ifdef SET_GUS_PATCHCONF_COMMENT
            if (old_name != NULL)
                free(old_name);
#endif
            return 1;
        }
        tone->name = safe_strdup(opts[0]);
        tone->instype = 1;
        if (atoi(opts[1]) == 128) /* drum */
        {
            tone->font_bank = 128;
            tone->font_preset = atoi(opts[2]);
            tone->font_keynote = atoi(opts[3]);
            opts += 4;
        }
        else
        {
            tone->font_bank = atoi(opts[1]);
            tone->font_preset = atoi(opts[2]);

            if (opts[3] && isdigit(opts[3][0]))
            {
                tone->font_keynote = atoi(opts[3]);
                opts += 4;
            }
            else
            {
                tone->font_keynote = -1;
                opts += 3;
            }
        }
    }
    else if (strcmp(pat, "%sample") == 0) /* Sample extention */
    {
        /* %sample filename */

        if (opts[0] == NULL)
        {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Syntax error", name, line);
            return 1;
        }
        tone->name = safe_strdup(opts[0]);
        tone->instype = 2;
        opts++;
    }
    else
    {
        tone->instype = 0;
        tone->name = safe_strdup(pat);
    }

    for (j = 0; opts[j] != NULL; j++)
    {
        int err;
        if ((err = set_gus_patchconf_opts(name, line, opts[j], tone)) != 0)
        {
#ifdef SET_GUS_PATCHCONF_COMMENT
            if (old_name != NULL)
                free(old_name);
#endif
            return err;
        }
        return err;
    }
#ifdef SET_GUS_PATCHCONF_COMMENT
    if (tone->comment == NULL ||
        (old_name != NULL && strcmp(old_name, tone->comment) == 0))
    {
        if (tone->comment != NULL)
            free(tone->comment);
        tone->comment = safe_strdup(tone->name);
    }
    if (old_name != NULL)
        free(old_name);
#else
    if (tone->comment == NULL)
        tone->comment = safe_strdup(tone->name);
#endif
    return 0;
}

static int set_patchconf(char *name, int line, ToneBank *bank, char *w[], int dr, int mapid, int bankmapfrom, int bankno)
{
    int i;

    i = atoi(w[0]);
    if (!dr)
        i -= progbase;
    if (i < 0 || i > 127)
    {
        if (dr)
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Drum number must be between 0 and 127", name, line);
        else
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Program must be between %d and %d", name, line, progbase, 127 + progbase);
        return 1;
    }
    if (!bank)
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Must specify tone bank or drum set before assignment", name, line);
        return 1;
    }

    if (set_gus_patchconf(name, line, &bank->tone[i], w[1], w + 2))
        return 1;
    if (mapid != INST_NO_MAP)
        set_instrument_map(mapid, bankmapfrom, i, bankno, i);
    return 0;
}

#define MAXWORDS 130
#define CHECKERRLIMIT                                          \
    if (++errcnt >= 10)                                        \
    {                                                          \
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL,                     \
                "Too many errors... Give up read %s", name);   \
        reuse_mblock(&varbuf);                                 \
        close_file(tf);                                        \
        return 1;                                              \
    }

int load_font_from_cfg(char* name)
{
    struct midi_file *tf;
    MBlockList varbuf;
    char *basedir, *sep;
    char buf[1024], *tmp, *w[MAXWORDS + 1];
    int line = 0, i = 0, j, words, param_parse_err, errcnt = 0;
    ToneBank *bank = NULL;
    int dr = 0, bankno = 0, mapid = INST_NO_MAP, origbankno = 0x7FFFFFFF;
    tf = open_file(name, 0, OF_VERBOSE);
    if(tf == NULL)
        return -1;
    init_mblock(&varbuf);

    basedir = strdup_mblock(&varbuf, current_filename);
    if (is_url_prefix(basedir))
        sep = strrchr(basedir, '/');
    else
        sep = pathsep_strrchr(basedir);

    errno = 0;
    while (tf_gets(buf, sizeof(buf), tf))
    {
        line++;
        i = 0;
        while (isspace(buf[i])) /* skip /^\s*(?#)/ */
            i++;
        if (buf[i] == '#' || buf[i] == '\0') /* /^#|^$/ */
            continue;
        tmp = expand_variables(buf, &varbuf, basedir);
        j = strcspn(tmp + i, " \t\r\n\240");
        if (j == 0)
            j = strlen(tmp + i);
        j = strip_trailing_comment(tmp + i, j);
        tmp[i + j] = '\0'; /* terminate the first token */
        w[0] = tmp + i;
        i += j + 1;
        words = param_parse_err = 0;
        while (words < MAXWORDS - 1) /* -1 : next arg */
        {
            char *terminator;
            while (isspace(tmp[i])) /* skip /^\s*(?#)/ */
                i++;
            if (tmp[i] == '\0' || tmp[i] == '#') /* /\s#/ */
                break;
            if ((tmp[i] == '"' || tmp[i] == '\'') && (terminator = strchr(tmp + i + 1, tmp[i])) != NULL)
            {
                if (!isspace(terminator[1]) && terminator[1] != '\0')
                {
                    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: there must be at least one whitespace between string terminator (%c) and the next parameter", name, line, tmp[i]);
                    CHECKERRLIMIT;
                    param_parse_err = 1;
                    break;
                }
                w[++words] = tmp + i + 1;
                i = terminator - tmp + 1;
                *terminator = '\0';
            }
            else /* not terminated */
            {
                j = strcspn(tmp + i, " \t\r\n\240");
                if (j > 0)
                    j = strip_trailing_comment(tmp + i, j);
                w[++words] = tmp + i;
                i += j;
                if (tmp[i] != '\0')  /* unless at the end-of-string (i.e. EOF) */
                    tmp[i++] = '\0'; /* terminate the token */
            }
        }
        if (param_parse_err)
            continue;
        w[++words] = NULL;
        if (!strcmp(w[0], "dir"))
        {
            if (words < 2)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No directory given", name, line);
                CHECKERRLIMIT;
                continue;
            }
            for (i = 1; i < words; i++)
                add_to_pathlist(w[i]);
        }
        else if (!strcmp(w[0], "bank"))
        {
            int newmapid, isdrum, newbankno;

            if (words < 2)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No bank number given", name, line);
                CHECKERRLIMIT;
                continue;
            }
            if (words != 2 && !isdigit(*w[1]))
            {
                if ((newmapid = mapname2id(w[1], &isdrum)) == -1 || isdrum)
                {
                    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Invalid bank map name: %s", name, line, w[1]);
                    CHECKERRLIMIT;
                    continue;
                }
                words--;
                memmove(&w[1], &w[2], sizeof w[0] * words);
            }
            else
                newmapid = INST_NO_MAP;
            i = atoi(w[1]);
            if (i < 0 || i > 127)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Tone bank must be between 0 and 127", name, line);
                CHECKERRLIMIT;
                continue;
            }

            newbankno = i;
            i = alloc_instrument_map_bank(0, newmapid, i);
            if (i == -1)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No free tone bank available to map", name, line);
                CHECKERRLIMIT;
                continue;
            }

            if (words == 2)
            {
                bank = tonebank[i];
                bankno = i;
                mapid = newmapid;
                origbankno = newbankno;
                dr = 0;
            }
            else
            {
                if (words < 4 || *w[2] < '0' || *w[2] > '9')
                {
                    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
                    CHECKERRLIMIT;
                    continue;
                }
                if (set_patchconf(name, line, tonebank[i], &w[2], 0, newmapid, newbankno, i))
                {
                    CHECKERRLIMIT;
                    continue;
                }
            }
        }
        /* drumset [mapid] num */
        else if (!strcmp(w[0], "drumset"))
        {
            int newmapid, isdrum, newbankno;

            if (words < 2)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No drum set number given", name, line);
                CHECKERRLIMIT;
                continue;
            }
            if (words != 2 && !isdigit(*w[1]))
            {
                if ((newmapid = mapname2id(w[1], &isdrum)) == -1 || !isdrum)
                {
                    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Invalid drum set map name: %s", name, line, w[1]);
                    CHECKERRLIMIT;
                    continue;
                }
                words--;
                memmove(&w[1], &w[2], sizeof w[0] * words);
            }
            else
                newmapid = INST_NO_MAP;
            i = atoi(w[1]) - progbase;
            if (i < 0 || i > 127)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Drum set must be between %d and %d", name, line, progbase, progbase + 127);
                CHECKERRLIMIT;
                continue;
            }

            newbankno = i;
            i = alloc_instrument_map_bank(1, newmapid, i);
            if (i == -1)
            {
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No free drum set available to map", name, line);
                CHECKERRLIMIT;
                continue;
            }

            if (words == 2)
            {
                bank = drumset[i];
                bankno = i;
                mapid = newmapid;
                origbankno = newbankno;
                dr = 1;
            }
            else
            {
                if (words < 4 || *w[2] < '0' || *w[2] > '9')
                {
                    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
                    CHECKERRLIMIT;
                    continue;
                }
                if (set_patchconf(name, line, drumset[i], &w[2], 1, newmapid, newbankno, i))
                {
                    CHECKERRLIMIT;
                    continue;
                }
            }
        }
        else
        {
            if (words < 2 || *w[0] < '0' || *w[0] > '9')
            {
                //if (extension_flag)
                //    continue;
                ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
                CHECKERRLIMIT;
                continue;
            }
            if (set_patchconf(name, line, bank, w, dr, mapid, origbankno, bankno))
            {
                CHECKERRLIMIT;
                continue;
            }
        }
    }
    if (errno)
    {
        ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Can't read %s: %s", name, strerror(errno));
        errcnt++;
    }
    reuse_mblock(&varbuf);
    close_file(tf);
    return errcnt;
}

#define MIDI_VERSION "2.15.3"
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION MIDI_VERSION
#endif
char *midi_version = PACKAGE_VERSION;
