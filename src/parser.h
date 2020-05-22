/**
 * @file parser.h
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief Generic libyang parsers structures and functions
 *
 * Copyright (c) 2020 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef LY_PARSER_H_
#define LY_PARSER_H_

#include <unistd.h>

/**
 * @brief Parser input structure specifying where the data are read.
 */
struct ly_in;

/**
 * @brief Types of the parser's inputs
 */
typedef enum LY_IN_TYPE {
    LY_IN_ERROR = -1,  /**< error value to indicate failure of the functions returning LY_IN_TYPE */
    LY_IN_FD,          /**< file descriptor printer */
    LY_IN_FILE,        /**< FILE stream parser */
    LY_IN_FILEPATH,    /**< filepath parser */
    LY_IN_MEMORY       /**< memory parser */
} LY_IN_TYPE;

/**
 * @brief Get input type of the input handler.
 *
 * @param[in] in Input handler.
 * @return Type of the parser's input.
 */
LY_IN_TYPE ly_in_type(const struct ly_in *in);

/**
 * @brief Reset the input medium to read from its beginning, so the following parser function will read from the object's beginning.
 *
 * Note that in case the underlying output is not seekable (stream referring a pipe/FIFO/socket or the callback output type),
 * nothing actually happens despite the function succeeds. Also note that the medium is not returned to the state it was when
 * the handler was created. For example, file is seeked into the offset zero, not to the offset where it was opened when
 * ly_in_new_file() was called.
 *
 * @param[in] in Input handler.
 * @return LY_SUCCESS in case of success
 * @return LY_ESYS in case of failure
 */
LY_ERR ly_in_reset(struct ly_in *in);

/**
 * @brief Create input handler using file descriptor.
 *
 * @param[in] fd File descriptor to use.
 * @return NULL in case of error.
 * @return Created input handler supposed to be passed to different ly*_parse*() functions.
 */
struct ly_in *ly_in_new_fd(int fd);

/**
 * @brief Get or reset file descriptor input handler.
 *
 * @param[in] in Input handler.
 * @param[in] fd Optional value of a new file descriptor for the handler. If -1, only the current file descriptor value is returned.
 * @return Previous value of the file descriptor. Note that caller is responsible for closing the returned file descriptor in case of setting new descriptor @p fd.
 * @return -1 in case of error when setting up the new file descriptor.
 */
int ly_in_fd(struct ly_in *in, int fd);

/**
 * @brief Create input handler using file stream.
 *
 * @param[in] f File stream to use.
 * @return NULL in case of error.
 * @return Created input handler supposed to be passed to different ly*_parse*() functions.
 */
struct ly_in *ly_in_new_file(FILE *f);

/**
 * @brief Get or reset file stream input handler.
 *
 * @param[in] in Input handler.
 * @param[in] f Optional new file stream for the handler. If NULL, only the current file stream is returned.
 * @return NULL in case of invalid argument or an error when setting up the new input file, original input handler @p in is untouched in this case.
 * @return Previous file stream of the handler. Note that caller is responsible for closing the returned stream in case of setting new stream @p f.
 */
FILE *ly_in_file(struct ly_in *in, FILE *f);

/**
 * @brief Create input handler using memory to read data.
 *
 * @param[in] str Pointer where to start reading data. The input data are expected to be NULL-terminated
 * @return NULL in case of error.
 * @return Created input handler supposed to be passed to different ly*_parse*() functions.
 */
struct ly_in *ly_in_new_memory(char *str);

/**
 * @brief Get or change memory where the data are read from.
 *
 * @param[in] in Input handler.
 * @param[in] str String containing the data to read.
 * @return Previous starting address to read data from. Note that the caller is responsible to free
 * the data in case of changing string pointer @p str.
 */
char *ly_in_memory(struct ly_in *in, char *str);

/**
 * @brief Create input handler file of the given filename.
 *
 * @param[in] filepath Path of the file where to read data.
 * @return NULL in case of error.
 * @return Created input handler supposed to be passed to different ly*_parse*() functions.
 */
struct ly_in *ly_in_new_filepath(const char *filepath);

/**
 * @brief Get or change the filepath of the file where the parser reads the data.
 *
 * Note that in case of changing the filepath, the current file is closed and a new one is
 * created/opened instead of renaming the previous file. Also note that the previous filepath
 * string is returned only in case of not changing it's value.
 *
 * @param[in] in Input handler.
 * @param[in] filepath Optional new filepath for the handler. If and only if NULL, the current filepath string is returned.
 * @return Previous filepath string in case the @p filepath argument is NULL.
 * @return NULL if changing filepath succeedes and ((void *)-1) otherwise.
 */
const char *ly_in_filepath(struct ly_in *in, const char *filepath);

/**
 * @brief Generic reader getting up to @p count bytes from given input @p in into the buffer starting at @p buf.
 *
 * @param[in] in Input handler.
 * @param[in] buf Memory buffer for storing read data. Can be NULL to just move in the input by the @p count bytes
 * @param[in] count Maximal number of bytes to write into @p buf (input is expected to be NULL-terminated).
 * If the @p buf is NULL, function seeks in the input object. It is possible to set even a negative value to move back in the input object.
 * In such a case the specified number of bytes is still written into the @p buf (the order of bytes is still from the lower addresses,
 * it does not reverse the original order of the input data) and it is not possible to go before the original beginning of the input.
 * @return The number of bytes read from input (written into @p buf).
 * @return Negative value in case of error, absolute value of the return code can be mapped to LY_ERR value.
 */
ssize_t ly_read(struct ly_in *in, void *buf, ssize_t count);

/**
 * @brief Free the input handler.
 * @param[in] in Input handler to free.
 * @param[in] destroy Flag to free the input data buffer (for LY_IN_MEMORY) or to
 * close stream/file descriptor (for LY_IN_FD and LY_IN_FILE)
 */
void ly_in_free(struct ly_in *in, int destroy);

#endif /* LY_PARSER_H_ */
