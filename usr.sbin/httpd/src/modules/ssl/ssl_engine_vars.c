/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |
** | '_ ` _ \ / _ \ / _` |   / __/ __| |
** | | | | | | (_) | (_| |   \__ \__ \ | mod_ssl - Apache Interface to SSLeay
** |_| |_| |_|\___/ \__,_|___|___/___/_| http://www.engelschall.com/sw/mod_ssl/
**                      |_____|
**  ssl_engine_vars.c
**  Variable Lookup Facility
*/

/* ====================================================================
 * Copyright (c) 1998-1999 Ralf S. Engelschall. All rights reserved.
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
 *     mod_ssl project (http://www.engelschall.com/sw/mod_ssl/)."
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
 *     mod_ssl project (http://www.engelschall.com/sw/mod_ssl/)."
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
                             /* ``Those of you who think they
                                  know everything are very annoying
                                  to those of us who do.''
                                                  -- Unknown       */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  Variable Lookup
**  _________________________________________________________________
*/

static char *ssl_var_lookup_header(pool *p, request_rec *r, const char *name);
static char *ssl_var_lookup_ssl(pool *p, conn_rec *c, char *var);
static char *ssl_var_lookup_ssl_cert(pool *p, X509 *xs, char *var);
static char *ssl_var_lookup_ssl_cert_dn(pool *p, X509_NAME *xsname, char *var);
static char *ssl_var_lookup_ssl_cert_valid(pool *p, ASN1_UTCTIME *tm);
static char *ssl_var_lookup_ssl_cert_serial(pool *p, X509 *xs);
static char *ssl_var_lookup_ssl_cert_chain(pool *p, STACK *sk, char *var);
static char *ssl_var_lookup_ssl_cert_PEM(pool *p, X509 *xs);
static char *ssl_var_lookup_ssl_cipher(pool *p, conn_rec *c, char *var);
static void  ssl_var_lookup_ssl_cipher_bits(char *cipher, int *usekeysize, int *algkeysize);
static char *ssl_var_lookup_ssl_version(pool *p, char *var);

void ssl_var_register(void)
{
    ap_hook_configure("ap::mod_ssl::var_lookup",
                      AP_HOOK_SIG6(ptr,ptr,ptr,ptr,ptr,ptr), AP_HOOK_DECLINE(NULL));
    ap_hook_register("ap::mod_ssl::var_lookup",
                     ssl_var_lookup, AP_HOOK_NOCTX);
    return;
}

void ssl_var_unregister(void)
{
    ap_hook_unregister("ap::mod_ssl::var_lookup", ssl_var_lookup);
    return;
}

char *ssl_var_lookup(pool *p, server_rec *s, conn_rec *c, request_rec *r, char *var)
{
    SSLModConfigRec *mc = myModConfig();
    char *result;
    BOOL resdup;
    time_t tc;
    struct tm *tm;

    result = NULL;
    resdup = TRUE;

    /*
     * When no pool is given try to find one
     */
    if (p == NULL) {
        if (r != NULL)
            p = r->pool;
        else if (c != NULL)
            p = c->pool;
        else
            p = mc->pPool;
    }

    /*
     * Request dependent stuff
     */
    if (r != NULL) {
        if (strcEQ(var, "HTTP_USER_AGENT"))
            result = ssl_var_lookup_header(p, r, "User-Agent");
        else if (strcEQ(var, "HTTP_REFERER"))
            result = ssl_var_lookup_header(p, r, "Referer");
        else if (strcEQ(var, "HTTP_COOKIE"))
            result = ssl_var_lookup_header(p, r, "Cookie");
        else if (strcEQ(var, "HTTP_FORWARDED"))
            result = ssl_var_lookup_header(p, r, "Forwarded");
        else if (strcEQ(var, "HTTP_HOST"))
            result = ssl_var_lookup_header(p, r, "Host");
        else if (strcEQ(var, "HTTP_PROXY_CONNECTION"))
            result = ssl_var_lookup_header(p, r, "Proxy-Connection");
        else if (strcEQ(var, "HTTP_ACCEPT"))
            result = ssl_var_lookup_header(p, r, "Accept");
        else if (strlen(var) > 5 && strcEQn(var, "HTTP:", 5))
            /* all other headers from which we are still not know about */
            result = ssl_var_lookup_header(p, r, var+5);
        else if (strcEQ(var, "THE_REQUEST"))
            result = r->the_request;
        else if (strcEQ(var, "REQUEST_METHOD"))
            result = r->method;
        else if (strcEQ(var, "REQUEST_SCHEME"))
            result = ap_http_method(r);
        else if (strcEQ(var, "REQUEST_URI"))
            result = r->uri;
        else if (strcEQ(var, "SCRIPT_FILENAME") ||
                 strcEQ(var, "REQUEST_FILENAME"))
            result = r->filename;
        else if (strcEQ(var, "PATH_INFO"))
            result = r->path_info;
        else if (strcEQ(var, "QUERY_STRING"))
            result = r->args;
        else if (strcEQ(var, "REMOTE_HOST"))
            result = (char *)ap_get_remote_host(r->connection,
                                                r->per_dir_config, REMOTE_NAME);
        else if (strcEQ(var, "REMOTE_IDENT"))
            result = (char *)ap_get_remote_logname(r);
        else if (strcEQ(var, "IS_SUBREQ"))
            result = (r->main != NULL ? "true" : "false");
        else if (strcEQ(var, "DOCUMENT_ROOT"))
            result = (char *)ap_document_root(r);
        else if (strcEQ(var, "SERVER_ADMIN"))
            result = r->server->server_admin;
        else if (strcEQ(var, "SERVER_NAME"))
            result = (char *)ap_get_server_name(r);
        else if (strcEQ(var, "SERVER_PORT"))
            result = ap_psprintf(p, "%u", ap_get_server_port(r));
        else if (strcEQ(var, "SERVER_PROTOCOL"))
            result = r->protocol;
    }

    /*
     * Connection stuff
     */
    if (result == NULL && c != NULL) {
        if (strcEQ(var, "REMOTE_ADDR"))
            result = c->remote_ip;
        else if (strcEQ(var, "REMOTE_USER"))
            result = c->user;
        else if (strcEQ(var, "AUTH_TYPE"))
            result = c->ap_auth_type;
        else if (strlen(var) > 4 && strcEQn(var, "SSL_", 4))
            result = ssl_var_lookup_ssl(p, c, var+4);
    }

    /*
     * Totally independent stuff
     */
    if (result == NULL) {
        if (strlen(var) > 12 && strcEQn(var, "SSL_VERSION_", 12))
            result = ssl_var_lookup_ssl_version(p, var+12);
        else if (strcEQ(var, "SERVER_SOFTWARE"))
            result = (char *)ap_get_server_version();
        else if (strcEQ(var, "API_VERSION")) {
            result = ap_psprintf(p, "%d", MODULE_MAGIC_NUMBER);
            resdup = FALSE;
        }
        else if (strcEQ(var, "TIME_YEAR")) {
            tc = time(NULL);
            tm = localtime(&tc);
            result = ap_psprintf(p, "%02d%02d",
                                 (tm->tm_year / 100) + 19, tm->tm_year % 100);
            resdup = FALSE;
        }
#define MKTIMESTR(format, tmfield) \
            tc = time(NULL); \
            tm = localtime(&tc); \
            result = ap_psprintf(p, format, tm->tmfield); \
            resdup = FALSE;
        else if (strcEQ(var, "TIME_MON")) {
            MKTIMESTR("%02d", tm_mon+1)
        }
        else if (strcEQ(var, "TIME_DAY")) {
            MKTIMESTR("%02d", tm_mday)
        }
        else if (strcEQ(var, "TIME_HOUR")) {
            MKTIMESTR("%02d", tm_hour)
        }
        else if (strcEQ(var, "TIME_MIN")) {
            MKTIMESTR("%02d", tm_min)
        }
        else if (strcEQ(var, "TIME_SEC")) {
            MKTIMESTR("%02d", tm_sec)
        }
        else if (strcEQ(var, "TIME_WDAY")) {
            MKTIMESTR("%d", tm_wday)
        }
        else if (strcEQ(var, "TIME")) {
            tc = time(NULL);
            tm = localtime(&tc);
            result = ap_psprintf(p,
                        "%02d%02d%02d%02d%02d%02d%02d", (tm->tm_year / 100) + 19,
                        (tm->tm_year % 100), tm->tm_mon+1, tm->tm_mday,
                        tm->tm_hour, tm->tm_min, tm->tm_sec);
            resdup = FALSE;
        }
        /* all other env-variables from the parent Apache process */
        else if (strlen(var) > 4 && strcEQn(var, "ENV:", 4)) {
            result = (char *)ap_table_get(r->notes, var+4);
            if (result == NULL)
                result = (char *)ap_table_get(r->subprocess_env, var+4);
            if (result == NULL)
                result = getenv(var+4);
        }
    }

    if (result != NULL && resdup)
        result = ap_pstrdup(p, result);
    if (result == NULL)
        result = "";
    return result;
}

static char *ssl_var_lookup_header(pool *p, request_rec *r, const char *name)
{
    array_header *hdrs_arr;
    table_entry *hdrs;
    int i;

    hdrs_arr = ap_table_elts(r->headers_in);
    hdrs = (table_entry *)hdrs_arr->elts;
    for (i = 0; i < hdrs_arr->nelts; ++i) {
        if (hdrs[i].key == NULL)
            continue;
        if (strcEQ(hdrs[i].key, name))
            return ap_pstrdup(p, hdrs[i].val);
    }
    return NULL;
}

static char *ssl_var_lookup_ssl(pool *p, conn_rec *c, char *var)
{
    char *result;
    X509 *xs;
    STACK *sk;
    SSL *ssl;

    result = NULL;

    if (strlen(var) > 8 && strcEQn(var, "VERSION_", 8)) {
        result = ssl_var_lookup_ssl_version(p, var+8);
    }
    else if (strcEQ(var, "PROTOCOL")) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        result = SSL_get_version(ssl);
    }
    else if (strlen(var) >= 6 && strcEQn(var, "CIPHER", 6)) {
        result = ssl_var_lookup_ssl_cipher(p, c, var+6);
    }
    else if (strlen(var) > 18 && strcEQn(var, "CLIENT_CERT_CHAIN_", 18)) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        sk = SSL_get_peer_cert_chain(ssl);
        result = ssl_var_lookup_ssl_cert_chain(p, sk, var+17);
    }
    else if (strlen(var) > 7 && strcEQn(var, "CLIENT_", 7)) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        if ((xs = SSL_get_peer_certificate(ssl)) != NULL)
            result = ssl_var_lookup_ssl_cert(p, xs, var+7);
    }
    else if (strlen(var) > 7 && strcEQn(var, "SERVER_", 7)) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        if ((xs = SSL_get_certificate(ssl)) != NULL)
            result = ssl_var_lookup_ssl_cert(p, xs, var+7);
    }
    return result;
}

