/*
**  mod_gsgi.c -- Apache sample gsgi module
**  [Autogenerated via ``apxs -n gsgi -g'']
**
**  To play with this sample module first compile it into a
**  DSO file and install it into Apache's modules directory
**  by running:
**
**    $ apxs -c -i mod_gsgi.c
**
**  Then activate it in Apache's httpd.conf file for instance
**  for the URL /gsgi in as follows:
**
**    #   httpd.conf
**    LoadModule gsgi_module modules/mod_gsgi.so
**    <Location /gsgi>
**    SetHandler gsgi
**    </Location>
**
**  Then after restarting Apache via
**
**    $ apachectl restart
**
**  you immediately can request the URL /gsgi and watch for the
**  output of this module. This can be achieved for instance via:
**
**    $ lynx -mime_header http://localhost/gsgi
**
**  The output should be similar to the following one:
**
**    HTTP/1.1 200 OK
**    Date: Tue, 31 Mar 1998 14:42:22 GMT
**    Server: Apache/1.3.4 (Unix)
**    Connection: close
**    Content-Type: text/html
**
**    The sample page from mod_gsgi.c
*/

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include "apr_thread_proc.h"
#include "apr_thread_cond.h"
#include "ap_mpm.h"
#include <gauche.h>
#include <strings.h>

module AP_MODULE_DECLARE_DATA gsgi_module;

typedef struct {
    char* script_file_path;
    char* add_load_path;
} gsgi_directory_config;

static const char* GSGI_HANDLER_NAME = "gsgi";
static const char* GSGI_ENTRY_POINT_NAME = "application";

/**
 * Get application procedure from script.
 */
static ScmObj gsgi_get_application(request_rec* r, const char* path) {
    ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "path: %s", path);
    ScmLoadPacket lpak;

    if (Scm_Load(path, 0, &lpak) < 0) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "load fail: %s %s", path, Scm_GetString(SCM_STRING(SCM_ERROR_MESSAGE(lpak.exception))));
        return SCM_NIL;
    }

    ScmObj application = Scm_SymbolValue(Scm_UserModule(), SCM_SYMBOL(SCM_INTERN(GSGI_ENTRY_POINT_NAME)));

    if (!SCM_PROCEDUREP(application)) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "application is not procedure");
        return SCM_NIL;
    }

    ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "load success: %s", path);

    return application;
}

/**
 * Build environment variables.
 */
static ScmObj build_env(request_rec *r) {
    int i = 0, mpm_query_info;
    ScmObj env = SCM_NIL, is_multithread = SCM_FALSE, is_multiprocess = SCM_FALSE, url_scheme;

    ap_add_cgi_vars(r);
    ap_add_common_vars(r);

    if (ap_mpm_query(AP_MPMQ_IS_THREADED, &mpm_query_info) == APR_SUCCESS) {
        is_multithread = (mpm_query_info == AP_MPMQ_STATIC || mpm_query_info == AP_MPMQ_DYNAMIC) ? SCM_TRUE : SCM_FALSE;
    }

    if (ap_mpm_query(AP_MPMQ_IS_FORKED, &mpm_query_info) == APR_SUCCESS) {
        is_multiprocess = (mpm_query_info == AP_MPMQ_STATIC || mpm_query_info == AP_MPMQ_DYNAMIC) ? SCM_TRUE : SCM_FALSE;
    }

    url_scheme = apr_table_get(r->subprocess_env, "HTTPS") == NULL ? SCM_MAKE_STR("http") : SCM_MAKE_STR("https");

    env = Scm_Acons(SCM_MAKE_STR("gsgi.version"), SCM_MAKE_STR("0.1"), env); // Is array better than string to represent GSGI specification version?
    env = Scm_Acons(SCM_MAKE_STR("gsgi.input"), SCM_NIL, env); // TODO
    env = Scm_Acons(SCM_MAKE_STR("gsgi.errors"), SCM_NIL, env); // TODO
    env = Scm_Acons(SCM_MAKE_STR("gsgi.run_once"), SCM_NIL, env); // TODO
    env = Scm_Acons(SCM_MAKE_STR("gsgi.nonblocking"), SCM_FALSE, env); // Does apache support non-blocking IO?
    env = Scm_Acons(SCM_MAKE_STR("gsgi.streaming"), SCM_FALSE, env); // Does apache support streaming IO?
    env = Scm_Acons(SCM_MAKE_STR("gsgi.multiprocess"), is_multiprocess, env);
    env = Scm_Acons(SCM_MAKE_STR("gsgi.multithread"), is_multithread, env);
    env = Scm_Acons(SCM_MAKE_STR("gsgi.url_scheme"), url_scheme, env);

    // modify PATH_INFO
    apr_table_set(r->subprocess_env, "PATH_INFO", r->uri);

    // copy r->subprocess_env
    const apr_array_header_t *head = apr_table_elts(r->subprocess_env);
    apr_table_entry_t *entries = (apr_table_entry_t*) head->elts;
    for (i = 0; i < head->nelts; ++i) {
        env = Scm_Acons(SCM_MAKE_STR(entries[i].key), SCM_MAKE_STR(entries[i].val), env);
    }

    // copy r->headers_in
    // http headers are case insensitive
    head = apr_table_elts(r->headers_in);
    entries = (apr_table_entry_t*) head->elts;
    for (i = 0; i < head->nelts; ++i) {
        if (strcasecmp("content-type", entries[i].key) == 0) {
            env = Scm_Acons(SCM_MAKE_STR(entries[i].key), SCM_MAKE_STR(entries[i].val), env);
        }
        else if (strcasecmp("content-length", entries[i].key) == 0) {
            env = Scm_Acons(SCM_MAKE_STR(entries[i].key), SCM_MAKE_STR(entries[i].val), env);
        }
        else if (strncasecmp("http_", entries[i].key, 5) == 0) {
            env = Scm_Acons(SCM_MAKE_STR(entries[i].key), SCM_MAKE_STR(entries[i].val), env);
        }
    }

    return env;
}

