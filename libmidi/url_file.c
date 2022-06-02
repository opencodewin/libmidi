#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "common.h"

#ifdef _WIN32
#include <windows.h>
#endif /* __W32__ */

#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/mman.h>
#else
#define try_mmap(fname, size_ret) w32_mmap(fname, size_ret, &hFile, &hMap)
#define munmap(addr, size)        w32_munmap(addr, size, hFile, hMap)
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((caddr_t)-1)
#endif /* MAP_FAILED */

#include "url.h"
#if !defined(O_BINARY)
#define O_BINARY 0
#endif

typedef struct _URL_file
{
    char common[sizeof(struct _URL)];

    char *mapptr; /* Non NULL if mmap is success */
    long mapsize;
    long pos;
#ifdef _WIN32
    HANDLE hFile, hMap;
#endif /* _WIN32 */

    FILE *fp; /* Non NULL if mmap is failure */
} URL_file;

static int name_file_check(char *url_string);
static long url_file_read(URL url, void *buff, long n);
static char *url_file_gets(URL url, char *buff, int n);
static int url_file_fgetc(URL url);
static long url_file_seek(URL url, long offset, int whence);
static long url_file_tell(URL url);
static void url_file_close(URL url);

struct URL_module URL_module_file =
    {
        URL_file_t,      /* type */
        name_file_check, /* URL checker */
        NULL,            /* initializer */
        url_file_open,   /* open */
        NULL             /* must be NULL */
};

static int name_file_check(char *s)
{
    int i;

    if (IS_PATH_SEP(s[0]))
        return 1;

    if (strncasecmp(s, "file:", 5) == 0)
        return 1;

#ifdef _WIN32
    /* [A-Za-z]: (for Windows) */
    if ((('A' <= s[0] && s[0] <= 'Z') ||
	    ('a' <= s[0] && s[0] <= 'z')) &&
       s[1] == ':')
	return 1;
#endif /* _WIN32 */

    for (i = 0; s[i] && s[i] != ':' && s[i] != '/'; i++)
        ;
    if (s[i] == ':' && s[i + 1] == '/')
        return 0;

    return 1;
}

#ifndef _WIN32
static char *try_mmap(char *path, long *size)
{
    int fd;
    char *p;
    struct stat st;

    errno = 0;
    fd = open(path, O_RDONLY | O_BINARY);
    if (fd < 0)
        return NULL;

    if (fstat(fd, &st) < 0)
    {
        int save_errno = errno;
        close(fd);
        errno = save_errno;
        return NULL;
    }

    p = (char *)mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == (char *)MAP_FAILED)
    {
        int save_errno = errno;
        close(fd);
        errno = save_errno;
        return NULL;
    }
    close(fd);
    *size = (long)st.st_size;
    return p;
}
#else
static void *w32_mmap(char *fname, long *size_ret, HANDLE *hFilePtr, HANDLE *hMapPtr)
{
    void *map;

    *hFilePtr = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ	, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(*hFilePtr == INVALID_HANDLE_VALUE)
	return NULL;
    *size_ret = GetFileSize(*hFilePtr, NULL);
    if(*size_ret == 0xffffffff)
    {
	CloseHandle(*hFilePtr);
	return NULL;
    }
    *hMapPtr = CreateFileMapping(*hFilePtr, NULL, PAGE_READONLY,
                                0, 0, NULL);
    if(*hMapPtr == NULL)
    {
	CloseHandle(*hFilePtr);
	return NULL;
    }
    map = MapViewOfFile(*hMapPtr, FILE_MAP_READ, 0, 0, 0);
    if(map == NULL)
    {
	CloseHandle(*hMapPtr);
	CloseHandle(*hFilePtr);
	return NULL;
    }
    return map;
}
static void w32_munmap(void *ptr, long size, HANDLE hFile, HANDLE hMap)
{
    UnmapViewOfFile(ptr);
    CloseHandle(hMap);
    CloseHandle(hFile);
}
#endif