static char *ssl_var_lookup_ssl_cert(pool *p, X509 *xs, char *var)
{
    char *result;
    BOOL resdup;
    X509_NAME *xsname;
    int nid;
    char *cp;

    result = NULL;
    resdup = TRUE;

    if (strcEQ(var, "M_VERSION")) {
        result = ap_psprintf(p, "%lu", X509_get_version(xs)+1);
        resdup = FALSE;
    }
    else if (strcEQ(var, "M_SERIAL")) {
        result = ssl_var_lookup_ssl_cert_serial(p, xs);
    }
    else if (strcEQ(var, "V_START")) {
        result = ssl_var_lookup_ssl_cert_valid(p, X509_get_notBefore(xs));
    }
    else if (strcEQ(var, "V_END")) {
        result = ssl_var_lookup_ssl_cert_valid(p, X509_get_notAfter(xs));
    }
    else if (strcEQ(var, "S_DN")) {
        xsname = X509_get_subject_name(xs);
        cp = X509_NAME_oneline(xsname, NULL, 0);
        result = ap_pstrdup(p, cp);
        free(cp);
        resdup = FALSE;
    }
    else if (strlen(var) > 5 && strcEQn(var, "S_DN_", 5)) {
        xsname = X509_get_subject_name(xs);
        result = ssl_var_lookup_ssl_cert_dn(p, xsname, var+5);
        resdup = FALSE;
    }
    else if (strcEQ(var, "I_DN")) {
        xsname = X509_get_issuer_name(xs);
        cp = X509_NAME_oneline(xsname, NULL, 0);
        result = ap_pstrdup(p, cp);
        free(cp);
        resdup = FALSE;
    }
    else if (strlen(var) > 5 && strcEQn(var, "I_DN_", 5)) {
        xsname = X509_get_issuer_name(xs);
        result = ssl_var_lookup_ssl_cert_dn(p, xsname, var+5);
        resdup = FALSE;
    }
    else if (strcEQ(var, "A_SIG")) {
        nid = OBJ_obj2nid(xs->cert_info->signature->algorithm);
        result = ap_pstrdup(p, (nid == NID_undef) ? "UNKNOWN" : OBJ_nid2ln(nid));
        resdup = FALSE;
    }
    else if (strcEQ(var, "A_KEY")) {
        nid = OBJ_obj2nid(xs->cert_info->key->algor->algorithm);
        result = ap_pstrdup(p, (nid == NID_undef) ? "UNKNOWN" : OBJ_nid2ln(nid));
        resdup = FALSE;
    }
    else if (strcEQ(var, "CERT")) {
        result = ssl_var_lookup_ssl_cert_PEM(p, xs);
    }

    if (result != NULL && resdup)
        result = ap_pstrdup(p, result);
    return result;
}

