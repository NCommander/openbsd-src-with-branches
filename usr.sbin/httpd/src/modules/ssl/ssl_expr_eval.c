/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |  mod_ssl
** | '_ ` _ \ / _ \ / _` |   / __/ __| |  Apache Interface to OpenSSL
** | | | | | | (_) | (_| |   \__ \__ \ |  www.modssl.org
** |_| |_| |_|\___/ \__,_|___|___/___/_|  ftp.modssl.org
**                      |_____|
**  ssl_expr_eval.c
**  Expression Evaluation
*/

/* ====================================================================
 * Copyright (c) 1998-2001 Ralf S. Engelschall. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by
 *     Ralf S. Engelschall <rse@engelschall.com> for use in the
 *     mod_ssl project (http://www.modssl.org/)."
 *
 * 4. The names "mod_ssl" must not be used to endorse or promote
 *    products derived from this software without prior written
 *    permission. For written permission, please contact
 *    rse@engelschall.com.
 *
 * 5. Products derived from this software may not be called "mod_ssl"
 *    nor may "mod_ssl" appear in their names without prior
 *    written permission of Ralf S. Engelschall.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by
 *     Ralf S. Engelschall <rse@engelschall.com> for use in the
 *     mod_ssl project (http://www.modssl.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY RALF S. ENGELSCHALL ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL RALF S. ENGELSCHALL OR
 * HIS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */
                             /* ``Make love,
                                  not software!''
                                        -- Unknown */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  Expression Evaluation
**  _________________________________________________________________
*/

static BOOL  ssl_expr_eval_comp(request_rec *, ssl_expr *);
static char *ssl_expr_eval_word(request_rec *, ssl_expr *);
static char *ssl_expr_eval_func_file(request_rec *, char *);
static int   ssl_expr_eval_strcmplex(char *, char *);

BOOL ssl_expr_eval(request_rec *r, ssl_expr *node)
{
    switch (node->node_op) {
        case op_True: {
            return TRUE;
        }
        case op_False: {
            return FALSE;
        }
        case op_Not: {
            ssl_expr *e = (ssl_expr *)node->node_arg1;
            return (!ssl_expr_eval(r, e));
        }
        case op_Or: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (ssl_expr_eval(r, e1) || ssl_expr_eval(r, e2));
        }
        case op_And: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (ssl_expr_eval(r, e1) && ssl_expr_eval(r, e2));
        }
        case op_Comp: {
            ssl_expr *e = (ssl_expr *)node->node_arg1;
            return ssl_expr_eval_comp(r, e);
        }
        default: {
            ssl_expr_error = "Internal evaluation error: Unknown expression node";
            return FALSE;
        }
    }
}

