/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_strings.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_log.h"


// yea, yea, global variables is bad.

char *tmp_var;
char *tmp;

typedef struct {
    apr_table_t *vars;
    apr_table_t *unsetenv;
} env_dir_config_rec;

module AP_MODULE_DECLARE_DATA php_fpm_module;

static void *create_env_dir_config(apr_pool_t *p, char *dummy)
{
    env_dir_config_rec *conf = apr_palloc(p, sizeof(*conf));

    conf->vars = apr_table_make(p, 10);
    conf->unsetenv = apr_table_make(p, 10);
    // allocate flags string and tmp string. TODO: Add check if it was allocated correctly
    tmp_var = apr_palloc(p,32768);
    tmp = apr_palloc(p, 512);
    memset(tmp_var,0,32768);

    return conf;
}

static void *merge_env_dir_configs(apr_pool_t *p, void *basev, void *addv)
{
    env_dir_config_rec *base = basev;
    env_dir_config_rec *add = addv;
    env_dir_config_rec *res = apr_palloc(p, sizeof(*res));

    const apr_table_entry_t *elts;
    const apr_array_header_t *arr;

    int i;

    /*
     * res->vars = copy_table( p, base->vars );
     * foreach $unsetenv ( @add->unsetenv )
     *     table_unset( res->vars, $unsetenv );
     * foreach $element ( @add->vars )
     *     table_set( res->vars, $element.key, $element.val );
     *
     * add->unsetenv already removed the vars from add->vars,
     * if they preceded the UnsetEnv directive.
     */
    res->vars = apr_table_copy(p, base->vars);
    res->unsetenv = NULL;

    arr = apr_table_elts(add->unsetenv);
    if (arr) {
        elts = (const apr_table_entry_t *)arr->elts;

        for (i = 0; i < arr->nelts; ++i) {
            apr_table_unset(res->vars, elts[i].key);
        }
    }

    arr = apr_table_elts(add->vars);
    if (arr) {
        elts = (const apr_table_entry_t *)arr->elts;

        for (i = 0; i < arr->nelts; ++i) {
            apr_table_setn(res->vars, elts[i].key, elts[i].val);
        }
    }

    return res;
}

static const char *add_env_module_vars_passed(cmd_parms *cmd, void *sconf_,
                                              const char *arg)
{
    env_dir_config_rec *sconf = sconf_;
    apr_table_t *vars = sconf->vars;
    const char *env_var;

    env_var = getenv(arg);
    if (env_var != NULL) {
        apr_table_setn(vars, arg, apr_pstrdup(cmd->pool, env_var));
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, cmd->server, APLOGNO(01506)
                     "PassEnv variable %s was undefined", arg);
    }

    return NULL;
}

static const char *add_env_module_vars_set(cmd_parms *cmd, void *sconf_,
                                           const char *name, const char *value)
{
    env_dir_config_rec *sconf = sconf_;
    //char tmp[512];

    if (ap_strchr_c(name, '=')) {
        char *env, *plast;

        env = apr_strtok(apr_pstrdup(cmd->temp_pool, name), "=", &plast);

        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, cmd->server, APLOGNO(10032)
                     "Spurious usage of '=' in an environment variable name. "
                     "'%s %s %s' expected instead?", cmd->cmd->name, env, plast);
    }

    /* name is mandatory, value is optional.  no value means
     * set the variable to an empty string
     */
    //apr_table_setn(sconf->vars, name, value ? value : "");
    if(strlen(tmp_var) < 32767) {
        snprintf(tmp, 511, "%s=%s;\n", name, value);
        strncat(tmp_var, tmp, 32767-strlen(tmp_var));
    }
    apr_table_setn(sconf->vars, "PHP_VALUE", tmp_var ? tmp_var : "");
    return NULL;
}

static const command_rec env_module_cmds[] =
{
AP_INIT_TAKE12("php_value", add_env_module_vars_set, NULL,
     OR_FILEINFO, "an PHP variable name and optional value to pass to CGI/FPM"),
AP_INIT_TAKE12("php_flag", add_env_module_vars_set, NULL,
     OR_FILEINFO, "an PHP variable name and optional value to pass to CGI/FPM"),
AP_INIT_TAKE12("php_admin_value", add_env_module_vars_set, NULL,
     ACCESS_CONF|RSRC_CONF, "an PHP variable name and optional value to pass to CGI/FPM"),
AP_INIT_TAKE12("php_admin_flag", add_env_module_vars_set, NULL,
     ACCESS_CONF|RSRC_CONF, "an PHP variable name and optional value to pass to CGI/FPM"),
    {NULL},
};

static int fixup_env_module(request_rec *r)
{
    env_dir_config_rec *sconf = ap_get_module_config(r->per_dir_config,
                                                     &php_fpm_module);

    if (apr_is_empty_table(sconf->vars)) {
        return DECLINED;
    }

    r->subprocess_env = apr_table_overlay(r->pool, r->subprocess_env,
            sconf->vars);

    return OK;
}

static void register_hooks(apr_pool_t *p)
{
    ap_hook_fixups(fixup_env_module, NULL, NULL, APR_HOOK_MIDDLE);
}

AP_DECLARE_MODULE(php_fpm) =
{
    STANDARD20_MODULE_STUFF,
    create_env_dir_config,      /* dir config creater */
    merge_env_dir_configs,      /* dir merger --- default is to override */
    NULL,                       /* server config */
    NULL,                       /* merge server configs */
    env_module_cmds,            /* command apr_table_t */
    register_hooks              /* register hooks */
};
