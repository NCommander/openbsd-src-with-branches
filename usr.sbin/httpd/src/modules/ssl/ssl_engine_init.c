/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |  mod_ssl
** | '_ ` _ \ / _ \ / _` |   / __/ __| |  Apache Interface to OpenSSL
** | | | | | | (_) | (_| |   \__ \__ \ |  www.modssl.org
** |_| |_| |_|\___/ \__,_|___|___/___/_|  ftp.modssl.org
**                      |_____|
**  ssl_engine_init.c
**  Initialization of Servers
*/

/* ====================================================================
 * Copyright (c) 1998-2000 Ralf S. Engelschall. All rights reserved.
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

/* ====================================================================
 * Copyright (c) 1995-1999 Ben Laurie. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by Ben Laurie
 *    for use in the Apache-SSL HTTP server project."
 *
 * 4. The name "Apache-SSL Server" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Ben Laurie
 *    for use in the Apache-SSL HTTP server project."
 *
 * THIS SOFTWARE IS PROVIDED BY BEN LAURIE ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL BEN LAURIE OR
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
                             /* ``Recursive, adj.;
                                  see Recursive.''
                                        -- Unknown   */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  Module Initialization
**  _________________________________________________________________
*/

/*
 *  Per-module initialization
 */
void ssl_init_Module(server_rec *s, pool *p)
{
    SSLModConfigRec *mc = myModConfig();
    SSLSrvConfigRec *sc;
    server_rec *s2;
    char *cp;
    int n;

    mc->nInitCount++;

    /*
     * Let us cleanup on restarts and exists
     */
    ap_register_cleanup(p, s, ssl_init_ModuleKill, ssl_init_ChildKill);

    /*
     * Any init round fixes the global config
     */
    ssl_config_global_create(); /* just to avoid problems */
    ssl_config_global_fix();

    /*
     *  try to fix the configuration and open the dedicated SSL
     *  logfile as early as possible
     */
    for (s2 = s; s2 != NULL; s2 = s2->next) {
        sc = mySrvConfig(s2);

        /* Fix up stuff that may not have been set */
        if (sc->bEnabled == UNSET)
            sc->bEnabled = FALSE;
        if (sc->nVerifyClient == SSL_CVERIFY_UNSET)
            sc->nVerifyClient = SSL_CVERIFY_NONE;
        if (sc->nVerifyDepth == UNSET)
            sc->nVerifyDepth = 1;
        if (sc->nSessionCacheTimeout == UNSET)
            sc->nSessionCacheTimeout = SSL_SESSION_CACHE_TIMEOUT;
        if (sc->nPassPhraseDialogType == SSL_PPTYPE_UNSET)
            sc->nPassPhraseDialogType = SSL_PPTYPE_BUILTIN;

        /* Open the dedicated SSL logfile */
        ssl_log_open(s, s2, p);
    }

    /*
     * Identification
     */
    if (mc->nInitCount == 1) {
        ssl_log(s, SSL_LOG_INFO, "Server: %s, Interface: %s, Library: %s",
                SERVER_BASEVERSION,
                ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_INTERFACE"),
                ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_LIBRARY"));
#ifdef WIN32
        ssl_log(s, SSL_LOG_WARN, "You are using mod_ssl under Win32. " 
                "This combination is *NOT* officially supported. "
                "Use it at your own risk!");
#endif
    }

    /*
     * Initialization round information
     */
    if (mc->nInitCount == 1)
        ssl_log(s, SSL_LOG_INFO, "Init: 1st startup round (still not detached)");
    else if (mc->nInitCount == 2)
        ssl_log(s, SSL_LOG_INFO, "Init: 2nd startup round (already detached)");
    else
        ssl_log(s, SSL_LOG_INFO, "Init: %d%s restart round (already detached)",
                mc->nInitCount-2, (mc->nInitCount-2) == 1 ? "st" : "nd");

#ifdef SSL_VENDOR
    ap_hook_use("ap::mod_ssl::vendor::init_module",
                AP_HOOK_SIG3(void,ptr,ptr), AP_HOOK_ALL, s, p);
#endif

    /*
     *  The initialization phase inside the Apache API is totally bogus.
     *  We actually have three non-trivial problems:
     *
     *  1. Under Unix the API does a 2-round initialization of modules while
     *     under Win32 it doesn't. This means we have to make sure that at
     *     least the pass phrase dialog doesn't occur twice.  We overcome this
     *     problem by using a counter (mc->nInitCount) which has to
     *     survive the init rounds.
     *
     *  2. Between the first and the second round Apache detaches from
     *     the terminal under Unix. This means that our pass phrase dialog
     *     _has_ to be done in the first round and _cannot_ be done in the
     *     second round.
     *
     *  3. When Dynamic Shared Object (DSO) mechanism is used under Unix the
     *     module segment (code & data) gets unloaded and re-loaded between
     *     the first and the second round. This means no global data survives
     *     between first and the second init round. We overcome this by using
     *     an entry ("ssl_module") inside the ap_global_ctx.
     *
     *  The situation as a table:
     *
     *  Unix/static Unix/DSO          Win32     Action Required
     *              (-DSHARED_MODULE) (-DWIN32)
     *  ----------- ----------------- --------- -----------------------------------
     *  -           load module       -         -
     *  init        init              init      SSL library init, Pass Phrase Dialog
     *  detach      detach            -         -
     *  -           reload module     -         -
     *  init        init              -         SSL library init, mod_ssl init
     *
     *  Ok, now try to solve this totally ugly situation...
     */

#ifdef SHARED_MODULE
    ssl_log(s, SSL_LOG_INFO, "Init: %snitializing %s library",
            mc->nInitCount == 1 ? "I" : "Rei", SSL_LIBRARY_NAME);
    ssl_init_SSLLibrary();
#else
    if (mc->nInitCount <= 2) {
        ssl_log(s, SSL_LOG_INFO, "Init: %snitializing %s library",
                mc->nInitCount == 1 ? "I" : "Rei", SSL_LIBRARY_NAME);
        ssl_init_SSLLibrary();
    }
#endif
    if (mc->nInitCount == 1) {
        ssl_pphrase_Handle(s, p);
        ssl_init_TmpKeysHandle(SSL_TKP_GEN, s, p);
#ifndef WIN32
        return;
#endif
    }

    /*
     * Warn the user that he should use the session cache.
     * But we can operate without it, of course.
     */
    if (mc->nSessionCacheMode == SSL_SCMODE_UNSET) {
        ssl_log(s, SSL_LOG_WARN,
                "Init: Session Cache is not configured [hint: SSLSessionCache]");
        mc->nSessionCacheMode = SSL_SCMODE_NONE;
    }

    /*
     *  initialize the mutex handling and session caching
     */
    ssl_mutex_init(s, p);
    ssl_scache_init(s, p);

    /*
     * Seed the Pseudo Random Number Generator (PRNG)
     */
    n = ssl_rand_seed(s, p, SSL_RSCTX_STARTUP);
    ssl_log(s, SSL_LOG_INFO, "Init: Seeding PRNG with %d bytes of entropy", n);

    /*
     *  allocate the temporary RSA keys and DH params
     */
    ssl_init_TmpKeysHandle(SSL_TKP_ALLOC, s, p);

    /*
     *  initialize servers
     */
    ssl_log(s, SSL_LOG_INFO, "Init: Initializing (virtual) servers for SSL");
    for (s2 = s; s2 != NULL; s2 = s2->next) {
        sc = mySrvConfig(s2);
        /*
         * Either now skip this server when SSL is disabled for
         * it or give out some information about what we're
         * configuring.
         */
        if (!sc->bEnabled)
            continue;
        ssl_log(s2, SSL_LOG_INFO,
                "Init: Configuring server %s for SSL protocol",
                ssl_util_vhostid(p, s2));

        /*
         * Read the server certificate and key
         */
        ssl_init_ConfigureServer(s2, p, sc);
    }

    /*
     * Configuration consistency checks
     */
    ssl_init_CheckServers(s, p);

    /*
     *  Announce mod_ssl and SSL library in HTTP Server field
     *  as ``mod_ssl/X.X.X OpenSSL/X.X.X''
     */
    if ((cp = ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_PRODUCT")) != NULL && cp[0] != NUL)
        ap_add_version_component(cp);
    ap_add_version_component(ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_INTERFACE"));
    ap_add_version_component(ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_LIBRARY"));

    return;
}

/*
 *  Initialize SSL library (also already needed for the pass phrase dialog)
 */
void ssl_init_SSLLibrary(void)
{
#ifdef WIN32
    CRYPTO_malloc_init();
#endif
    SSL_load_error_strings();
    SSL_library_init();
    ssl_util_thread_setup();
    X509V3_add_standard_extensions();
    return;
}

/*
 * Handle the Temporary RSA Keys and DH Params
 */
void ssl_init_TmpKeysHandle(int action, server_rec *s, pool *p)
{
    SSLModConfigRec *mc = myModConfig();
    ssl_asn1_t *asn1;
    unsigned char *ucp;
    RSA *rsa;
    DH *dh;

    /* Generate Keys and Params */
    if (action == SSL_TKP_GEN) {

        ssl_log(s, SSL_LOG_INFO, "Init: Generating temporary RSA private keys (512/1024 bits)");

        /* generate 512 bit RSA key */
        if ((rsa = RSA_generate_key(512, RSA_F4, NULL, NULL)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR, "Init: Failed to generate temporary 512 bit RSA private key");
            ssl_die();
        }
        asn1 = (ssl_asn1_t *)ssl_ds_table_push(mc->tTmpKeys, "RSA:512");
        asn1->nData  = i2d_RSAPrivateKey(rsa, NULL);
        asn1->cpData = ap_palloc(mc->pPool, asn1->nData);
        ucp = asn1->cpData; i2d_RSAPrivateKey(rsa, &ucp); /* 2nd arg increments */
        RSA_free(rsa);

        /* generate 1024 bit RSA key */
        if ((rsa = RSA_generate_key(1024, RSA_F4, NULL, NULL)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR, "Init: Failed to generate temporary 1024 bit RSA private key");
            ssl_die();
        }
        asn1 = (ssl_asn1_t *)ssl_ds_table_push(mc->tTmpKeys, "RSA:1024");
        asn1->nData  = i2d_RSAPrivateKey(rsa, NULL);
        asn1->cpData = ap_palloc(mc->pPool, asn1->nData);
        ucp = asn1->cpData; i2d_RSAPrivateKey(rsa, &ucp); /* 2nd arg increments */
        RSA_free(rsa);

        ssl_log(s, SSL_LOG_INFO, "Init: Configuring temporary DH parameters (512/1024 bits)");

        /* import 512 bit DH param */
        if ((dh = ssl_dh_GetTmpParam(512)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR, "Init: Failed to import temporary 512 bit DH parameters");
            ssl_die();
        }
        asn1 = (ssl_asn1_t *)ssl_ds_table_push(mc->tTmpKeys, "DH:512");
        asn1->nData  = i2d_DHparams(dh, NULL);
        asn1->cpData = ap_palloc(mc->pPool, asn1->nData);
        ucp = asn1->cpData; i2d_DHparams(dh, &ucp); /* 2nd arg increments */
        /* no need to free dh, it's static */

        /* import 1024 bit DH param */
        if ((dh = ssl_dh_GetTmpParam(1024)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR, "Init: Failed to import temporary 1024 bit DH parameters");
            ssl_die();
        }
        asn1 = (ssl_asn1_t *)ssl_ds_table_push(mc->tTmpKeys, "DH:1024");
        asn1->nData  = i2d_DHparams(dh, NULL);
        asn1->cpData = ap_palloc(mc->pPool, asn1->nData);
        ucp = asn1->cpData; i2d_DHparams(dh, &ucp); /* 2nd arg increments */
        /* no need to free dh, it's static */
    }

    /* Allocate Keys and Params */
    else if (action == SSL_TKP_ALLOC) {

        ssl_log(s, SSL_LOG_INFO, "Init: Configuring temporary RSA private keys (512/1024 bits)");

        /* allocate 512 bit RSA key */
        if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tTmpKeys, "RSA:512")) != NULL) {
            ucp = asn1->cpData;
            if ((mc->pTmpKeys[SSL_TKPIDX_RSA512] = 
                 (void *)d2i_RSAPrivateKey(NULL, &ucp, asn1->nData)) == NULL) {
                ssl_log(s, SSL_LOG_ERROR, "Init: Failed to load temporary 512 bit RSA private key");
                ssl_die();
            }
        }

        /* allocate 1024 bit RSA key */
        if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tTmpKeys, "RSA:1024")) != NULL) {
            ucp = asn1->cpData;
            if ((mc->pTmpKeys[SSL_TKPIDX_RSA1024] = 
                 (void *)d2i_RSAPrivateKey(NULL, &ucp, asn1->nData)) == NULL) {
                ssl_log(s, SSL_LOG_ERROR, "Init: Failed to load temporary 1024 bit RSA private key");
                ssl_die();
            }
        }

        ssl_log(s, SSL_LOG_INFO, "Init: Configuring temporary DH parameters (512/1024 bits)");

        /* allocate 512 bit DH param */
        if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tTmpKeys, "DH:512")) != NULL) {
            ucp = asn1->cpData;
            if ((mc->pTmpKeys[SSL_TKPIDX_DH512] = 
                 (void *)d2i_DHparams(NULL, &ucp, asn1->nData)) == NULL) {
                ssl_log(s, SSL_LOG_ERROR, "Init: Failed to load temporary 512 bit DH parameters");
                ssl_die();
            }
        }

        /* allocate 1024 bit DH param */
        if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tTmpKeys, "DH:1024")) != NULL) {
            ucp = asn1->cpData;
            if ((mc->pTmpKeys[SSL_TKPIDX_DH1024] = 
                 (void *)d2i_DHparams(NULL, &ucp, asn1->nData)) == NULL) {
                ssl_log(s, SSL_LOG_ERROR, "Init: Failed to load temporary 1024 bit DH parameters");
                ssl_die();
            }
        }
    }

    /* Free Keys and Params */
    else if (action == SSL_TKP_FREE) {
        if (mc->pTmpKeys[SSL_TKPIDX_RSA512] != NULL) {
            RSA_free((RSA *)mc->pTmpKeys[SSL_TKPIDX_RSA512]);
            mc->pTmpKeys[SSL_TKPIDX_RSA512] = NULL;
        }
        if (mc->pTmpKeys[SSL_TKPIDX_RSA1024] != NULL) {
            RSA_free((RSA *)mc->pTmpKeys[SSL_TKPIDX_RSA1024]);
            mc->pTmpKeys[SSL_TKPIDX_RSA1024] = NULL;
        }
        if (mc->pTmpKeys[SSL_TKPIDX_DH512] != NULL) {
            DH_free((DH *)mc->pTmpKeys[SSL_TKPIDX_DH512]);
            mc->pTmpKeys[SSL_TKPIDX_DH512] = NULL;
        }
        if (mc->pTmpKeys[SSL_TKPIDX_DH1024] != NULL) {
            DH_free((DH *)mc->pTmpKeys[SSL_TKPIDX_DH1024]);
            mc->pTmpKeys[SSL_TKPIDX_DH1024] = NULL;
        }
    }
    return;
}

