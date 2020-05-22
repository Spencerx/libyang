/*
 * @file test_inout.c
 * @author: Radek Krejci <rkrejci@cesnet.cz>
 * @brief unit tests for input and output handlers functions
 *
 * Copyright (c) 2018 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#include "tests/config.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../src/common.h"
#include "../../src/log.h"
#include "../../src/printer.h"
#include "../../src/parser.h"


#define BUFSIZE 1024
char logbuf[BUFSIZE] = {0};
int store = -1; /* negative for infinite logging, positive for limited logging */

/* set to 0 to printing error messages to stderr instead of checking them in code */
#define ENABLE_LOGGER_CHECKING 1

#if ENABLE_LOGGER_CHECKING
static void
logger(LY_LOG_LEVEL level, const char *msg, const char *path)
{
    (void) level; /* unused */
    if (store) {
        if (path && path[0]) {
            snprintf(logbuf, BUFSIZE - 1, "%s %s", msg, path);
        } else {
            strncpy(logbuf, msg, BUFSIZE - 1);
        }
        if (store > 0) {
            --store;
        }
    }
}
#endif

static int
logger_setup(void **state)
{
    (void) state; /* unused */

    ly_set_log_clb(logger, 0);

    return 0;
}

static int
logger_teardown(void **state)
{
    (void) state; /* unused */
#if ENABLE_LOGGER_CHECKING
    if (*state) {
        fprintf(stderr, "%s\n", logbuf);
    }
#endif
    return 0;
}

void
logbuf_clean(void)
{
    logbuf[0] = '\0';
}

#if ENABLE_LOGGER_CHECKING
#   define logbuf_assert(str) assert_string_equal(logbuf, str)
#else
#   define logbuf_assert(str)
#endif

static void
test_input_mem(void **state)
{
    struct ly_in *in = NULL;
    char *str1 = "a", *str2 = "b";

    *state = test_input_mem;

    assert_null(ly_in_new_memory(NULL));
    assert_null(ly_in_memory(NULL, NULL));

    assert_non_null(in = ly_in_new_memory(str1));
    assert_int_equal(LY_IN_MEMORY, ly_in_type(in));
    assert_ptr_equal(str1, ly_in_memory(in, str2));
    assert_ptr_equal(str2, ly_in_memory(in, NULL));
    assert_ptr_equal(str2, ly_in_memory(in, NULL));
    ly_in_free(in, 0);

    /* cleanup */
    *state = NULL;
}

static void
test_input_fd(void **state)
{
    struct ly_in *in = NULL;
    int fd1, fd2;
    struct stat statbuf;

    *state = test_input_fd;

    assert_null(ly_in_new_fd(-1));
    assert_int_equal(-1, ly_in_fd(NULL, -1));

    assert_int_not_equal(-1, fd1 = open(__FILE__, O_RDONLY));
    assert_int_not_equal(-1, fd2 = open(__FILE__, O_RDONLY));

    assert_non_null(in = ly_in_new_fd(fd1));
    assert_int_equal(LY_IN_FD, ly_in_type(in));
    assert_ptr_equal(fd1, ly_in_fd(in, fd2));
    assert_ptr_equal(fd2, ly_in_fd(in, -1));
    assert_ptr_equal(fd2, ly_in_fd(in, -1));
    ly_in_free(in, 1);
    /* fd1 is still open */
    assert_int_equal(0, fstat(fd1, &statbuf));
    close(fd1);
    /* but fd2 was closed by ly_in_free() */
    errno = 0;
    assert_int_equal(-1, fstat(fd2, &statbuf));
    assert_int_equal(errno, EBADF);

    /* cleanup */
    *state = NULL;
}

static void
test_input_file(void **state)
{
    struct ly_in *in = NULL;
    FILE *f1 = NULL, *f2 = NULL;

    *state = test_input_file;

    assert_null(ly_in_new_file(NULL));
    assert_null(ly_in_file(NULL, NULL));

    assert_int_not_equal(-1, f1 = fopen(__FILE__, "r"));
    assert_int_not_equal(-1, f2 = fopen(__FILE__, "r"));

    assert_non_null(in = ly_in_new_file(f1));
    assert_int_equal(LY_IN_FILE, ly_in_type(in));
    assert_ptr_equal(f1, ly_in_file(in, f2));
    assert_ptr_equal(f2, ly_in_file(in, NULL));
    assert_ptr_equal(f2, ly_in_file(in, NULL));
    ly_in_free(in, 1);
    /* f1 is still open */
    assert_int_not_equal(-1, fileno(f1));
    fclose(f1);
    /* but f2 was closed by ly_in_free() */

    /* cleanup */
    *state = NULL;
}

static void
test_input_filepath(void **state)
{
    struct ly_in *in = NULL;
    const char *path1 = __FILE__, *path2 = __FILE__;

    *state = test_input_filepath;

    assert_ptr_equal(NULL, ly_in_new_filepath(NULL));
    assert_ptr_equal(((void *)-1), ly_in_filepath(NULL, NULL));

    assert_non_null(in = ly_in_new_filepath(path1));
    assert_int_equal(LY_IN_FILEPATH, ly_in_type(in));
    assert_ptr_equal(NULL, ly_in_filepath(in, path2));
    assert_string_equal(path2, ly_in_filepath(in, NULL));
    ly_in_free(in, 0);

    /* cleanup */
    *state = NULL;
}

static void
test_input_read(void **state)
{
    struct ly_in *in = NULL;
    char *data, buf[21] = {0};

    *state = test_input_read;

    data = "testline1\ntestline2\n";
    assert_non_null(in = ly_in_new_memory(data));

    assert_int_equal(-1 * LY_EINVAL, ly_read(NULL, NULL, 0));
    assert_int_equal(0, ly_read(in, NULL, 0));

    assert_int_equal(10, ly_read(in, buf, 10));
    assert_string_equal(buf, "testline1\n");
    assert_int_equal(10, ly_read(in, buf, 10));
    assert_string_equal(buf, "testline2\n");
    assert_int_equal(LY_SUCCESS, ly_in_reset(in));
    assert_int_equal(20, ly_read(in, buf, 20));
    assert_string_equal(buf, "testline1\ntestline2\n");
    buf[0] = buf[10] = '\0';
    assert_int_equal(0, ly_read(in, buf, 10));
    assert_string_equal(buf, "");
    assert_int_equal(10, ly_read(in, buf, -10));
    assert_string_equal(buf, "testline2\n");
    assert_int_equal(10, ly_read(in, buf, -10));
    assert_string_equal(buf, "testline1\n");
    buf[0] = '\0';
    assert_int_equal(0, ly_read(in, buf, -10));
    assert_string_equal(buf, "");
    assert_int_equal(10, ly_read(in, NULL, 10));
    assert_int_equal(10, ly_read(in, buf, 15));
    assert_string_equal(buf, "testline2\n");

    /* cleanup */
    ly_in_free(in, 0);
    *state = NULL;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_input_mem, logger_setup, logger_teardown),
        cmocka_unit_test_setup_teardown(test_input_fd, logger_setup, logger_teardown),
        cmocka_unit_test_setup_teardown(test_input_file, logger_setup, logger_teardown),
        cmocka_unit_test_setup_teardown(test_input_filepath, logger_setup, logger_teardown),
        cmocka_unit_test_setup_teardown(test_input_read, logger_setup, logger_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