static const struct {
    char *name;
    int   nid;
} ssl_var_lookup_ssl_cert_dn_rec[] = {
    { "C",     NID_countryName            },
    { "SP",    NID_stateOrProvinceName    },
    { "L",     NID_localityName           },
    { "O",     NID_organizationName       },
    { "OU",    NID_organizationalUnitName },
    { "CN",    NID_commonName             },
    { "Email", NID_pkcs9_emailAddress     },
    { NULL,    0                          }
};

static char *ssl_var_lookup_ssl_cert_dn(pool *p, X509_NAME *xsname, char *var)
{
    char *result;
    X509_NAME_ENTRY *xsne;
    int i, j, n;

    result = NULL;

    for (i = 0; ssl_var_lookup_ssl_cert_dn_rec[i].name != NULL; i++) {
        if (strEQ(var, ssl_var_lookup_ssl_cert_dn_rec[i].name)) {
            for (j = 0; j < sk_num(xsname->entries); j++) {
                xsne = (X509_NAME_ENTRY *)sk_value(xsname->entries, j);
                n = OBJ_obj2nid(xsne->object);
                if (n == ssl_var_lookup_ssl_cert_dn_rec[i].nid) {
                    result = ap_palloc(p, xsne->value->length+1);
                    ap_cpystrn(result, (char *)xsne->value->data, xsne->value->length+1);
                    result[xsne->value->length] = NUL;
                    break;
                }
            }
            break;
        }
    }
    return result;
}