/*
 * Configure a particular server
 */
void ssl_init_ConfigureServer(server_rec *s, pool *p, SSLSrvConfigRec *sc)
{
    SSLModConfigRec *mc = myModConfig();
    int nVerify;
    char *cpVHostID;
    EVP_PKEY *pKey;
    SSL_CTX *ctx;
    STACK_OF(X509_NAME) *skCAList;
    ssl_asn1_t *asn1;
    unsigned char *ucp;
    char *cp;
    BOOL ok;
    BOOL bSkipFirst;
    int isca, pathlen;
    int i, n;

    /*
     * Create the server host:port string because we need it a lot
     */
    cpVHostID = ssl_util_vhostid(p, s);

    /*
     * Now check for important parameters and the
     * possibility that the user forgot to set them.
     */
    if (sc->szPublicCertFile[0] == NULL) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) No SSL Certificate set [hint: SSLCertificateFile]",
                cpVHostID);
        ssl_die();
    }

    /*
     *  Check for problematic re-initializations
     */
    if (sc->pPublicCert[SSL_AIDX_RSA] != NULL ||
        sc->pPublicCert[SSL_AIDX_DSA] != NULL   ) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) Illegal attempt to re-initialise SSL for server "
                "(theoretically shouldn't happen!)", cpVHostID);
        ssl_die();
    }

    /*
     *  Create the new per-server SSL context
     */
    if (sc->nProtocol == SSL_PROTOCOL_NONE) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) No SSL protocols available [hint: SSLProtocol]",
                cpVHostID);
        ssl_die();
    }
    cp = ap_pstrcat(p, (sc->nProtocol & SSL_PROTOCOL_SSLV2 ? "SSLv2, " : ""),
                       (sc->nProtocol & SSL_PROTOCOL_SSLV3 ? "SSLv3, " : ""),
                       (sc->nProtocol & SSL_PROTOCOL_TLSV1 ? "TLSv1, " : ""), NULL);
    cp[strlen(cp)-2] = NUL;
    ssl_log(s, SSL_LOG_TRACE,
            "Init: (%s) Creating new SSL context (protocols: %s)", cpVHostID, cp);
    if (sc->nProtocol == SSL_PROTOCOL_SSLV2)
        ctx = SSL_CTX_new(SSLv2_server_method());  /* only SSLv2 is left */
    else
        ctx = SSL_CTX_new(SSLv23_server_method()); /* be more flexible */
    SSL_CTX_set_options(ctx, SSL_OP_ALL);
    if (!(sc->nProtocol & SSL_PROTOCOL_SSLV2))
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
    if (!(sc->nProtocol & SSL_PROTOCOL_SSLV3))
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
    if (!(sc->nProtocol & SSL_PROTOCOL_TLSV1))
        SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
    SSL_CTX_set_app_data(ctx, s);
    sc->pSSLCtx = ctx;

    /*
     * Configure additional context ingredients
     */
    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
    if (mc->nSessionCacheMode == SSL_SCMODE_UNSET)
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
    else
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);

    /*
     *  Configure callbacks for SSL context
     */
    nVerify = SSL_VERIFY_NONE;
    if (sc->nVerifyClient == SSL_CVERIFY_REQUIRE)
        nVerify |= SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    if (   (sc->nVerifyClient == SSL_CVERIFY_OPTIONAL)
        || (sc->nVerifyClient == SSL_CVERIFY_OPTIONAL_NO_CA) )
        nVerify |= SSL_VERIFY_PEER;
    SSL_CTX_set_verify(ctx, nVerify,  ssl_callback_SSLVerify);
    SSL_CTX_sess_set_new_cb(ctx,      ssl_callback_NewSessionCacheEntry);
    SSL_CTX_sess_set_get_cb(ctx,      ssl_callback_GetSessionCacheEntry);
    SSL_CTX_sess_set_remove_cb(ctx,   ssl_callback_DelSessionCacheEntry);
    SSL_CTX_set_tmp_rsa_callback(ctx, ssl_callback_TmpRSA);
    SSL_CTX_set_tmp_dh_callback(ctx,  ssl_callback_TmpDH);
    SSL_CTX_set_info_callback(ctx,    ssl_callback_LogTracingState);

    /*
     *  Configure SSL Cipher Suite
     */
    if (sc->szCipherSuite != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring permitted SSL ciphers [%s]", 
                cpVHostID, sc->szCipherSuite);
        if (!SSL_CTX_set_cipher_list(ctx, sc->szCipherSuite)) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure permitted SSL ciphers",
                    cpVHostID);
            ssl_die();
        }
    }

    /*
     * Configure Client Authentication details
     */
    if (sc->szCACertificateFile != NULL || sc->szCACertificatePath != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring client authentication", cpVHostID);
        if (!SSL_CTX_load_verify_locations(ctx,
                                           sc->szCACertificateFile,
                                           sc->szCACertificatePath)) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure verify locations "
                    "for client authentication", cpVHostID);
            ssl_die();
        }
        if ((skCAList = ssl_init_FindCAList(s, p, sc->szCACertificateFile,
                                            sc->szCACertificatePath)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR,
                    "Init: (%s) Unable to determine list of available "
                    "CA certificates for client authentication", cpVHostID);
            ssl_die();
        }
        SSL_CTX_set_client_CA_list(sc->pSSLCtx, skCAList);
    }

    /*
     * Configure Certificate Revocation List (CRL) Details
     */
    if (sc->szCARevocationFile != NULL || sc->szCARevocationPath != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring certificate revocation facility", cpVHostID);
        if ((sc->pRevocationStore =
                SSL_X509_STORE_create(sc->szCARevocationFile,
                                      sc->szCARevocationPath)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure X.509 CRL storage "
                    "for certificate revocation", cpVHostID);
            ssl_die();
        }
    }

    /*
     * Give a warning when no CAs were configured but client authentication
     * should take place. This cannot work.
     */
    if (sc->nVerifyClient == SSL_CVERIFY_REQUIRE) {
        skCAList = SSL_CTX_get_client_CA_list(ctx);
        if (sk_X509_NAME_num(skCAList) == 0)
            ssl_log(s, SSL_LOG_WARN,
                    "Init: Ops, you want to request client authentication, "
                    "but no CAs are known for verification!? "
                    "[Hint: SSLCACertificate*]");
    }

    /*
     *  Configure server certificate(s)
     */
    ok = FALSE;
    cp = ap_psprintf(p, "%s:RSA", cpVHostID);
    if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tPublicCert, cp)) != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring RSA server certificate", cpVHostID);
        ucp = asn1->cpData;
        if ((sc->pPublicCert[SSL_AIDX_RSA] = d2i_X509(NULL, &ucp, asn1->nData)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to import RSA server certificate",
                    cpVHostID);
            ssl_die();
        }
        if (SSL_CTX_use_certificate(ctx, sc->pPublicCert[SSL_AIDX_RSA]) <= 0) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure RSA server certificate",
                    cpVHostID);
            ssl_die();
        }
        ok = TRUE;
    }
    cp = ap_psprintf(p, "%s:DSA", cpVHostID);
    if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tPublicCert, cp)) != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring DSA server certificate", cpVHostID);
        ucp = asn1->cpData;
        if ((sc->pPublicCert[SSL_AIDX_DSA] = d2i_X509(NULL, &ucp, asn1->nData)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to import DSA server certificate",
                    cpVHostID);
            ssl_die();
        }
        if (SSL_CTX_use_certificate(ctx, sc->pPublicCert[SSL_AIDX_DSA]) <= 0) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure DSA server certificate",
                    cpVHostID);
            ssl_die();
        }
        ok = TRUE;
    }
    if (!ok) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) Ops, no RSA or DSA server certificate found?!", cpVHostID);
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) You have to perform a *full* server restart when you added or removed a certificate and/or key file", cpVHostID);
        ssl_die();
    }

    /*
     * Some information about the certificate(s)
     */
    for (i = 0; i < SSL_AIDX_MAX; i++) {
        if (sc->pPublicCert[i] != NULL) {
            if (SSL_X509_isSGC(sc->pPublicCert[i])) {
                ssl_log(s, SSL_LOG_INFO,
                        "Init: (%s) %s server certificate enables "
                        "Server Gated Cryptography (SGC)", 
                        cpVHostID, (i == SSL_AIDX_RSA ? "RSA" : "DSA"));
            }
            if (SSL_X509_getBC(sc->pPublicCert[i], &isca, &pathlen)) {
                if (isca)
                    ssl_log(s, SSL_LOG_WARN,
                        "Init: (%s) %s server certificate is a CA certificate "
                        "(BasicConstraints: CA == TRUE !?)",
                        cpVHostID, (i == SSL_AIDX_RSA ? "RSA" : "DSA"));
                if (pathlen > 0)
                    ssl_log(s, SSL_LOG_WARN,
                        "Init: (%s) %s server certificate is not a leaf certificate "
                        "(BasicConstraints: pathlen == %d > 0 !?)",
                        cpVHostID, (i == SSL_AIDX_RSA ? "RSA" : "DSA"), pathlen);
            }
            if (SSL_X509_getCN(p, sc->pPublicCert[i], &cp)) {
                if (ap_is_fnmatch(cp) &&
                    !ap_fnmatch(cp, s->server_hostname, FNM_PERIOD|FNM_CASE_BLIND)) {
                    ssl_log(s, SSL_LOG_WARN,
                        "Init: (%s) %s server certificate wildcard CommonName (CN) `%s' "
                        "does NOT match server name!?", cpVHostID, 
                        (i == SSL_AIDX_RSA ? "RSA" : "DSA"), cp);
                }
                else if (strNE(s->server_hostname, cp)) {
                    ssl_log(s, SSL_LOG_WARN,
                        "Init: (%s) %s server certificate CommonName (CN) `%s' "
                        "does NOT match server name!?", cpVHostID, 
                        (i == SSL_AIDX_RSA ? "RSA" : "DSA"), cp);
                }
            }
        }
    }

    /*
     *  Configure server private key(s)
     */
    ok = FALSE;
    cp = ap_psprintf(p, "%s:RSA", cpVHostID);
    if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tPrivateKey, cp)) != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring RSA server private key", cpVHostID);
        ucp = asn1->cpData;
        if ((sc->pPrivateKey[SSL_AIDX_RSA] = 
             d2i_PrivateKey(EVP_PKEY_RSA, NULL, &ucp, asn1->nData)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to import RSA server private key",
                    cpVHostID);
            ssl_die();
        }
        if (SSL_CTX_use_PrivateKey(ctx, sc->pPrivateKey[SSL_AIDX_RSA]) <= 0) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure RSA server private key",
                    cpVHostID);
            ssl_die();
        }
        ok = TRUE;
    }
    cp = ap_psprintf(p, "%s:DSA", cpVHostID);
    if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tPrivateKey, cp)) != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring DSA server private key", cpVHostID);
        ucp = asn1->cpData;
        if ((sc->pPrivateKey[SSL_AIDX_DSA] = 
             d2i_PrivateKey(EVP_PKEY_DSA, NULL, &ucp, asn1->nData)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to import DSA server private key",
                    cpVHostID);
            ssl_die();
        }
        if (SSL_CTX_use_PrivateKey(ctx, sc->pPrivateKey[SSL_AIDX_DSA]) <= 0) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure DSA server private key",
                    cpVHostID);
            ssl_die();
        }
        ok = TRUE;
    }
    if (!ok) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) Ops, no RSA or DSA server private key found?!", cpVHostID);
        ssl_die();
    }

    /*
     * Optionally copy DSA parameters for certificate from private key
     * (see http://www.psy.uq.edu.au/~ftp/Crypto/ssleay/TODO.html)
     */
    if (   sc->pPublicCert[SSL_AIDX_DSA] != NULL
        && sc->pPrivateKey[SSL_AIDX_DSA] != NULL) {
        pKey = X509_get_pubkey(sc->pPublicCert[SSL_AIDX_DSA]);
        if (   pKey != NULL
            && EVP_PKEY_type(pKey->type) == EVP_PKEY_DSA 
            && EVP_PKEY_missing_parameters(pKey))
            EVP_PKEY_copy_parameters(pKey, sc->pPrivateKey[SSL_AIDX_DSA]);
    }

    /* 
     * Optionally configure extra server certificate chain certificates.
     * This is usually done by OpenSSL automatically when one of the
     * server cert issuers are found under SSLCACertificatePath or in
     * SSLCACertificateFile. But because these are intended for client
     * authentication it can conflict. For instance when you use a
     * Global ID server certificate you've to send out the intermediate
     * CA certificate, too. When you would just configure this with
     * SSLCACertificateFile and also use client authentication mod_ssl
     * would accept all clients also issued by this CA. Obviously this
     * isn't what we want in this situation. So this feature here exists
     * to allow one to explicity configure CA certificates which are
     * used only for the server certificate chain.
     */
    if (sc->szCertificateChain != NULL) {
        bSkipFirst = FALSE;
        for (i = 0; i < SSL_AIDX_MAX && sc->szPublicCertFile[i] != NULL; i++) {
            if (strEQ(sc->szPublicCertFile[i], sc->szCertificateChain)) {
                bSkipFirst = TRUE;
                break;
            }
        }
        if ((n = SSL_CTX_use_certificate_chain(ctx, sc->szCertificateChain, 
                                               bSkipFirst, NULL)) < 0) {
            ssl_log(s, SSL_LOG_ERROR,
                    "Init: (%s) Failed to configure CA certificate chain!", cpVHostID);
            ssl_die();
        }
        ssl_log(s, SSL_LOG_TRACE, "Init: (%s) Configuring "
                "server certificate chain (%d CA certificate%s)", cpVHostID,
                n, n == 1 ? "" : "s");
    }

    return;
}

