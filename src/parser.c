/**
 * @file printer.c
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief Generic libyang printers functions.
 *
 * Copyright (c) 2015 - 2019 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "parser.h"
#include "parser_internal.h"

API LY_IN_TYPE
ly_in_type(const struct ly_in *in)
{
    LY_CHECK_ARG_RET(NULL, in, LY_IN_ERROR);
    return in->type;
}

API struct ly_in *
ly_in_new_fd(int fd)
{
    struct ly_in *in;
    size_t length;
    char *addr;

    LY_CHECK_ARG_RET(NULL, fd >= 0, NULL);

    LY_CHECK_RET(ly_mmap(NULL, fd, &length, (void **)&addr), NULL);
    if (!addr) {
        LOGERR(NULL, LY_EINVAL, "Empty input file.");
        return NULL;
    }

    in = calloc(1, sizeof *in);
    LY_CHECK_ERR_RET(!in, LOGMEM(NULL); ly_munmap(addr, length), NULL);

    in->type = LY_IN_FD;
    in->method.fd = fd;
    in->current = in->start = addr;
    in->length = length;

    return in;
}

API int
ly_in_fd(struct ly_in *in, int fd)
{
    int prev_fd;
    size_t length;
    const char *addr;

    LY_CHECK_ARG_RET(NULL, in, in->type == LY_IN_FD, -1);

    prev_fd = in->method.fd;

    if (fd != -1) {
        LY_CHECK_RET(ly_mmap(NULL, fd, &length, (void **)&addr), -1);
        if (!addr) {
            LOGERR(NULL, LY_EINVAL, "Empty input file.");
            return -1;
        }

        ly_munmap((char*)in->start, in->length);

        in->method.fd = fd;
        in->current = in->start = addr;
        in->length = length;
    }

    return prev_fd;
}

API struct ly_in *
ly_in_new_file(FILE *f)
{
    struct ly_in *in;

    LY_CHECK_ARG_RET(NULL, f, NULL);

    in = ly_in_new_fd(fileno(f));
    LY_CHECK_RET(!in, NULL);

    /* convert the LY_IN_FD input handler into the LY_IN_FILE */
    in->type = LY_IN_FILE;
    in->method.f = f;

    return in;
}

API FILE *
ly_in_file(struct ly_in *in, FILE *f)
{
    FILE *prev_f;

    LY_CHECK_ARG_RET(NULL, in, in->type == LY_IN_FILE, NULL);

    prev_f = in->method.f;

    if (f) {
        /* convert LY_IN_FILE handler into LY_IN_FD to be able to update it via ly_in_fd() */
        in->type = LY_IN_FD;
        in->method.fd = fileno(prev_f);
        if (ly_in_fd(in, fileno(f)) == -1) {
            in->type = LY_IN_FILE;
            in->method.f = prev_f;
            return NULL;
        }

        /* if success, convert the result back */
        in->type = LY_IN_FILE;
        in->method.f = f;
    }

    return prev_f;
}

API struct ly_in *
ly_in_new_memory(const char *str)
{
    struct ly_in *in;

    LY_CHECK_ARG_RET(NULL, str, NULL);

    in = calloc(1, sizeof *in);
    LY_CHECK_ERR_RET(!in, LOGMEM(NULL), NULL);

    in->type = LY_IN_MEMORY;
    in->start = in->current = str;

    return in;
}

const char *
ly_in_memory(struct ly_in *in, const char *str)
{
    const char *data;

    LY_CHECK_ARG_RET(NULL, in, in->type == LY_IN_MEMORY, NULL);

    data = in->current;

    if (str) {
        in->start = in->current = str;
    }

    return data;
}

API LY_ERR
ly_in_reset(struct ly_in *in)
{
    LY_CHECK_ARG_RET(NULL, in, LY_EINVAL);

    in->current = in->start;
    return LY_SUCCESS;
}

API struct ly_in *
ly_in_new_filepath(const char *filepath, size_t len)
{
    struct ly_in *in;
    char *fp;
    int fd;

    LY_CHECK_ARG_RET(NULL, filepath, NULL);

    if (len) {
        fp = strndup(filepath, len);
    } else {
        fp = strdup(filepath);
    }

    fd = open(fp, O_RDONLY);
    LY_CHECK_ERR_RET(!fd, LOGERR(NULL, LY_ESYS, "Failed to open file \"%s\" (%s).", fp, strerror(errno)); free(fp), NULL);

    in = ly_in_new_fd(fd);
    LY_CHECK_ERR_RET(!in, free(fp), NULL);

    /* convert the LY_IN_FD input handler into the LY_IN_FILE */
    in->type = LY_IN_FILEPATH;
    in->method.fpath.fd = fd;
    in->method.fpath.filepath = fp;

    return in;
}