/**
 * Handle status code.
 */
static int gsgi_handle_status(request_rec* r, ScmObj status) {
    // status must be integer value
    if (!SCM_INTEGERP(status)) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    r->status = SCM_INT_VALUE(status);
    r->status_line = ap_get_status_line(r->status);

    return OK;
}

/**
 * Handle headers.
 */
static int gsgi_handle_headers(request_rec* r, ScmObj headers) {
    ScmObj rest;
    ScmObj header;

    // content-type
    ScmObj content_type_pair = Scm_Assoc(SCM_MAKE_STR("Content-Type"), headers, SCM_CMP_EQUAL);
    char* content_type = Scm_GetString(SCM_STRING(SCM_CDR(content_type_pair)));
    r->content_type = content_type;

    SCM_FOR_EACH(rest, headers) {
        header = Scm_Car(rest);
        char* key = Scm_GetString(SCM_STRING(Scm_Car(header)));
        char* value = Scm_GetString(SCM_STRING(Scm_Cdr(header)));
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[header] %s: %s", key, value);
        apr_table_set(r->headers_out, key, value);
    }

    return OK;
}

/**
 * Handle body.
 */
static int gsgi_handle_body(request_rec* r, ScmObj body) {
    ScmObj response = Scm_StringJoin(body, SCM_STRING(SCM_MAKE_STR("\n")), SCM_STRING_JOIN_SUFFIX);
    ap_rprintf(r, "%s", Scm_GetStringConst(SCM_STRING(response)));
    return OK;
}

/**
 * directory config.
 */
static gsgi_directory_config* gsgi_allocate_directory_config(apr_pool_t* p) {
    gsgi_directory_config* config = (gsgi_directory_config*) apr_pcalloc(p, sizeof(gsgi_directory_config));
    config->script_file_path = "";
    config->add_load_path = "";
    return config;
}

static void* gsgi_create_directory_config(apr_pool_t *pool, char *dir) {
    return gsgi_allocate_directory_config(pool);
}

static const char* gsgi_set_script_file_path(cmd_parms* cmd, void* mconfig, const char* arg) {
    gsgi_directory_config *config = (gsgi_directory_config*) mconfig;
    config->script_file_path = arg;
    return NULL;
}

static const char* gsgi_add_load_path(cmd_parms* cmd, void* mconfig, const char* args) {
    gsgi_directory_config *config = (gsgi_directory_config*) mconfig;
    config->add_load_path = args;
    return NULL;
}

static int gsgi_handler(request_rec *r)
{
    int rc = 0;
    int result = 0;
    gsgi_directory_config *dconfig;
    ScmEvalPacket epak;
    ScmObj application, env;

    if (strcmp(r->handler, GSGI_HANDLER_NAME)) {
        return DECLINED;
    }

    dconfig = ap_get_module_config(r->per_dir_config, &gsgi_module);

    Scm_Init(GAUCHE_SIGNATURE);

    env = build_env(r);

    ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "env: %s", Scm_GetString(SCM_STRING(Scm_Sprintf("%S", env))));

    Scm_AddLoadPath(dconfig->add_load_path, FALSE);

    application = gsgi_get_application(r, dconfig->script_file_path);

    if (SCM_NULLP(application)) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "GSGI application is NULL");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    result = Scm_Apply(application, SCM_LIST1(env), &epak);

    if (result <= 0) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Scm_Apply failed: %s", Scm_GetString(SCM_STRING(Scm_Sprintf("%S", SCM_ERROR_MESSAGE(epak.exception)))));
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (epak.numResults != 3) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "GSGI application must return (values http-status-code http-response-headers http-response-body).");

        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // ouput status
    rc = gsgi_handle_status(r, epak.results[0]);
    if (rc != OK) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "gsgi_handle_status() failed.");
        return rc;
    }

    // output response headers
    rc = gsgi_handle_headers(r, epak.results[1]);
    if (rc != OK) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "gsgi_handle_headers() failed.");
        return rc;
    }

    // output response body
    rc = gsgi_handle_body(r, epak.results[2]);
    if (rc != OK) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "gsgi_handle_body() failed.");
        return rc;
    }

    return OK;
}

static void gsgi_register_hooks(apr_pool_t *p) {
    ap_hook_handler(gsgi_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/**
 * command
 */
static const command_rec gsgi_commands[] = {
    AP_INIT_TAKE1("GSGIScriptFilePath", gsgi_set_script_file_path, NULL, ACCESS_CONF, "script file path"),
    AP_INIT_RAW_ARGS("GSGIAddLoadPath", gsgi_add_load_path, NULL, ACCESS_CONF, "add load path"),
    {NULL}
};

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA gsgi_module = {
    STANDARD20_MODULE_STUFF,
    gsgi_create_directory_config,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    NULL,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    gsgi_commands,         /* table of config file commands       */
    gsgi_register_hooks  /* register hooks                      */
};