void ssl_init_CheckServers(server_rec *sm, pool *p)
{
    server_rec *s;
    server_rec **ps;
    SSLSrvConfigRec *sc;
    ssl_ds_table *t;
    pool *sp;
    char *key;
    BOOL bConflict;

    /*
     * Give out warnings when a server has HTTPS configured 
     * for the HTTP port or vice versa
     */
    for (s = sm; s != NULL; s = s->next) {
        sc = mySrvConfig(s);
        if (sc->bEnabled && s->port == DEFAULT_HTTP_PORT)
            ssl_log(sm, SSL_LOG_WARN,
                    "Init: (%s) You configured HTTPS(%d) on the standard HTTP(%d) port!",
                    ssl_util_vhostid(p, s), DEFAULT_HTTPS_PORT, DEFAULT_HTTP_PORT);
        if (!sc->bEnabled && s->port == DEFAULT_HTTPS_PORT)
            ssl_log(sm, SSL_LOG_WARN,
                    "Init: (%s) You configured HTTP(%d) on the standard HTTPS(%d) port!",
                    ssl_util_vhostid(p, s), DEFAULT_HTTP_PORT, DEFAULT_HTTPS_PORT);
    }

    /*
     * Give out warnings when more than one SSL-aware virtual server uses the
     * same IP:port. This doesn't work because mod_ssl then will always use
     * just the certificate/keys of one virtual host (which one cannot be said
     * easily - but that doesn't matter here).
     */
    sp = ap_make_sub_pool(p);
    t = ssl_ds_table_make(sp, sizeof(server_rec *));
    bConflict = FALSE;
    for (s = sm; s != NULL; s = s->next) {
        sc = mySrvConfig(s);
        if (!sc->bEnabled)
            continue;
        key = ap_psprintf(sp, "%pA:%u", &s->addrs->host_addr, s->addrs->host_port);
        ps = ssl_ds_table_get(t, key);
        if (ps != NULL) {
            ssl_log(sm, SSL_LOG_WARN,
                    "Init: SSL server IP/port conflict: %s (%s:%d) vs. %s (%s:%d)",
                    ssl_util_vhostid(p, s), 
                    (s->defn_name != NULL ? s->defn_name : "unknown"),
                    s->defn_line_number,
                    ssl_util_vhostid(p, *ps),
                    ((*ps)->defn_name != NULL ? (*ps)->defn_name : "unknown"), 
                    (*ps)->defn_line_number);
            bConflict = TRUE;
            continue;
        }
        ps = ssl_ds_table_push(t, key);
        *ps = s;
    }
    ssl_ds_table_kill(t);
    ap_destroy_pool(sp);
    if (bConflict)
        ssl_log(sm, SSL_LOG_WARN,
                "Init: You should not use name-based virtual hosts in conjunction with SSL!!");

    return;
}