static BOOL ssl_expr_eval_comp(request_rec *r, ssl_expr *node)
{
    switch (node->node_op) {
        case op_EQ: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (strcmp(ssl_expr_eval_word(r, e1), ssl_expr_eval_word(r, e2)) == 0);
        }
        case op_NE: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (strcmp(ssl_expr_eval_word(r, e1), ssl_expr_eval_word(r, e2)) != 0);
        }
        case op_LT: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (ssl_expr_eval_strcmplex(ssl_expr_eval_word(r, e1), ssl_expr_eval_word(r, e2)) <  0);
        }
        case op_LE: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (ssl_expr_eval_strcmplex(ssl_expr_eval_word(r, e1), ssl_expr_eval_word(r, e2)) <= 0);
        }
        case op_GT: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (ssl_expr_eval_strcmplex(ssl_expr_eval_word(r, e1), ssl_expr_eval_word(r, e2)) >  0);
        }
        case op_GE: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            return (ssl_expr_eval_strcmplex(ssl_expr_eval_word(r, e1), ssl_expr_eval_word(r, e2)) >= 0);
        }
        case op_IN: {
            ssl_expr *e1 = (ssl_expr *)node->node_arg1;
            ssl_expr *e2 = (ssl_expr *)node->node_arg2;
            ssl_expr *e3;
            char *w1 = ssl_expr_eval_word(r, e1);
            BOOL found = FALSE;
            do {
                e3 = (ssl_expr *)e2->node_arg1;
                e2 = (ssl_expr *)e2->node_arg2;
                if (strcmp(w1, ssl_expr_eval_word(r, e3)) == 0) {
                    found = TRUE;
                    break;
                }
            } while (e2 != NULL);
            return found;
        }
        case op_REG: {
            ssl_expr *e1;
            ssl_expr *e2;
            char *word;
            regex_t *regex;

            e1 = (ssl_expr *)node->node_arg1;
            e2 = (ssl_expr *)node->node_arg2;
            word = ssl_expr_eval_word(r, e1);
            regex = (regex_t *)(e2->node_arg1);
            return (regexec(regex, word, 0, NULL, 0) == 0);
        }
        case op_NRE: {
            ssl_expr *e1;
            ssl_expr *e2;
            char *word;
            regex_t *regex;

            e1 = (ssl_expr *)node->node_arg1;
            e2 = (ssl_expr *)node->node_arg2;
            word = ssl_expr_eval_word(r, e1);
            regex = (regex_t *)(e2->node_arg1);
            return !(regexec(regex, word, 0, NULL, 0) == 0);
        }
        default: {
            ssl_expr_error = "Internal evaluation error: Unknown expression node";
            return FALSE;
        }
    }
}

static char *ssl_expr_eval_word(request_rec *r, ssl_expr *node)
{
    switch (node->node_op) {
        case op_Digit: {
            char *string = (char *)node->node_arg1;
            return string;
        }
        case op_String: {
            char *string = (char *)node->node_arg1;
            return string;
        }
        case op_Var: {
            char *var = (char *)node->node_arg1;
            char *val = ssl_var_lookup(r->pool, r->server, r->connection, r, var);
            return (val == NULL ? "" : val);
        }
        case op_Func: {
            char *name = (char *)node->node_arg1;
            ssl_expr *args = (ssl_expr *)node->node_arg2;
            if (strEQ(name, "file"))
                return ssl_expr_eval_func_file(r, (char *)(args->node_arg1));
            else {
                ssl_expr_error = "Internal evaluation error: Unknown function name";
                return "";
            }
        }
        default: {
            ssl_expr_error = "Internal evaluation error: Unknown expression node";
            return FALSE;
        }
    }
}

static char *ssl_expr_eval_func_file(request_rec *r, char *filename)
{
    FILE *fp;
    char *buf;
    int len;

    if ((fp = ap_pfopen(r->pool, filename, "r")) == NULL) {
        ssl_expr_error = "Cannot open file";
        return "";
    }
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    if (len == 0) {
        buf = (char *)ap_palloc(r->pool, sizeof(char) * 1);
        *buf = NUL;
    }
    else {
        if ((buf = (char *)ap_palloc(r->pool, sizeof(char) * len+1)) == NULL) {
            ssl_expr_error = "Cannot allocate memory";
            ap_pfclose(r->pool, fp);
            return "";
        }
        fseek(fp, 0, SEEK_SET);
        if (fread(buf, len, 1, fp) == 0) {
            ssl_expr_error = "Cannot read from file";
            fclose(fp);
            return ("");
        }
        buf[len] = NUL;
    }
    ap_pfclose(r->pool, fp);
    return buf;
}

/* a variant of strcmp(3) which works correctly also for number strings */
static int ssl_expr_eval_strcmplex(char *cpNum1, char *cpNum2)
{
    int i, n1, n2;

    if (cpNum1 == NULL)
        return -1;
    if (cpNum2 == NULL)
        return +1;
    n1 = strlen(cpNum1);
    n2 = strlen(cpNum2);
    if (n1 > n2)
        return 1;
    if (n1 < n2)
        return -1;
    for (i = 0; i < n1; i++) {
        if (cpNum1[i] > cpNum2[i])
            return 1;
        if (cpNum1[i] < cpNum2[i])
            return -1;
    }
    return 0;
}

