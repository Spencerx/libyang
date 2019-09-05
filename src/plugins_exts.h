/**
 * @file plugins_exts.h
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief libyang support for YANG extensions implementation.
 *
 * Copyright (c) 2015 - 2019 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef LY_PLUGINS_EXTS_H_
#define LY_PLUGINS_EXTS_H_

#include "set.h"
#include "tree_schema.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup extensions YANG Extensions
 *
 * @{
 */

/**
 * @brief Extensions API version
 */
#define LYEXT_API_VERSION 1

/**
 * @brief Macro to store version of extension plugins API in the plugins.
 * It is matched when the plugin is being loaded by libyang.
 */
#define LYEXT_VERSION_CHECK int lyext_api_version = LYEXT_API_VERSION;

/**
 * @defgroup extensionscompile YANG Extensions - Compilation Helpers
 * @ingroup extensions
 * @brief Helper functions to compile (via lyext_clb_compile callback) statements inside the extension instance.
 *
 * NOTE: There is a lot of useful static functions in the tree_schema_compile.c which could be provided here. Since we don't want
 * to have a large API with functions which will be never used, we provide here just the functions which are evidently needed.
 * If you, as an extension plugin author, need to make some of the compile functions available, please contact libyang maintainers
 * via the GITHUB issue tracker.
 *
 * @{
 */

/**
 * @brief internal context for compilation
 */
struct lysc_ctx {
    struct ly_ctx *ctx;
    struct lys_module *mod;
    struct lys_module *mod_def; /**< context module for the definitions of the nodes being currently
                                     processed - groupings are supposed to be evaluated in place where
                                     defined, but its content instances are supposed to be placed into
                                     the target module (mod) */
    struct ly_set groupings;    /**< stack for groupings circular check */
    struct ly_set unres;        /**< to validate leafref's target and xpath of when/must */
    struct ly_set dflts;        /**< set of incomplete default values */
    struct ly_set tpdf_chain;
    uint16_t path_len;
    int options;                /**< various @ref scflags. */
#define LYSC_CTX_BUFSIZE 4078
    char path[LYSC_CTX_BUFSIZE];
};

/**
 * @brief Possible cardinalities of the YANG statements.
 *
 * Used in extensions plugins to define cardinalities of the extension instance substatements.
 */
enum ly_stmt_cardinality {
    LY_STMT_CARD_OPT,    /* 0..1 */
    LY_STMT_CARD_MAND,   /* 1 */
    LY_STMT_CARD_SOME,   /* 1..n */
    LY_STMT_CARD_ANY     /* 0..n */
};

/**
 * @brief Description of the extension instance substatements.
 *
 * Provided by extensions plugins to libyang's lyext_compile_stmts() to be able to correctly compile the content of extension instances.
 * Note that order of the defined records matters - just follow the values of enum ly_stmt and order the records from lower to higher values.
 */
struct lysc_ext_substmt {
    enum ly_stmt stmt;                     /**< allowed substatement */
    enum ly_stmt_cardinality cardinality;  /**< cardinality of the substatement */
    void *storage;                         /**< pointer to the storage of the compiled statement according to the specific
                                                lysc_ext_substmt::stmt and lysc_ext_substmt::cardinality */
};

/**
 * @brief Compile substatements of an extension instance.
 */
LY_ERR lys_compile_extension_instance(struct lysc_ctx *ctx, const struct lysp_ext_instance *ext, struct lysc_ext_substmt *substmts);

/**
 * @brief Update path in the compile context, which is used for logging where the compilation failed.
 *
 * @param[in] ctx Compile context with the path.
 * @param[in] parent Parent of the current node to check difference of the node's module. The current module is taken from lysc_ctx::mod.
 * @param[in] name Name of the node to update path with. If NULL, the last segment is removed. If the format is `{keyword}`, the following
 * call updates the segment to the form `{keyword='name'}` (to remove this compound segment, 2 calls with NULL @p name must be used).
 */
void lysc_update_path(struct lysc_ctx *ctx, struct lysc_node *parent, const char *name);