static int ssl_init_FindCAList_X509NameCmp(X509_NAME **a, X509_NAME **b)
{
    return(X509_NAME_cmp(*a, *b));
}

STACK_OF(X509_NAME) *ssl_init_FindCAList(server_rec *s, pool *pp, char *cpCAfile, char *cpCApath)
{
    STACK_OF(X509_NAME) *skCAList;
    STACK_OF(X509_NAME) *sk;
    DIR *dir;
    struct DIR_TYPE *direntry;
    char *cp;
    pool *p;
    int n;

    /*
     * Use a subpool so we don't bloat up the server pool which
     * is remains in memory for the complete operation time of
     * the server.
     */
    p = ap_make_sub_pool(pp);

    /*
     * Start with a empty stack/list where new
     * entries get added in sorted order.
     */
    skCAList = sk_X509_NAME_new(ssl_init_FindCAList_X509NameCmp);

    /*
     * Process CA certificate bundle file
     */
    if (cpCAfile != NULL) {
        sk = SSL_load_client_CA_file(cpCAfile);
        for(n = 0; sk != NULL && n < sk_X509_NAME_num(sk); n++) {
            ssl_log(s, SSL_LOG_TRACE,
                    "CA certificate: %s",
                    X509_NAME_oneline(sk_X509_NAME_value(sk, n), NULL, 0));
            if (sk_X509_NAME_find(skCAList, sk_X509_NAME_value(sk, n)) < 0)
                sk_X509_NAME_push(skCAList, sk_X509_NAME_value(sk, n));
        }
    }

    /*
     * Process CA certificate path files
     */
    if (cpCApath != NULL) {
        dir = ap_popendir(p, cpCApath);
        while ((direntry = readdir(dir)) != NULL) {
            cp = ap_pstrcat(p, cpCApath, "/", direntry->d_name, NULL);
            sk = SSL_load_client_CA_file(cp);
            for(n = 0; sk != NULL && n < sk_X509_NAME_num(sk); n++) {
                ssl_log(s, SSL_LOG_TRACE,
                        "CA certificate: %s",
                        X509_NAME_oneline(sk_X509_NAME_value(sk, n), NULL, 0));
                if (sk_X509_NAME_find(skCAList, sk_X509_NAME_value(sk, n)) < 0)
                    sk_X509_NAME_push(skCAList, sk_X509_NAME_value(sk, n));
            }
        }
        ap_pclosedir(p, dir);
    }

    /*
     * Cleanup
     */
    sk_X509_NAME_set_cmp_func(skCAList, NULL);
    ap_destroy_pool(p);

    return skCAList;
}