static char *ssl_var_lookup_ssl_cert_valid(pool *p, ASN1_UTCTIME *tm)
{
    char *result;
    BIO* bio;
    int n;

    if ((bio = BIO_new(BIO_s_mem())) == NULL)
        return NULL;
    ASN1_UTCTIME_print(bio, tm);
    n = BIO_pending(bio);
    result = ap_pcalloc(p, n+1);
    n = BIO_read(bio, result, n);
    result[n] = NUL;
    BIO_free(bio);
    return result;
}

static char *ssl_var_lookup_ssl_cert_serial(pool *p, X509 *xs)
{
    char *result;
    BIO* bio;
    int n;

    if ((bio = BIO_new(BIO_s_mem())) == NULL)
        return NULL;
    i2a_ASN1_INTEGER(bio, X509_get_serialNumber(xs));
    n = BIO_pending(bio);
    result = ap_pcalloc(p, n+1);
    n = BIO_read(bio, result, n);
    result[n] = NUL;
    BIO_free(bio);
    return result;
}

static char *ssl_var_lookup_ssl_cert_chain(pool *p, STACK *sk, char *var)
{
    char *result;
    X509 *xs;
    int n;

    result = NULL;

    if (strspn(var, "0123456789") == strlen(var)) {
        n = atoi(var);
        if (sk_num(sk) >= n) {
            xs = (X509 *)sk_value(sk, n);
            result = ssl_var_lookup_ssl_cert_PEM(p, xs);
        }
    }

    return result;
}

static char *ssl_var_lookup_ssl_cert_PEM(pool *p, X509 *xs)
{
    char *result;
    BIO *bio;
    int n;

    if ((bio = BIO_new(BIO_s_mem())) == NULL)
        return NULL;
    PEM_write_bio_X509(bio, xs);
    n = BIO_pending(bio);
    result = ap_pcalloc(p, n+1);
    n = BIO_read(bio, result, n);
    result[n] = NUL;
    BIO_free(bio);
    return result;
}