URL url_file_open(char *fname)
{
    URL_file *url;
    char *mapptr; /* Non NULL if mmap is success */
    long mapsize;
    FILE *fp; /* Non NULL if mmap is failure */
#ifdef _WIN32
    HANDLE hFile, hMap;
#endif /* __W32__ */

#ifdef DEBUG
    fprintf(stderr, "url_file_open(%s)\n", fname);
#endif /* DEBUG */

    if (!strcmp(fname, "-"))
    {
        mapptr = NULL;
        mapsize = 0;
        fp = stdin;
        goto done;
    }

    if (strncasecmp(fname, "file:", 5) == 0)
        fname += 5;
    if (*fname == '\0')
    {
        url_errno = errno = ENOENT;
        return NULL;
    }
    fname = url_expand_home_dir(fname);

    fp = NULL;
    mapsize = 0;
    errno = 0;
    mapptr = try_mmap(fname, &mapsize);
    if (errno == ENOENT || errno == EACCES)
    {
        url_errno = errno;
        return NULL;
    }

#ifdef DEBUG
    if (mapptr != NULL)
        fprintf(stderr, "mmap - success. size=%d\n", mapsize);
#ifdef HAVE_MMAP
    else
        fprintf(stderr, "mmap - failure.\n");
#endif
#endif /* DEBUG */

    if (mapptr == NULL)
    {
        fp = fopen(fname, "rb");
        if (fp == NULL)
        {
            url_errno = errno;
            return NULL;
        }
    }

done:
    url = (URL_file *)alloc_url(sizeof(URL_file));
    if (url == NULL)
    {
        url_errno = errno;
        if (mapptr)
            munmap(mapptr, mapsize);
        if (fp && fp != stdin)
            fclose(fp);
        errno = url_errno;
        return NULL;
    }

    /* common members */
    URLm(url, type) = URL_file_t;
    URLm(url, url_read) = url_file_read;
    URLm(url, url_gets) = url_file_gets;
    URLm(url, url_fgetc) = url_file_fgetc;
    URLm(url, url_close) = url_file_close;
    if (fp == stdin)
    {
        URLm(url, url_seek) = NULL;
        URLm(url, url_tell) = NULL;
    }
    else
    {
        URLm(url, url_seek) = url_file_seek;
        URLm(url, url_tell) = url_file_tell;
    }

    /* private members */
    url->mapptr = mapptr;
    url->mapsize = mapsize;
    url->pos = 0;
    url->fp = fp;
#ifdef _WIN32
    url->hFile = hFile;
    url->hMap = hMap;
#endif /* _WIN32 */

    return (URL)url;
}

static long url_file_read(URL url, void *buff, long n)
{
    URL_file *urlp = (URL_file *)url;

    if (urlp->mapptr != NULL)
    {
        if (urlp->pos + n > urlp->mapsize)
            n = urlp->mapsize - urlp->pos;
        memcpy(buff, urlp->mapptr + urlp->pos, n);
        urlp->pos += n;
    }
    else
    {
        if ((n = (long)fread(buff, 1, n, urlp->fp)) == 0)
        {
            if (ferror(urlp->fp))
            {
                url_errno = errno;
                return -1;
            }
            return 0;
        }
    }
    return n;
}

char *url_file_gets(URL url, char *buff, int n)
{
    URL_file *urlp = (URL_file *)url;

    if (urlp->mapptr != NULL)
    {
        long s;
        char *nlp, *p;

        if (urlp->mapsize == urlp->pos)
            return NULL;
        if (n <= 0)
            return buff;
        if (n == 1)
        {
            *buff = '\0';
            return buff;
        }
        n--; /* for '\0' */
        s = urlp->mapsize - urlp->pos;
        if (s > n)
            s = n;
        p = urlp->mapptr + urlp->pos;
        nlp = (char *)memchr(p, url_newline_code, s);
        if (nlp != NULL)
            s = nlp - p + 1;
        memcpy(buff, p, s);
        buff[s] = '\0';
        urlp->pos += s;
        return buff;
    }

    return fgets(buff, n, urlp->fp);
}

int url_file_fgetc(URL url)
{
    URL_file *urlp = (URL_file *)url;

    if (urlp->mapptr != NULL)
    {
        if (urlp->mapsize == urlp->pos)
            return EOF;
        return urlp->mapptr[urlp->pos++] & 0xff;
    }

#ifdef getc
    return getc(urlp->fp);
#else
    return fgetc(urlp->fp);
#endif /* getc */
}

static void url_file_close(URL url)
{
    URL_file *urlp = (URL_file *)url;
    if (urlp->mapptr != NULL)
    {
#ifdef _WIN32
	    HANDLE hFile = urlp->hFile;
	    HANDLE hMap = urlp->hMap;
#endif /* __W32__ */
        munmap(urlp->mapptr, urlp->mapsize);
    }
    if (urlp->fp != NULL)
    {
        if (urlp->fp == stdin)
            rewind(stdin);
        else
            fclose(urlp->fp);
    }
    free(url);
}

static long url_file_seek(URL url, long offset, int whence)
{
    URL_file *urlp = (URL_file *)url;
    long ret;

    if (urlp->mapptr == NULL)
        return fseek(urlp->fp, offset, whence);
    ret = urlp->pos;
    switch (whence)
    {
    case SEEK_SET:
        urlp->pos = offset;
        break;
    case SEEK_CUR:
        urlp->pos += offset;
        break;
    case SEEK_END:
        urlp->pos = urlp->mapsize + offset;
        break;
    }
    if (urlp->pos > urlp->mapsize)
        urlp->pos = urlp->mapsize;
    else if (urlp->pos < 0)
        urlp->pos = 0;

    return ret;
}

static long url_file_tell(URL url)
{
    URL_file *urlp = (URL_file *)url;

    return urlp->mapptr ? urlp->pos : ftell(urlp->fp);
}