void ssl_init_Child(server_rec *s, pool *p)
{
     /* open the mutex lockfile */
     ssl_mutex_reinit(s, p);
     return;
}

void ssl_init_ChildKill(void *data)
{
    /* currently nothing to do */
    return;
}

void ssl_init_ModuleKill(void *data)
{
    SSLSrvConfigRec *sc;
    server_rec *s = (server_rec *)data;

    /*
     * Drop the session cache and mutex
     */
    ssl_scache_kill(s);
    ssl_mutex_kill(s);

    /* 
     * Destroy the temporary keys and params
     */
    ssl_init_TmpKeysHandle(SSL_TKP_FREE, s, NULL);

    /*
     * Free the non-pool allocated structures
     * in the per-server configurations
     */
    for (; s != NULL; s = s->next) {
        sc = mySrvConfig(s);
        if (sc->pPublicCert[SSL_AIDX_RSA] != NULL) {
            X509_free(sc->pPublicCert[SSL_AIDX_RSA]);
            sc->pPublicCert[SSL_AIDX_RSA] = NULL;
        }
        if (sc->pPublicCert[SSL_AIDX_DSA] != NULL) {
            X509_free(sc->pPublicCert[SSL_AIDX_DSA]);
            sc->pPublicCert[SSL_AIDX_DSA] = NULL;
        }
        if (sc->pPrivateKey[SSL_AIDX_RSA] != NULL) {
            EVP_PKEY_free(sc->pPrivateKey[SSL_AIDX_RSA]);
            sc->pPrivateKey[SSL_AIDX_RSA] = NULL;
        }
        if (sc->pPrivateKey[SSL_AIDX_DSA] != NULL) {
            EVP_PKEY_free(sc->pPrivateKey[SSL_AIDX_DSA]);
            sc->pPrivateKey[SSL_AIDX_DSA] = NULL;
        }
        if (sc->pSSLCtx != NULL) {
            SSL_CTX_free(sc->pSSLCtx);
            sc->pSSLCtx = NULL;
        }
    }
    return;
}