/** @} extensionscompile */

/**
 * @brief Callback to compile extension from the lysp_ext_instance to the lysc_ext_instance. The later structure is generally prepared
 * and only the extension specific data are supposed to be added (if any).
 *
 * @param[in] cctx Current compile context.
 * @param[in] p_ext Parsed extension instance data.
 * @param[in,out] c_ext Prepared compiled extension instance structure where an addition, extension-specific, data are supposed to be placed
 * for later use (data validation or use of external tool).
 * @return LY_SUCCESS in case of success.
 * @return LY_EVALID in case of non-conforming parsed data.
 */
typedef LY_ERR (*lyext_clb_compile)(struct lysc_ctx *cctx, const struct lysp_ext_instance *p_ext, struct lysc_ext_instance *c_ext);

/**
 * @brief Callback to free the extension specific data created by the lyext_clb_compile callback of the same extension plugin.
 *
 * @param[in,out] ext Compiled extension structure where the data to free are placed.
 */
typedef void (*lyext_clb_free)(struct lysc_ext_instance *ext);

/**
 * @brief Callback to decide if data instance is valid according to the schema.
 *
 * The callback is used only for the extension instances placed in the following parent statements
 * (which is specified as lysc_ext_instance::parent_type):
 *     - LYEXT_PAR_NODE - @p node is instance of the schema node where the extension instance was specified.
 *     - LYEXT_PAR_TPDF - @p node is instance of the schema node with the value of the typedef's type where the extension instance was specified.
 *     - LYEXT_PAR_TYPE - @p node is instance of the schema node with the value of the type where the extension instance was specified.
 *     - LYEXT_PAR_TYPE_BIT - @p node is instance of the schema node with the value of the bit where the extension instance was specified.
 *     - LYEXT_PAR_TYPE_ENUM - @p node is instance of the schema node with the value of the enum where the extension instance was specified.
 *
 * @param[in] ext Extension instance to be checked.
 * @param[in] node Data node, where the extension data are supposed to be placed.
 *
 * @return LY_SUCCESS on data validation success.
 * @return LY_EVALID in case the validation fails.
 */
typedef LY_ERR (*lyext_clb_data_validation)(struct lysc_ext_instance *ext, struct lyd_node *node);

/**
 * @brief Extension plugin implementing various aspects of a YANG extension
 */
struct lyext_plugin {
    const char *id;                     /**< Plugin identification (mainly for distinguish incompatible versions of the plugins for external tools) */
    lyext_clb_compile compile;          /**< Callback to compile extension instance from the parsed data */
    lyext_clb_data_validation validate; /**< Callback to decide if data instance is valid according to the schema. */
    /* TODO printers? (schema/data) */
    lyext_clb_free free;                /**< Free the extension instance specific data created by lyext_plugin::compile callback */
};

struct lyext_plugins_list {
    const char *module;          /**< name of the module where the extension is defined */
    const char *revision;        /**< optional module revision - if not specified, the plugin applies to any revision,
                                      which is not an optimal approach due to a possible future revisions of the module.
                                      Instead, there should be defined multiple items in the plugins list, each with the
                                      different revision, but all with the same pointer to the plugin extension. The
                                      only valid use case for the NULL revision is the case the module has no revision. */
    const char *name;            /**< name of the extension */
    struct lyext_plugin *plugin; /**< plugin for the extension */
};


/**
 * @brief Provide a log message from an extension plugin.
 *
 * @param[in] ext Compiled extension structure providing generic information about the extension/plugin causing the message.
 * @param[in] level Log message level (error, warning, etc.)
 * @param[in] err_no Error type code.
 * @param[in] path Path relevant to the message.
 * @param[in] format Format string to print.
 */
void lyext_log(const struct lysc_ext_instance *ext, LY_LOG_LEVEL level, LY_ERR err_no, const char *path, const char *format, ...);

/** @} extensions */

#ifdef __cplusplus
}
#endif

#endif /* LY_PLUGINS_EXTS_H_ */