static char *ssl_var_lookup_ssl_cipher(pool *p, conn_rec *c, char *var)
{
    char *result;
    BOOL resdup;
    char *cipher;
    int usekeysize, algkeysize;
    SSL *ssl;

    result = NULL;
    resdup = TRUE;

    if (strEQ(var, "")) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        result = SSL_get_cipher_name(ssl);
    }
    else if (strcEQ(var, "_EXPORT")) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        cipher = SSL_get_cipher_name(ssl);
        ssl_var_lookup_ssl_cipher_bits(cipher, &usekeysize, &algkeysize);
        result = (usekeysize < 56 ? "true" : "false");
    }
    else if (strcEQ(var, "_USEKEYSIZE")) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        cipher = SSL_get_cipher_name(ssl);
        ssl_var_lookup_ssl_cipher_bits(cipher, &usekeysize, &algkeysize);
        result = ap_psprintf(p, "%d", usekeysize);
        resdup = FALSE;
    }
    else if (strcEQ(var, "_ALGKEYSIZE")) {
        ssl = ap_ctx_get(c->client->ctx, "ssl");
        cipher = SSL_get_cipher_name(ssl);
        ssl_var_lookup_ssl_cipher_bits(cipher, &usekeysize, &algkeysize);
        result = ap_psprintf(p, "%d", algkeysize);
        resdup = FALSE;
    }

    if (result != NULL && resdup)
        result = ap_pstrdup(p, result);
    return result;
}

/*
 * This structure is used instead of SSL_get_cipher_bits() because
 * this SSLeay function has rounding problems, but we want the
 * correct sizes.
 */