API const char *
ly_in_filepath(struct ly_in *in, const char *filepath, size_t len)
{
    int fd, prev_fd;
    char *fp = NULL;

    LY_CHECK_ARG_RET(NULL, in, in->type == LY_IN_FILEPATH, filepath ? NULL : ((void *)-1));

    if (!filepath) {
        return in->method.fpath.filepath;
    }

    if (len) {
        fp = strndup(filepath, len);
    } else {
        fp = strdup(filepath);
    }

    /* replace filepath */
    fd = open(fp, O_RDONLY);
    LY_CHECK_ERR_RET(!fd, LOGERR(NULL, LY_ESYS, "Failed to open file \"%s\" (%s).", fp, strerror(errno)); free(fp), NULL);

    /* convert LY_IN_FILEPATH handler into LY_IN_FD to be able to update it via ly_in_fd() */
    in->type = LY_IN_FD;
    prev_fd = ly_in_fd(in, fd);
    LY_CHECK_ERR_RET(prev_fd == -1, in->type = LY_IN_FILEPATH; free(fp), NULL);

    /* and convert the result back */
    in->type = LY_IN_FILEPATH;
    close(prev_fd);
    free(in->method.fpath.filepath);
    in->method.fpath.fd = fd;
    in->method.fpath.filepath = fp;

    return NULL;
}

void
lys_parser_fill_filepath(struct ly_ctx *ctx, struct ly_in *in, const char **filepath)
{
    char path[PATH_MAX];
    char proc_path[32];
    int len;

    LY_CHECK_ARG_RET(NULL, ctx, in, filepath, );
    if (*filepath) {
        /* filepath already set */
        return;
    }

    switch (in->type) {
    case LY_IN_FILEPATH:
        if (realpath(in->method.fpath.filepath, path) != NULL) {
            *filepath = lydict_insert(ctx, path, 0);
        } else {
            *filepath = lydict_insert(ctx, in->method.fpath.filepath, 0);
        }

        break;
    case LY_IN_FD:
#ifdef __APPLE__
        if (fcntl(fd, F_GETPATH, path) != -1) {
            mod->filepath = lydict_insert(ctx, path, 0);
        }
#else
        /* get URI if there is /proc */
        sprintf(proc_path, "/proc/self/fd/%d", in->method.fd);
        if ((len = readlink(proc_path, path, PATH_MAX - 1)) > 0) {
            *filepath = lydict_insert(ctx, path, len);
        }
#endif
        break;
    case LY_IN_MEMORY:
    case LY_IN_FILE:
        /* nothing to do */
        break;
    default:
        LOGINT(ctx);
        break;
    }

}

API void
ly_in_free(struct ly_in *in, int destroy)
{
    if (!in) {
        return;
    } else if (in->type == LY_IN_ERROR) {
        LOGINT(NULL);
        return;
    }

    if (destroy) {
        if (in->type == LY_IN_MEMORY) {
            free((char*)in->start);
        } else {
            ly_munmap((char*)in->start, in->length);

            if (in->type == LY_IN_FILE) {
                fclose(in->method.f);
            } else {
                close(in->method.fd);

                if (in->type == LY_IN_FILEPATH) {
                    free(in->method.fpath.filepath);
                }
            }
        }
    } else if (in->type != LY_IN_MEMORY) {
        ly_munmap((char*)in->start, in->length);

        if (in->type == LY_IN_FILEPATH) {
            close(in->method.fpath.fd);
            free(in->method.fpath.filepath);
        }
    }

    free(in);
}

API ssize_t
ly_read(struct ly_in *in, void *buf, ssize_t count)
{
    ssize_t i;
    int direction; /* 1 - moving forward; -1 - moving back */

    LY_CHECK_ARG_RET(NULL, in, in->type != LY_IN_ERROR, LY_EINVAL * -1);

    if (count > 0) {
        direction = 1;
        if (in->current[0] == '\0') {
            return 0;
        }
    } else if (count < 0) {
        direction = -1;
        count = count * -1;
    } else {
        /* nothing to do */
        return 0;
    }

    for (i = 0; i < count; i++) {
        const char *c = &in->current[i * direction];
        if ((direction > 0 && *c == '\0') || (direction < 0 && c == in->start)) {
            break;
        }
    }

    if (buf) {
        if (direction > 0) {
            memcpy(buf, in->current, i);
            in->current = in->current + i;
        } else {
            in->current = in->current - i;
            memcpy(buf, in->current, i);
        }
    } else {
        in->current = in->current + (i * direction);
    }

    return i;
}