static const struct {
    char *szName;
    int nUseKeySize;
    int nAlgKeySize;
} ssl_var_lookup_ssl_cipher_bits_rec[] = {
    { SSL3_TXT_RSA_IDEA_128_SHA          /*IDEA-CBC-SHA*/,           128, 128 },
    { SSL3_TXT_RSA_NULL_MD5              /*NULL-MD5*/,                 0,   0 },
    { SSL3_TXT_RSA_NULL_SHA              /*NULL-SHA*/,                 0,   0 },
    { SSL3_TXT_RSA_RC4_40_MD5            /*EXP-RC4-MD5*/,             40, 128 },
    { SSL3_TXT_RSA_RC4_128_MD5           /*RC4-MD5*/,                128, 128 },
    { SSL3_TXT_RSA_RC4_128_SHA           /*RC4-SHA*/,                128, 128 },
    { SSL3_TXT_RSA_RC2_40_MD5            /*EXP-RC2-CBC-MD5*/,         40, 128 },
    { SSL3_TXT_RSA_IDEA_128_SHA          /*IDEA-CBC-MD5*/,           128, 128 },
    { SSL3_TXT_RSA_DES_40_CBC_SHA        /*EXP-DES-CBC-SHA*/,         40,  56 },
    { SSL3_TXT_RSA_DES_64_CBC_SHA        /*DES-CBC-SHA*/ ,            56,  56 },
    { SSL3_TXT_RSA_DES_192_CBC3_SHA      /*DES-CBC3-SHA*/ ,          168, 168 },
    { SSL3_TXT_DH_DSS_DES_40_CBC_SHA     /*EXP-DH-DSS-DES-CBC-SHA*/,  40,  56 },
    { SSL3_TXT_DH_DSS_DES_64_CBC_SHA     /*DH-DSS-DES-CBC-SHA*/,      56,  56 },
    { SSL3_TXT_DH_DSS_DES_192_CBC3_SHA   /*DH-DSS-DES-CBC3-SHA*/,    168, 168 },
    { SSL3_TXT_DH_RSA_DES_40_CBC_SHA     /*EXP-DH-RSA-DES-CBC-SHA*/,  40,  56 },
    { SSL3_TXT_DH_RSA_DES_64_CBC_SHA     /*DH-RSA-DES-CBC-SHA*/,      56,  56 },
    { SSL3_TXT_DH_RSA_DES_192_CBC3_SHA   /*DH-RSA-DES-CBC3-SHA*/,    168, 168 },
    { SSL3_TXT_EDH_DSS_DES_40_CBC_SHA    /*EXP-EDH-DSS-DES-CBC-SHA*/, 40,  56 },
    { SSL3_TXT_EDH_DSS_DES_64_CBC_SHA    /*EDH-DSS-DES-CBC-SHA*/,     56,  56 },
    { SSL3_TXT_EDH_DSS_DES_192_CBC3_SHA  /*EDH-DSS-DES-CBC3-SHA*/,   168, 168 },
    { SSL3_TXT_EDH_RSA_DES_40_CBC_SHA    /*EXP-EDH-RSA-DES-CBC*/,     40,  56 },
    { SSL3_TXT_EDH_RSA_DES_64_CBC_SHA    /*EDH-RSA-DES-CBC-SHA*/,     56,  56 },
    { SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA  /*EDH-RSA-DES-CBC3-SHA*/,   168, 168 },
    { SSL3_TXT_ADH_RC4_40_MD5            /*EXP-ADH-RC4-MD5*/,         40, 128 },
    { SSL3_TXT_ADH_RC4_128_MD5           /*ADH-RC4-MD5*/,            128, 128 },
    { SSL3_TXT_ADH_DES_40_CBC_SHA        /*EXP-ADH-DES-CBC-SHA*/,     40, 128 },
    { SSL3_TXT_ADH_DES_64_CBC_SHA        /*ADH-DES-CBC-SHA*/,         56,  56 },
    { SSL3_TXT_ADH_DES_192_CBC_SHA       /*ADH-DES-CBC3-SHA*/,       168, 168 },
    { SSL3_TXT_FZA_DMS_NULL_SHA          /*FZA-NULL-SHA*/,             0,   0 },
    { SSL3_TXT_FZA_DMS_FZA_SHA           /*FZA-FZA-CBC-SHA*/,          0,   0 },
    { SSL3_TXT_FZA_DMS_RC4_SHA           /*FZA-RC4-SHA*/,            128, 128 },
    { SSL2_TXT_DES_64_CFB64_WITH_MD5_1   /*DES-CFB-M1*/,              56,  56 },
    { SSL2_TXT_RC2_128_CBC_WITH_MD5      /*RC2-CBC-MD5*/,            128, 128 },
    { SSL2_TXT_DES_64_CBC_WITH_MD5       /*DES-CBC-MD5*/,             56,  56 },
    { SSL2_TXT_DES_192_EDE3_CBC_WITH_MD5 /*DES-CBC3-MD5*/,           168, 168 },
    { SSL2_TXT_RC4_64_WITH_MD5           /*RC4-64-MD5*/,              64,  64 },
    { SSL2_TXT_NULL                      /*NULL*/,                     0,   0 },
    { NULL,                                                            0,   0 }
};

static void ssl_var_lookup_ssl_cipher_bits(char *cipher, int *usekeysize, int *algkeysize)
{
    int n;

    *usekeysize = 0;
    *algkeysize = 0;
    for (n = 0; ssl_var_lookup_ssl_cipher_bits_rec[n].szName; n++) {
        if (strEQ(cipher, ssl_var_lookup_ssl_cipher_bits_rec[n].szName)) {
            *algkeysize = ssl_var_lookup_ssl_cipher_bits_rec[n].nAlgKeySize;
            *usekeysize = ssl_var_lookup_ssl_cipher_bits_rec[n].nUseKeySize;
            break;
        }
    }
    return;
}

static char *ssl_var_lookup_ssl_version(pool *p, char *var)
{
    char *result;
    char *cp, *cp2;

    result = NULL;

    if (strEQ(var, "PRODUCT")) {
#if defined(SSL_PRODUCT_NAME) && defined(SSL_PRODUCT_VERSION)
        result = ap_psprintf(p, "%s/%s", SSL_PRODUCT_NAME, SSL_PRODUCT_VERSION);
#else
        result = NULL;
#endif
    }
    else if (strEQ(var, "INTERFACE")) {
        result = ap_psprintf(p, "mod_ssl/%s", MOD_SSL_VERSION);
    }
    else if (strEQ(var, "LIBRARY")) {
        result = ap_pstrdup(p, SSLeay_version(SSLEAY_VERSION));
        if ((cp = strchr(result, ' ')) != NULL) {
            *cp = '/';
            if ((cp2 = strchr(cp, ' ')) != NULL)
                *cp2 = NUL;
        }
    }
    return result;
}

