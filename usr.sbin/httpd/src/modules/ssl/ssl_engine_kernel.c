/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |  mod_ssl
** | '_ ` _ \ / _ \ / _` |   / __/ __| |  Apache Interface to OpenSSL
** | | | | | | (_) | (_| |   \__ \__ \ |  www.modssl.org
** |_| |_| |_|\___/ \__,_|___|___/___/_|  ftp.modssl.org
**                      |_____|
**  ssl_engine_kernel.c
**  The SSL engine kernel
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
                             /* ``It took me fifteen years to discover
                                  I had no talent for programming, but
                                  I couldn't give it up because by that
                                  time I was too famous.''
                                            -- Unknown                */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  SSL Engine Kernel
**  _________________________________________________________________
*/

/*
 *  Connect Handler:
 *  Connect SSL to the accepted socket
 *
 *  Usually we would need an Apache API hook which is triggered right after
 *  the socket is accepted for handling a new request. But Apache 1.3 doesn't
 *  provide such a hook, so we have to patch http_main.c and call this
 *  function directly.
 */
void ssl_hook_NewConnection(conn_rec *conn)
{
    server_rec *srvr;
    BUFF *fb;
    SSLSrvConfigRec *sc;
    ap_ctx *apctx;
    SSL *ssl;
    char *cp;
    char *cpVHostID;
    char *cpVHostMD5;
    X509 *xs;
    int rc;

    /*
     * Get context
     */
    srvr = conn->server;
    fb   = conn->client;
    sc   = mySrvConfig(srvr);

    /*
     * Create SSL context
     */
    ap_ctx_set(fb->ctx, "ssl", NULL);

    /*
     * Immediately stop processing if SSL
     * is disabled for this connection
     */
    if (sc == NULL || !sc->bEnabled)
        return;

    /*
     * Remember the connection information for
     * later access inside callback functions
     */
    cpVHostID = ssl_util_vhostid(conn->pool, srvr);
    ssl_log(srvr, SSL_LOG_INFO, "Connection to child %d established "
            "(server %s, client %s)", conn->child_num, cpVHostID, 
            conn->remote_ip != NULL ? conn->remote_ip : "unknown");

    /*
     * Seed the Pseudo Random Number Generator (PRNG)
     */
    ssl_rand_seed(srvr, conn->pool, SSL_RSCTX_CONNECT, "");

    /*
     * Create a new SSL connection with the configured server SSL context and
     * attach this to the socket. Additionally we register this attachment
     * so we can detach later.
     */
    if ((ssl = SSL_new(sc->pSSLCtx)) == NULL) {
        ssl_log(conn->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                "Unable to create a new SSL connection from the SSL context");
        ap_ctx_set(fb->ctx, "ssl", NULL);
        ap_bsetflag(fb, B_EOF|B_EOUT, 1);
        conn->aborted = 1;
        return;
    }
    SSL_clear(ssl);
    cpVHostMD5 = ap_md5(conn->pool, (unsigned char *)cpVHostID);
    if (!SSL_set_session_id_context(ssl, (unsigned char *)cpVHostMD5, strlen(cpVHostMD5))) {
        ssl_log(conn->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                "Unable to set session id context to `%s'", cpVHostMD5);
        ap_ctx_set(fb->ctx, "ssl", NULL);
        ap_bsetflag(fb, B_EOF|B_EOUT, 1);
        conn->aborted = 1;
        return;
    }
    SSL_set_app_data(ssl, conn);
    apctx = ap_ctx_new(conn->pool);
    ap_ctx_set(apctx, "ssl::request_rec", NULL);
    ap_ctx_set(apctx, "ssl::verify::depth", AP_CTX_NUM2PTR(0));
    SSL_set_app_data2(ssl, apctx);
    SSL_set_fd(ssl, fb->fd);
    ap_ctx_set(fb->ctx, "ssl", ssl);

    /*
     *  Configure callbacks for SSL connection
     */
    SSL_set_tmp_rsa_callback(ssl, ssl_callback_TmpRSA);
    SSL_set_tmp_dh_callback(ssl,  ssl_callback_TmpDH);
    if (sc->nLogLevel >= SSL_LOG_DEBUG) {
        BIO_set_callback(SSL_get_rbio(ssl), ssl_io_data_cb);
        BIO_set_callback_arg(SSL_get_rbio(ssl), ssl);
    }

    /*
     * Predefine some client verification results
     */
    ap_ctx_set(fb->ctx, "ssl::client::dn", NULL);
    ap_ctx_set(fb->ctx, "ssl::verify::error", NULL);
    ap_ctx_set(fb->ctx, "ssl::verify::info", NULL);
    SSL_set_verify_result(ssl, X509_V_OK);

    /*
     * We have to manage a I/O timeout ourself, because Apache
     * does it the first time when reading the request, but we're
     * working some time before this happens.
     */
    ap_ctx_set(ap_global_ctx, "ssl::handshake::timeout", (void *)FALSE);
    ap_set_callback_and_alarm(ssl_hook_TimeoutConnection, srvr->timeout);

    /*
     * Now enter the SSL Handshake Phase
     */
    while (!SSL_is_init_finished(ssl)) {

        if ((rc = SSL_accept(ssl)) <= 0) {

            if (SSL_get_error(ssl, rc) == SSL_ERROR_ZERO_RETURN) {
                /*
                 * The case where the connection was closed before any data
                 * was transferred. That's not a real error and can occur
                 * sporadically with some clients.
                 */
                ssl_log(srvr, SSL_LOG_INFO,
                        "SSL handshake stopped: connection was closed");
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                SSL_smart_shutdown(ssl);
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
            else if (ERR_GET_REASON(ERR_peek_error()) == SSL_R_HTTP_REQUEST) {
                /*
                 * The case where OpenSSL has recognized a HTTP request:
                 * This means the client speaks plain HTTP on our HTTPS
                 * port. Hmmmm...  At least for this error we can be more friendly
                 * and try to provide him with a HTML error page. We have only one
                 * problem: OpenSSL has already read some bytes from the HTTP
                 * request. So we have to skip the request line manually and
                 * instead provide a faked one in order to continue the internal
                 * Apache processing.
                 *
                 */
                char ca[2];
                int rv;

                /* log the situation */
                ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                        "SSL handshake failed: HTTP spoken on HTTPS port; "
                        "trying to send HTML error page");

                /* first: skip the remaining bytes of the request line */
                do {
                    do {
                        rv = read(fb->fd, ca, 1);
                    } while (rv == -1 && errno == EINTR);
                } while (rv > 0 && ca[0] != '\012' /*LF*/);

                /* second: fake the request line */
                fb->inbase = ap_palloc(fb->pool, fb->bufsiz);
                ap_cpystrn((char *)fb->inbase, "GET /mod_ssl:error:HTTP-request HTTP/1.0\r\n",
                           fb->bufsiz);
                fb->inptr = fb->inbase;
                fb->incnt = strlen((char *)fb->inptr);

                /* third: kick away the SSL stuff */
                SSL_set_shutdown(ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
                SSL_smart_shutdown(ssl);
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);

                /* finally: let Apache go on with processing */
                return;
            }
            else if (ap_ctx_get(ap_global_ctx, "ssl::handshake::timeout") == (void *)TRUE) {
                ssl_log(srvr, SSL_LOG_ERROR,
                        "SSL handshake timed out (client %s, server %s)",
                        conn->remote_ip != NULL ? conn->remote_ip : "unknown", cpVHostID);
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                SSL_smart_shutdown(ssl);
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
            else if (SSL_get_error(ssl, rc) == SSL_ERROR_SYSCALL) {
                if (errno == EINTR)
                    continue;
                if (errno > 0)
                    ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_SSLERR|SSL_ADD_ERRNO,
                            "SSL handshake interrupted by system "
                            "[Hint: Stop button pressed in browser?!]");
                else
                    ssl_log(srvr, SSL_LOG_INFO|SSL_ADD_SSLERR|SSL_ADD_ERRNO,
                            "Spurious SSL handshake interrupt"
                            "[Hint: Usually just one of those OpenSSL confusions!?]");
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                SSL_smart_shutdown(ssl);
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
            else {
                /*
                 * Ok, anything else is a fatal error
                 */
                ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_SSLERR|SSL_ADD_ERRNO,
                        "SSL handshake failed (server %s, client %s)", cpVHostID, 
                        conn->remote_ip != NULL ? conn->remote_ip : "unknown");

                /*
                 * try to gracefully shutdown the connection:
                 * - send an own shutdown message (be gracefully)
                 * - don't wait for peer's shutdown message (deadloop)
                 * - kick away the SSL stuff immediately
                 * - block the socket, so Apache cannot operate any more
                 */
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                SSL_smart_shutdown(ssl);
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
        }

        /*
         * Check for failed client authentication
         */
        if (   SSL_get_verify_result(ssl) != X509_V_OK
            || ap_ctx_get(fb->ctx, "ssl::verify::error") != NULL) {
            cp = (char *)ap_ctx_get(fb->ctx, "ssl::verify::error");
            ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "SSL client authentication failed: %s", 
                    cp != NULL ? cp : "unknown reason");
            SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
            SSL_smart_shutdown(ssl);
            SSL_free(ssl);
            ap_ctx_set(fb->ctx, "ssl", NULL);
            ap_bsetflag(fb, B_EOF|B_EOUT, 1);
            conn->aborted = 1;
            return;
        }

        /*
         * Remember the peer certificate's DN
         */
        if ((xs = SSL_get_peer_certificate(ssl)) != NULL) {
            cp = X509_NAME_oneline(X509_get_subject_name(xs), NULL, 0);
            ap_ctx_set(fb->ctx, "ssl::client::dn", ap_pstrdup(conn->pool, cp));
            free(cp);
        }

        /*
         * Make really sure that when a peer certificate
         * is required we really got one... (be paranoid)
         */
        if (   sc->nVerifyClient == SSL_CVERIFY_REQUIRE
            && ap_ctx_get(fb->ctx, "ssl::client::dn") == NULL) {
            ssl_log(srvr, SSL_LOG_ERROR,
                    "No acceptable peer certificate available");
            SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
            SSL_smart_shutdown(ssl);
            SSL_free(ssl);
            ap_ctx_set(fb->ctx, "ssl", NULL);
            ap_bsetflag(fb, B_EOF|B_EOUT, 1);
            conn->aborted = 1;
            return;
        }
    }

    /*
     * Remove the timeout handling
     */
    ap_set_callback_and_alarm(NULL, 0);
    ap_ctx_set(ap_global_ctx, "ssl::handshake::timeout", (void *)FALSE);

    /*
     * Improve I/O throughput by using
     * OpenSSL's read-ahead functionality
     * (don't used under Win32, because
     * there we use select())
     */
#ifndef WIN32
    SSL_set_read_ahead(ssl, TRUE);
#endif

#ifdef SSL_VENDOR
    /* Allow vendors to do more things on connection time... */
    ap_hook_use("ap::mod_ssl::vendor::new_connection",
                AP_HOOK_SIG2(void,ptr), AP_HOOK_ALL, conn);
#endif

    return;
}

/*
 * Signal handler function for the SSL handshake phase
 */
void ssl_hook_TimeoutConnection(int sig)
{
    /* we just set a flag for the handshake processing loop */
    ap_ctx_set(ap_global_ctx, "ssl::handshake::timeout", (void *)TRUE);
    return;
}

/*
 *  Close the SSL part of the socket connection
 *  (called immediately _before_ the socket is closed)
 */
void ssl_hook_CloseConnection(conn_rec *conn)
{
    SSL *ssl;
    char *cpType;

    ssl = ap_ctx_get(conn->client->ctx, "ssl");
    if (ssl == NULL)
        return;

    /*
     * First make sure that no more data is pending in Apache's BUFF,
     * because when it's (implicitly) flushed later by the ap_bclose()
     * calls of Apache it would lead to an I/O error in the browser due
     * to the fact that the SSL layer was already removed by us.
     */
    ap_bflush(conn->client);

    /*
     * Now close the SSL layer of the connection. We've to take
     * the TLSv1 standard into account here:
     *
     * | 7.2.1. Closure alerts
     * |
     * | The client and the server must share knowledge that the connection is
     * | ending in order to avoid a truncation attack. Either party may
     * | initiate the exchange of closing messages.
     * |
     * | close_notify
     * |     This message notifies the recipient that the sender will not send
     * |     any more messages on this connection. The session becomes
     * |     unresumable if any connection is terminated without proper
     * |     close_notify messages with level equal to warning.
     * |
     * | Either party may initiate a close by sending a close_notify alert.
     * | Any data received after a closure alert is ignored.
     * |
     * | Each party is required to send a close_notify alert before closing
     * | the write side of the connection. It is required that the other party
     * | respond with a close_notify alert of its own and close down the
     * | connection immediately, discarding any pending writes. It is not
     * | required for the initiator of the close to wait for the responding
     * | close_notify alert before closing the read side of the connection.
     *
     * This means we've to send a close notify message, but haven't to wait
     * for the close notify of the client. Actually we cannot wait for the
     * close notify of the client because some clients (including Netscape
     * 4.x) don't send one, so we would hang.
     */

    /*
     * exchange close notify messages, but allow the user
     * to force the type of handshake via SetEnvIf directive
     */
    if (ap_ctx_get(conn->client->ctx, "ssl::flag::unclean-shutdown") == PTRUE) {
        /* perform no close notify handshake at all
           (violates the SSL/TLS standard!) */
        SSL_set_shutdown(ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
        cpType = "unclean";
    }
    else if (ap_ctx_get(conn->client->ctx, "ssl::flag::accurate-shutdown") == PTRUE) {
        /* send close notify and wait for clients close notify
           (standard compliant, but usually causes connection hangs) */
        SSL_set_shutdown(ssl, 0);
        cpType = "accurate";
    }
    else {
        /* send close notify, but don't wait for clients close notify
           (standard compliant and safe, so it's the DEFAULT!) */
        SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
        cpType = "standard";
    }
    SSL_smart_shutdown(ssl);

    /* deallocate the SSL connection */
    SSL_free(ssl);
    ap_ctx_set(conn->client->ctx, "ssl", NULL);

    /* and finally log the fact that we've closed the connection */
    ssl_log(conn->server, SSL_LOG_INFO,
            "Connection to child %d closed with %s shutdown (server %s, client %s)",
            conn->child_num, cpType, ssl_util_vhostid(conn->pool, conn->server),
            conn->remote_ip != NULL ? conn->remote_ip : "unknown");
    return;
}

/*
 *  Post Read Request Handler
 */
int ssl_hook_ReadReq(request_rec *r)
{
    SSL *ssl;
    ap_ctx *apctx;

    /*
     * Get the SSL connection structure and perform the
     * delayed interlinking from SSL back to request_rec
     */
    ssl = ap_ctx_get(r->connection->client->ctx, "ssl");
    if (ssl != NULL) {
        apctx = SSL_get_app_data2(ssl);
        ap_ctx_set(apctx, "ssl::request_rec", r);
    }

    /*
     * Force the mod_ssl content handler when URL indicates this
     */
    if (strEQn(r->uri, "/mod_ssl:", 9))
        r->handler = "mod_ssl:content-handler";
    if (ssl != NULL) {
        ap_ctx_set(r->ctx, "ap::http::method",  "https");
        ap_ctx_set(r->ctx, "ap::default::port", "443");
    }
    else {
        ap_ctx_set(r->ctx, "ap::http::method",  NULL);
        ap_ctx_set(r->ctx, "ap::default::port", NULL);
    }
    return DECLINED;
}

/*
 *  URL Translation Handler
 */
int ssl_hook_Translate(request_rec *r)
{
    if (ap_ctx_get(r->connection->client->ctx, "ssl") == NULL)
        return DECLINED;

    /*
     * Log information about incoming HTTPS requests
     */
    if (ap_is_initial_req(r))
        ssl_log(r->server, SSL_LOG_INFO,
                "%s HTTPS request received for child %d (server %s)",
                r->connection->keepalives <= 0 ?
                    "Initial (No.1)" :
                    ap_psprintf(r->pool, "Subsequent (No.%d)",
                                r->connection->keepalives+1),
                r->connection->child_num,
                ssl_util_vhostid(r->pool, r->server));

    /*
     * Move SetEnvIf information from request_rec to conn_rec/BUFF
     * to allow the close connection handler to use them.
     */
    if (ap_table_get(r->subprocess_env, "ssl-unclean-shutdown") != NULL)
        ap_ctx_set(r->connection->client->ctx, "ssl::flag::unclean-shutdown", PTRUE);
    else
        ap_ctx_set(r->connection->client->ctx, "ssl::flag::unclean-shutdown", PFALSE);
    if (ap_table_get(r->subprocess_env, "ssl-accurate-shutdown") != NULL)
        ap_ctx_set(r->connection->client->ctx, "ssl::flag::accurate-shutdown", PTRUE);
    else
        ap_ctx_set(r->connection->client->ctx, "ssl::flag::accurate-shutdown", PFALSE);

    return DECLINED;
}

/*
 *  Content Handler
 */
int ssl_hook_Handler(request_rec *r)
{
    int port;
    char *thisport;
    char *thisurl;

    if (strNEn(r->uri, "/mod_ssl:", 9))
        return DECLINED;

    if (strEQ(r->uri, "/mod_ssl:error:HTTP-request")) {
        thisport = "";
        port = ap_get_server_port(r);
        if (!ap_is_default_port(port, r))
            thisport = ap_psprintf(r->pool, ":%u", port);
        thisurl = ap_psprintf(r->pool, "https://%s%s/",
                              ap_get_server_name(r), thisport);

        ap_table_setn(r->notes, "error-notes", ap_psprintf(r->pool,
                      "Reason: You're speaking plain HTTP to an SSL-enabled server port.<BR>\n"
                      "Instead use the HTTPS scheme to access this URL, please.<BR>\n"
                      "<BLOCKQUOTE>Hint: <A HREF=\"%s\"><B>%s</B></A></BLOCKQUOTE>",
                      thisurl, thisurl));
    }

    return HTTP_BAD_REQUEST;
}

/*
 *  Access Handler
 */
int ssl_hook_Access(request_rec *r)
{
    SSLDirConfigRec *dc;
    SSLSrvConfigRec *sc;
    SSL *ssl;
    SSL_CTX *ctx = NULL;
    array_header *apRequirement;
    ssl_require_t *pRequirements;
    ssl_require_t *pRequirement;
    char *cp;
    int ok;
    int i;
    BOOL renegotiate;
    BOOL renegotiate_quick;
#ifdef SSL_EXPERIMENTAL_PERDIRCA
    BOOL reconfigured_locations;
    STACK_OF(X509_NAME) *skCAList;
    char *cpCAPath;
    char *cpCAFile;
#endif
    X509 *cert;
    STACK_OF(X509) *certstack;
    X509_STORE *certstore;
    X509_STORE_CTX certstorectx;
    int depth;
    STACK_OF(SSL_CIPHER) *skCipherOld;
    STACK_OF(SSL_CIPHER) *skCipher;
    SSL_CIPHER *pCipher;
    ap_ctx *apctx;
    int nVerifyOld;
    int nVerify;
    int n;
    void *vp;
    int rc;

    dc  = myDirConfig(r);
    sc  = mySrvConfig(r->server);
    ssl = ap_ctx_get(r->connection->client->ctx, "ssl");
    if (ssl != NULL)
        ctx = SSL_get_SSL_CTX(ssl);

    /*
     * Support for SSLRequireSSL directive
     */
    if (dc->bSSLRequired && ssl == NULL) {
        ap_log_reason("SSL connection required", r->filename, r);
        /* remember forbidden access for strict require option */
        ap_table_setn(r->notes, "ssl-access-forbidden", (void *)1);
        return FORBIDDEN;
    }

    /*
     * Check to see if SSL protocol is on
     */
    if (!sc->bEnabled)
        return DECLINED;
    if (ssl == NULL)
        return DECLINED;

    /*
     * Support for per-directory reconfigured SSL connection parameters.
     *
     * This is implemented by forcing an SSL renegotiation with the
     * reconfigured parameter suite. But Apache's internal API processing
     * makes our life very hard here, because when internal sub-requests occur
     * we nevertheless should avoid multiple unnecessary SSL handshakes (they
     * require extra network I/O and especially time to perform). 
     * 
     * But the optimization for filtering out the unnecessary handshakes isn't
     * obvious and trivial.  Especially because while Apache is in its
     * sub-request processing the client could force additional handshakes,
     * too. And these take place perhaps without our notice. So the only
     * possibility is to explicitly _ask_ OpenSSL whether the renegotiation
     * has to be performed or not. It has to performed when some parameters
     * which were previously known (by us) are not those we've now
     * reconfigured (as known by OpenSSL) or (in optimized way) at least when
     * the reconfigured parameter suite is stronger (more restrictions) than
     * the currently active one.
     */
    renegotiate            = FALSE;
    renegotiate_quick      = FALSE;
#ifdef SSL_EXPERIMENTAL_PERDIRCA
    reconfigured_locations = FALSE;
#endif

    /*
     * Override of SSLCipherSuite
     *
     * We provide two options here:
     *
     * o The paranoid and default approach where we force a renegotiation when
     *   the cipher suite changed in _any_ way (which is straight-forward but
     *   often forces renegotiations too often and is perhaps not what the
     *   user actually wanted).
     *
     * o The optimized and still secure way where we force a renegotiation
     *   only if the currently active cipher is no longer contained in the
     *   reconfigured/new cipher suite. Any other changes are not important
     *   because it's the servers choice to select a cipher from the ones the
     *   client supports. So as long as the current cipher is still in the new
     *   cipher suite we're happy. Because we can assume we would have
     *   selected it again even when other (better) ciphers exists now in the
     *   new cipher suite. This approach is fine because the user explicitly
     *   has to enable this via ``SSLOptions +OptRenegotiate''. So we do no
     *   implicit optimizations.
     */
    if (dc->szCipherSuite != NULL) {
        /* remember old state */
        pCipher = NULL;
        skCipherOld = NULL;
        if (dc->nOptions & SSL_OPT_OPTRENEGOTIATE)
            pCipher = SSL_get_current_cipher(ssl);
        else {
            skCipherOld = SSL_get_ciphers(ssl);
            if (skCipherOld != NULL)
                skCipherOld = sk_SSL_CIPHER_dup(skCipherOld);
        }
        /* configure new state */
        if (!SSL_set_cipher_list(ssl, dc->szCipherSuite)) {
            ssl_log(r->server, SSL_LOG_WARN|SSL_ADD_SSLERR,
                    "Unable to reconfigure (per-directory) permitted SSL ciphers");
            if (skCipherOld != NULL)
                sk_SSL_CIPHER_free(skCipherOld);
            return FORBIDDEN;
        }
        /* determine whether a renegotiation has to be forced */
        skCipher = SSL_get_ciphers(ssl);
        if (dc->nOptions & SSL_OPT_OPTRENEGOTIATE) {
            /* optimized way */
            if ((pCipher == NULL && skCipher != NULL) ||
                (pCipher != NULL && skCipher == NULL)   )
                renegotiate = TRUE;
            else if (pCipher != NULL && skCipher != NULL
                     && sk_SSL_CIPHER_find(skCipher, pCipher) < 0) {
                renegotiate = TRUE;
            }
        }
        else {
            /* paranoid way */
            if ((skCipherOld == NULL && skCipher != NULL) ||
                (skCipherOld != NULL && skCipher == NULL)   )
                renegotiate = TRUE;
            else if (skCipherOld != NULL && skCipher != NULL) {
                for (n = 0; !renegotiate && n < sk_SSL_CIPHER_num(skCipher); n++) {
                    if (sk_SSL_CIPHER_find(skCipherOld, sk_SSL_CIPHER_value(skCipher, n)) < 0)
                        renegotiate = TRUE;
                }
                for (n = 0; !renegotiate && n < sk_SSL_CIPHER_num(skCipherOld); n++) {
                    if (sk_SSL_CIPHER_find(skCipher, sk_SSL_CIPHER_value(skCipherOld, n)) < 0)
                        renegotiate = TRUE;
                }
            }
        }
        /* cleanup */
        if (skCipherOld != NULL)
            sk_SSL_CIPHER_free(skCipherOld);
        /* tracing */
        if (renegotiate)
            ssl_log(r->server, SSL_LOG_TRACE,
                    "Reconfigured cipher suite will force renegotiation");
    }

    /*
     * override of SSLVerifyDepth
     *
     * The depth checks are handled by us manually inside the verify callback
     * function and not by OpenSSL internally (and our function is aware of
     * both the per-server and per-directory contexts). So we cannot ask
     * OpenSSL about the currently verify depth. Instead we remember it in our
     * ap_ctx attached to the SSL* of OpenSSL.  We've to force the
     * renegotiation if the reconfigured/new verify depth is less than the
     * currently active/remembered verify depth (because this means more
     * restriction on the certificate chain).
     */
    if (dc->nVerifyDepth != UNSET) {
        apctx = SSL_get_app_data2(ssl);
        if ((vp = ap_ctx_get(apctx, "ssl::verify::depth")) != NULL)
            n = (int)AP_CTX_PTR2NUM(vp);
        else
            n = sc->nVerifyDepth;
        ap_ctx_set(apctx, "ssl::verify::depth",
                   AP_CTX_NUM2PTR(dc->nVerifyDepth));
        /* determine whether a renegotiation has to be forced */
        if (dc->nVerifyDepth < n) {
            renegotiate = TRUE;
            ssl_log(r->server, SSL_LOG_TRACE,
                    "Reduced client verification depth will force renegotiation");
        }
    }

    /*
     * override of SSLVerifyClient
     *
     * We force a renegotiation if the reconfigured/new verify type is
     * stronger than the currently active verify type. 
     *
     * The order is: none << optional_no_ca << optional << require
     *
     * Additionally the following optimization is possible here: When the
     * currently active verify type is "none" but a client certificate is
     * already known/present, it's enough to manually force a client
     * verification but at least skip the I/O-intensive renegotation
     * handshake.
     */
    if (dc->nVerifyClient != SSL_CVERIFY_UNSET) {
        /* remember old state */
        nVerifyOld = SSL_get_verify_mode(ssl);
        /* configure new state */
        nVerify = SSL_VERIFY_NONE;
        if (dc->nVerifyClient == SSL_CVERIFY_REQUIRE)
            nVerify |= SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        if (   (dc->nVerifyClient == SSL_CVERIFY_OPTIONAL)
            || (dc->nVerifyClient == SSL_CVERIFY_OPTIONAL_NO_CA) )
            nVerify |= SSL_VERIFY_PEER;
        SSL_set_verify(ssl, nVerify, ssl_callback_SSLVerify);
        SSL_set_verify_result(ssl, X509_V_OK);
        /* determine whether we've to force a renegotiation */
        if (nVerify != nVerifyOld) {
            if (   (   (nVerifyOld == SSL_VERIFY_NONE)
                    && (nVerify    != SSL_VERIFY_NONE))
                || (  !(nVerifyOld &  SSL_VERIFY_PEER)
                    && (nVerify    &  SSL_VERIFY_PEER))
                || (  !(nVerifyOld &  (SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT))
                    && (nVerify    &  (SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT)))) {
                renegotiate = TRUE;
                /* optimization */
                if (   dc->nOptions & SSL_OPT_OPTRENEGOTIATE
                    && nVerifyOld == SSL_VERIFY_NONE
                    && SSL_get_peer_certificate(ssl) != NULL)
                    renegotiate_quick = TRUE;
                ssl_log(r->server, SSL_LOG_TRACE,
                        "Changed client verification type will force %srenegotiation",
                        renegotiate_quick ? "quick " : "");
             }
        }
    }

    /*
     *  override SSLCACertificateFile & SSLCACertificatePath
     *  This is tagged experimental because it has to use an ugly kludge: We
     *  have to change the locations inside the SSL_CTX* (per-server global)
     *  instead inside SSL* (per-connection local) and reconfigure it to the
     *  old values later. That's problematic at least for the threaded process
     *  model of Apache under Win32 or when an error occurs. But unless
     *  OpenSSL provides a SSL_load_verify_locations() function we've no other
     *  chance to provide this functionality...
     */
#ifdef SSL_EXPERIMENTAL_PERDIRCA
    if (   (   dc->szCACertificateFile != NULL
            && (   sc->szCACertificateFile == NULL
                || (   sc->szCACertificateFile != NULL
                    && strNE(dc->szCACertificateFile, sc->szCACertificateFile))))
        || (   dc->szCACertificatePath != NULL
            && (   sc->szCACertificatePath == NULL
                || (   sc->szCACertificatePath != NULL
                    && strNE(dc->szCACertificatePath, sc->szCACertificatePath)))) ) {
        cpCAFile = dc->szCACertificateFile != NULL ?
                   dc->szCACertificateFile : sc->szCACertificateFile;
        cpCAPath = dc->szCACertificatePath != NULL ?
                   dc->szCACertificatePath : sc->szCACertificatePath;
        /*
           FIXME: This should be...
           if (!SSL_load_verify_locations(ssl, cpCAFile, cpCAPath)) {
           ...but OpenSSL still doesn't provide this!
         */
        if (!SSL_CTX_load_verify_locations(ctx, cpCAFile, cpCAPath)) {
            ssl_log(r->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Unable to reconfigure verify locations "
                    "for client authentication");
            return FORBIDDEN;
        }
        if ((skCAList = ssl_init_FindCAList(r->server, r->pool,
                                            cpCAFile, cpCAPath)) == NULL) {
            ssl_log(r->server, SSL_LOG_ERROR,
                    "Unable to determine list of available "
                    "CA certificates for client authentication");
            return FORBIDDEN;
        }
        SSL_set_client_CA_list(ssl, skCAList);
        renegotiate = TRUE;
        reconfigured_locations = TRUE;
        ssl_log(r->server, SSL_LOG_TRACE,
                "Changed client verification locations will force renegotiation");
    }
#endif /* SSL_EXPERIMENTAL_PERDIRCA */

#ifdef SSL_CONSERVATIVE 
    /* 
     *  SSL renegotiations in conjunction with HTTP
     *  requests using the POST method are not supported.
     */
    if (renegotiate && r->method_number == M_POST) {
        ssl_log(r->server, SSL_LOG_ERROR,
                "SSL Re-negotiation in conjunction with POST method not supported!");
        ssl_log(r->server, SSL_LOG_INFO,
                "You have to compile without -DSSL_CONSERVATIVE to enabled support for this.");
        return METHOD_NOT_ALLOWED;
    }
#endif /* SSL_CONSERVATIVE */

    /*
     * now do the renegotiation if anything was actually reconfigured
     */
    if (renegotiate) {
        /*
         * Now we force the SSL renegotation by sending the Hello Request
         * message to the client. Here we have to do a workaround: Actually
         * OpenSSL returns immediately after sending the Hello Request (the
         * intent AFAIK is because the SSL/TLS protocol says it's not a must
         * that the client replies to a Hello Request). But because we insist
         * on a reply (anything else is an error for us) we have to go to the
         * ACCEPT state manually. Using SSL_set_accept_state() doesn't work
         * here because it resets too much of the connection.  So we set the
         * state explicitly and continue the handshake manually.
         */
        ssl_log(r->server, SSL_LOG_INFO, "Requesting connection re-negotiation");
        if (renegotiate_quick) {
            /* perform just a manual re-verification of the peer */
            ssl_log(r->server, SSL_LOG_TRACE,
                    "Performing quick renegotiation: just re-verifying the peer");
            certstore = SSL_CTX_get_cert_store(ctx);
            if (certstore == NULL) {
                ssl_log(r->server, SSL_LOG_ERROR, "Cannot find certificate storage");
                return FORBIDDEN;
            }
            certstack = SSL_get_peer_cert_chain(ssl);
            if (certstack == NULL || sk_X509_num(certstack) == 0) {
                ssl_log(r->server, SSL_LOG_ERROR, "Cannot find peer certificate chain");
                return FORBIDDEN;
            }
            cert = sk_X509_value(certstack, 0);
            X509_STORE_CTX_init(&certstorectx, certstore, cert, certstack);
            depth = SSL_get_verify_depth(ssl);
            if (depth >= 0)
                X509_STORE_CTX_set_depth(&certstorectx, depth);
            X509_STORE_CTX_set_ex_data(&certstorectx,
                SSL_get_ex_data_X509_STORE_CTX_idx(), (char *)ssl);
            if (!X509_verify_cert(&certstorectx))
                ssl_log(r->server, SSL_LOG_ERROR|SSL_ADD_SSLERR, 
                        "Re-negotiation verification step failed");
            SSL_set_verify_result(ssl, certstorectx.error);
            X509_STORE_CTX_cleanup(&certstorectx);
        }
        else {
            /* do a full renegotiation */
            ssl_log(r->server, SSL_LOG_TRACE,
                    "Performing full renegotiation: complete handshake protocol");
            if (r->main != NULL)
                SSL_set_session_id_context(ssl, (unsigned char *)&(r->main), sizeof(r->main));
            else
                SSL_set_session_id_context(ssl, (unsigned char *)&r, sizeof(r));
#ifndef SSL_CONSERVATIVE
            ssl_io_suck(r, ssl);
#endif
            SSL_renegotiate(ssl);
            SSL_do_handshake(ssl);
            if (SSL_get_state(ssl) != SSL_ST_OK) {
                ssl_log(r->server, SSL_LOG_ERROR, "Re-negotiation request failed");
                return FORBIDDEN;
            }
            ssl_log(r->server, SSL_LOG_INFO, "Awaiting re-negotiation handshake");
            SSL_set_state(ssl, SSL_ST_ACCEPT);
            SSL_do_handshake(ssl);
            if (SSL_get_state(ssl) != SSL_ST_OK) {
                ssl_log(r->server, SSL_LOG_ERROR,
                        "Re-negotiation handshake failed: Not accepted by client!?");
                return FORBIDDEN;
            }
        }

        /*
         * Remember the peer certificate's DN
         */
        if ((cert = SSL_get_peer_certificate(ssl)) != NULL) {
            cp = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
            ap_ctx_set(r->connection->client->ctx, "ssl::client::dn", 
                       ap_pstrdup(r->connection->pool, cp));
            free(cp);
        }

        /*
         * Finally check for acceptable renegotiation results
         */
        if (dc->nVerifyClient != SSL_CVERIFY_NONE) {
            if (   dc->nVerifyClient == SSL_CVERIFY_REQUIRE
                && SSL_get_verify_result(ssl) != X509_V_OK  ) {
                ssl_log(r->server, SSL_LOG_ERROR,
                        "Re-negotiation handshake failed: Client verification failed");
                return FORBIDDEN;
            }
            if (   dc->nVerifyClient == SSL_CVERIFY_REQUIRE
                && SSL_get_peer_certificate(ssl) == NULL   ) {
                ssl_log(r->server, SSL_LOG_ERROR,
                        "Re-negotiation handshake failed: Client certificate missing");
                return FORBIDDEN;
            }
        }
    }

    /*
     * Under old OpenSSL we had to change the X509_STORE inside the
     * SSL_CTX instead inside the SSL structure, so we have to reconfigure it
     * to the old values. This should be changed with forthcoming OpenSSL
     * versions when better functionality is avaiable.
     */
#ifdef SSL_EXPERIMENTAL_PERDIRCA
    if (renegotiate && reconfigured_locations) {
        if (!SSL_CTX_load_verify_locations(ctx,
                sc->szCACertificateFile, sc->szCACertificatePath)) {
            ssl_log(r->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Unable to reconfigure verify locations "
                    "to per-server configuration parameters");
            return FORBIDDEN;
        }
    }
#endif /* SSL_EXPERIMENTAL_PERDIRCA */

    /*
     * Check SSLRequire boolean expressions
     */
    apRequirement = dc->aRequirement;
    pRequirements = (ssl_require_t *)apRequirement->elts;
    for (i = 0; i < apRequirement->nelts; i++) {
        pRequirement = &pRequirements[i];
        ok = ssl_expr_exec(r, pRequirement->mpExpr);
        if (ok < 0) {
            cp = ap_psprintf(r->pool, "Failed to execute SSL requirement expression: %s",
                             ssl_expr_get_error());
            ap_log_reason(cp, r->filename, r);
            /* remember forbidden access for strict require option */
            ap_table_setn(r->notes, "ssl-access-forbidden", (void *)1);
            return FORBIDDEN;
        }
        if (ok != 1) {
            ssl_log(r->server, SSL_LOG_INFO,
                    "Access to %s denied for %s (requirement expression not fulfilled)",
                    r->filename, r->connection->remote_ip);
            ssl_log(r->server, SSL_LOG_INFO,
                    "Failed expression: %s", pRequirement->cpExpr);
            ap_log_reason("SSL requirement expression not fulfilled "
                          "(see SSL logfile for more details)", r->filename, r);
            /* remember forbidden access for strict require option */
            ap_table_setn(r->notes, "ssl-access-forbidden", (void *)1);
            return FORBIDDEN;
        }
    }

    /*
     * Else access is granted from our point of view (except vendor
     * handlers override). But we have to return DECLINED here instead
     * of OK, because mod_auth and other modules still might want to
     * deny access.
     */
    rc = DECLINED;
#ifdef SSL_VENDOR
    ap_hook_use("ap::mod_ssl::vendor::access_handler",
                AP_HOOK_SIG2(int,ptr), AP_HOOK_DECLINE(DECLINED),
                &rc, r);
#endif
    return rc;
}

/*
 *  Auth Handler:
 *  Fake a Basic authentication from the X509 client certificate.
 *
 *  This must be run fairly early on to prevent a real authentication from
 *  occuring, in particular it must be run before anything else that
 *  authenticates a user.  This means that the Module statement for this
 *  module should be LAST in the Configuration file.
 */
int ssl_hook_Auth(request_rec *r)
{
    SSLSrvConfigRec *sc = mySrvConfig(r->server);
    SSLDirConfigRec *dc = myDirConfig(r);
    char b1[MAX_STRING_LEN], b2[MAX_STRING_LEN];
    char *clientdn;
    const char *cpAL;
    const char *cpUN;
    const char *cpPW;

    /*
     * Additionally forbid access (again)
     * when strict require option is used.
     */
    if (   (dc->nOptions & SSL_OPT_STRICTREQUIRE)
        && (ap_table_get(r->notes, "ssl-access-forbidden") != NULL))
        return FORBIDDEN;

    /*
     * Make sure the user is not able to fake the client certificate
     * based authentication by just entering an X.509 Subject DN
     * ("/XX=YYY/XX=YYY/..") as the username and "password" as the
     * password.
     */
    if ((cpAL = ap_table_get(r->headers_in, "Authorization")) != NULL) {
        if (strcEQ(ap_getword(r->pool, &cpAL, ' '), "Basic")) {
            while (*cpAL == ' ' || *cpAL == '\t')
                cpAL++;
            cpAL = ap_pbase64decode(r->pool, cpAL);
            cpUN = ap_getword_nulls(r->pool, &cpAL, ':');
            cpPW = cpAL;
            if (cpUN[0] == '/' && strEQ(cpPW, "password"))
                return FORBIDDEN;
        }
    }

    /*
     * We decline operation in various situations...
     */
    if (!sc->bEnabled)
        return DECLINED;
    if (ap_ctx_get(r->connection->client->ctx, "ssl") == NULL)
        return DECLINED;
    if (!(dc->nOptions & SSL_OPT_FAKEBASICAUTH))
        return DECLINED;
    if (r->connection->user)
        return DECLINED;
    if ((clientdn = (char *)ap_ctx_get(r->connection->client->ctx, "ssl::client::dn")) == NULL)
        return DECLINED;

    /*
     * Fake a password - which one would be immaterial, as, it seems, an empty
     * password in the users file would match ALL incoming passwords, if only
     * we were using the standard crypt library routine. Unfortunately, OpenSSL
     * "fixes" a "bug" in crypt and thus prevents blank passwords from
     * working.  (IMHO what they really fix is a bug in the users of the code
     * - failing to program correctly for shadow passwords).  We need,
     * therefore, to provide a password. This password can be matched by
     * adding the string "xxj31ZMTZzkVA" as the password in the user file.
     * This is just the crypted variant of the word "password" ;-)
     */
    ap_snprintf(b1, sizeof(b1), "%s:password", clientdn);
    ssl_util_uuencode(b2, b1, FALSE);
    ap_snprintf(b1, sizeof(b1), "Basic %s", b2);
    ap_table_set(r->headers_in, "Authorization", b1);
    ssl_log(r->server, SSL_LOG_INFO,
            "Faking HTTP Basic Auth header: \"Authorization: %s\"", b1);

    return DECLINED;
}

int ssl_hook_UserCheck(request_rec *r)
{
    SSLDirConfigRec *dc = myDirConfig(r);

    /*
     * Additionally forbid access (again)
     * when strict require option is used.
     */
    if (   (dc->nOptions & SSL_OPT_STRICTREQUIRE)
        && (ap_table_get(r->notes, "ssl-access-forbidden") != NULL))
        return FORBIDDEN;

    return DECLINED;
}

/*
 *   Fixup Handler
 */

static const char *ssl_hook_Fixup_vars[] = {
    "SSL_VERSION_INTERFACE",
    "SSL_VERSION_LIBRARY",
    "SSL_PROTOCOL",
    "SSL_CIPHER",
    "SSL_CIPHER_EXPORT",
    "SSL_CIPHER_USEKEYSIZE",
    "SSL_CIPHER_ALGKEYSIZE",
    "SSL_CLIENT_VERIFY",
    "SSL_CLIENT_M_VERSION",
    "SSL_CLIENT_M_SERIAL",
    "SSL_CLIENT_V_START",
    "SSL_CLIENT_V_END",
    "SSL_CLIENT_S_DN",
    "SSL_CLIENT_S_DN_C",
    "SSL_CLIENT_S_DN_ST",
    "SSL_CLIENT_S_DN_L",
    "SSL_CLIENT_S_DN_O",
    "SSL_CLIENT_S_DN_OU",
    "SSL_CLIENT_S_DN_CN",
    "SSL_CLIENT_S_DN_T",
    "SSL_CLIENT_S_DN_I",
    "SSL_CLIENT_S_DN_G",
    "SSL_CLIENT_S_DN_S",
    "SSL_CLIENT_S_DN_D",
    "SSL_CLIENT_S_DN_UID",
    "SSL_CLIENT_S_DN_Email",
    "SSL_CLIENT_I_DN",
    "SSL_CLIENT_I_DN_C",
    "SSL_CLIENT_I_DN_ST",
    "SSL_CLIENT_I_DN_L",
    "SSL_CLIENT_I_DN_O",
    "SSL_CLIENT_I_DN_OU",
    "SSL_CLIENT_I_DN_CN",
    "SSL_CLIENT_I_DN_T",
    "SSL_CLIENT_I_DN_I",
    "SSL_CLIENT_I_DN_G",
    "SSL_CLIENT_I_DN_S",
    "SSL_CLIENT_I_DN_D",
    "SSL_CLIENT_I_DN_UID",
    "SSL_CLIENT_I_DN_Email",
    "SSL_CLIENT_A_KEY",
    "SSL_CLIENT_A_SIG",
    "SSL_SERVER_M_VERSION",
    "SSL_SERVER_M_SERIAL",
    "SSL_SERVER_V_START",
    "SSL_SERVER_V_END",
    "SSL_SERVER_S_DN",
    "SSL_SERVER_S_DN_C",
    "SSL_SERVER_S_DN_ST",
    "SSL_SERVER_S_DN_L",
    "SSL_SERVER_S_DN_O",
    "SSL_SERVER_S_DN_OU",
    "SSL_SERVER_S_DN_CN",
    "SSL_SERVER_S_DN_T",
    "SSL_SERVER_S_DN_I",
    "SSL_SERVER_S_DN_G",
    "SSL_SERVER_S_DN_S",
    "SSL_SERVER_S_DN_D",
    "SSL_SERVER_S_DN_UID",
    "SSL_SERVER_S_DN_Email",
    "SSL_SERVER_I_DN",
    "SSL_SERVER_I_DN_C",
    "SSL_SERVER_I_DN_ST",
    "SSL_SERVER_I_DN_L",
    "SSL_SERVER_I_DN_O",
    "SSL_SERVER_I_DN_OU",
    "SSL_SERVER_I_DN_CN",
    "SSL_SERVER_I_DN_T",
    "SSL_SERVER_I_DN_I",
    "SSL_SERVER_I_DN_G",
    "SSL_SERVER_I_DN_S",
    "SSL_SERVER_I_DN_D",
    "SSL_SERVER_I_DN_UID",
    "SSL_SERVER_I_DN_Email",
    "SSL_SERVER_A_KEY",
    "SSL_SERVER_A_SIG",
    "SSL_SESSION_ID",
    NULL
};

int ssl_hook_Fixup(request_rec *r)
{
    SSLSrvConfigRec *sc = mySrvConfig(r->server);
    SSLDirConfigRec *dc = myDirConfig(r);
    table *e = r->subprocess_env;
    char *var;
    char *val;
    STACK_OF(X509) *sk;
    SSL *ssl;
    int i;

    /*
     * Check to see if SSL is on
     */
    if (!sc->bEnabled)
        return DECLINED;
    if ((ssl = ap_ctx_get(r->connection->client->ctx, "ssl")) == NULL)
        return DECLINED;

    /*
     * Annotate the SSI/CGI environment with standard SSL information
     */
    /* the always present HTTPS (=HTTP over SSL) flag! */
    ap_table_set(e, "HTTPS", "on"); 
    /* standard SSL environment variables */
    if (dc->nOptions & SSL_OPT_STDENVVARS) {
        for (i = 0; ssl_hook_Fixup_vars[i] != NULL; i++) {
            var = (char *)ssl_hook_Fixup_vars[i];
            val = ssl_var_lookup(r->pool, r->server, r->connection, r, var);
            if (!strIsEmpty(val))
                ap_table_set(e, var, val);
        }
    }

    /*
     * On-demand bloat up the SSI/CGI environment with certificate data
     */
    if (dc->nOptions & SSL_OPT_EXPORTCERTDATA) {
        val = ssl_var_lookup(r->pool, r->server, r->connection, r, "SSL_SERVER_CERT");
        ap_table_set(e, "SSL_SERVER_CERT", val);
        val = ssl_var_lookup(r->pool, r->server, r->connection, r, "SSL_CLIENT_CERT");
        ap_table_set(e, "SSL_CLIENT_CERT", val);
        if ((sk = SSL_get_peer_cert_chain(ssl)) != NULL) {
            for (i = 0; i < sk_X509_num(sk); i++) {
                var = ap_psprintf(r->pool, "SSL_CLIENT_CERT_CHAIN_%d", i);
                val = ssl_var_lookup(r->pool, r->server, r->connection, r, var);
                if (val != NULL)
                     ap_table_set(e, var, val);
            }
        }
    }

    /*
     * On-demand bloat up the SSI/CGI environment with compat variables
     */
#ifdef SSL_COMPAT
    if (dc->nOptions & SSL_OPT_COMPATENVVARS)
        ssl_compat_variables(r);
#endif

    return DECLINED;
}

/*  _________________________________________________________________
**
**  OpenSSL Callback Functions
**  _________________________________________________________________
*/

/*
 * Handle out temporary RSA private keys on demand
 *
 * The background of this as the TLSv1 standard explains it:
 *
 * | D.1. Temporary RSA keys
 * |
 * |    US Export restrictions limit RSA keys used for encryption to 512
 * |    bits, but do not place any limit on lengths of RSA keys used for
 * |    signing operations. Certificates often need to be larger than 512
 * |    bits, since 512-bit RSA keys are not secure enough for high-value
 * |    transactions or for applications requiring long-term security. Some
 * |    certificates are also designated signing-only, in which case they
 * |    cannot be used for key exchange.
 * |
 * |    When the public key in the certificate cannot be used for encryption,
 * |    the server signs a temporary RSA key, which is then exchanged. In
 * |    exportable applications, the temporary RSA key should be the maximum
 * |    allowable length (i.e., 512 bits). Because 512-bit RSA keys are
 * |    relatively insecure, they should be changed often. For typical
 * |    electronic commerce applications, it is suggested that keys be
 * |    changed daily or every 500 transactions, and more often if possible.
 * |    Note that while it is acceptable to use the same temporary key for
 * |    multiple transactions, it must be signed each time it is used.
 * |
 * |    RSA key generation is a time-consuming process. In many cases, a
 * |    low-priority process can be assigned the task of key generation.
 * |    Whenever a new key is completed, the existing temporary key can be
 * |    replaced with the new one.
 *
 * So we generated 512 and 1024 bit temporary keys on startup
 * which we now just handle out on demand....
 */
RSA *ssl_callback_TmpRSA(SSL *pSSL, int nExport, int nKeyLen)
{
    SSLModConfigRec *mc = myModConfig();
    RSA *rsa;

    rsa = NULL;
    if (nExport) {
        /* It's because an export cipher is used */
        if (nKeyLen == 512)
            rsa = (RSA *)mc->pTmpKeys[SSL_TKPIDX_RSA512];
        else if (nKeyLen == 1024)
            rsa = (RSA *)mc->pTmpKeys[SSL_TKPIDX_RSA1024];
        else
            /* it's too expensive to generate on-the-fly, so keep 1024bit */
            rsa = (RSA *)mc->pTmpKeys[SSL_TKPIDX_RSA1024];
    }
    else {
        /* It's because a sign-only certificate situation exists */
        rsa = (RSA *)mc->pTmpKeys[SSL_TKPIDX_RSA1024];
    }
    return rsa;
}

/* 
 * Handle out the already generated DH parameters...
 */
DH *ssl_callback_TmpDH(SSL *pSSL, int nExport, int nKeyLen)
{
    SSLModConfigRec *mc = myModConfig();
    DH *dh;

    dh = NULL;
    if (nExport) {
        /* It's because an export cipher is used */
        if (nKeyLen == 512)
            dh = (DH *)mc->pTmpKeys[SSL_TKPIDX_DH512];
        else if (nKeyLen == 1024)
            dh = (DH *)mc->pTmpKeys[SSL_TKPIDX_DH1024];
        else
            /* it's too expensive to generate on-the-fly, so keep 1024bit */
            dh = (DH *)mc->pTmpKeys[SSL_TKPIDX_DH1024];
    }
    else {
        /* It's because a sign-only certificate situation exists */
        dh = (DH *)mc->pTmpKeys[SSL_TKPIDX_DH1024];
    }
    return dh;
}

/*
 * This OpenSSL callback function is called when OpenSSL
 * does client authentication and verifies the certificate chain.
 */
int ssl_callback_SSLVerify(int ok, X509_STORE_CTX *ctx)
{
    SSL *ssl;
    conn_rec *conn;
    server_rec *s;
    request_rec *r;
    SSLSrvConfigRec *sc;
    SSLDirConfigRec *dc;
    ap_ctx *actx;
    X509 *xs;
    int errnum;
    int errdepth;
    char *cp;
    char *cp2;
    int depth;
    int verify;

    /*
     * Get Apache context back through OpenSSL context
     */
    ssl  = (SSL *)X509_STORE_CTX_get_app_data(ctx);
    conn = (conn_rec *)SSL_get_app_data(ssl);
    actx = (ap_ctx *)SSL_get_app_data2(ssl);
    r    = (request_rec *)ap_ctx_get(actx, "ssl::request_rec");
    s    = conn->server;
    sc   = mySrvConfig(s);
    dc   = (r != NULL ? myDirConfig(r) : NULL);

    /*
     * Get verify ingredients
     */
    xs       = X509_STORE_CTX_get_current_cert(ctx);
    errnum   = X509_STORE_CTX_get_error(ctx);
    errdepth = X509_STORE_CTX_get_error_depth(ctx);

    /*
     * Log verification information
     */
    cp  = X509_NAME_oneline(X509_get_subject_name(xs), NULL, 0);
    cp2 = X509_NAME_oneline(X509_get_issuer_name(xs),  NULL, 0);
    ssl_log(s, SSL_LOG_TRACE,
            "Certificate Verification: depth: %d, subject: %s, issuer: %s",
            errdepth, cp != NULL ? cp : "-unknown-",
            cp2 != NULL ? cp2 : "-unknown");
    if (cp)
        free(cp);
    if (cp2)
        free(cp2);

    /*
     * Check for optionally acceptable non-verifiable issuer situation
     */
    if (dc != NULL && dc->nVerifyClient != SSL_CVERIFY_UNSET)
        verify = dc->nVerifyClient;
    else
        verify = sc->nVerifyClient;
    if (   (   errnum == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT
            || errnum == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN
            || errnum == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY
#if SSL_LIBRARY_VERSION >= 0x00905000
            || errnum == X509_V_ERR_CERT_UNTRUSTED
#endif
            || errnum == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE  )
        && verify == SSL_CVERIFY_OPTIONAL_NO_CA                       ) {
        ssl_log(s, SSL_LOG_TRACE,
                "Certificate Verification: Verifiable Issuer is configured as "
                "optional, therefore we're accepting the certificate");
        ap_ctx_set(conn->client->ctx, "ssl::verify::info", "GENEROUS");
        ok = TRUE;
    }

    /*
     * Additionally perform CRL-based revocation checks
     */
    if (ok) {
        ok = ssl_callback_SSLVerify_CRL(ok, ctx, s);
        if (!ok)
            errnum = X509_STORE_CTX_get_error(ctx);
    }

    /*
     * If we already know it's not ok, log the real reason
     */
    if (!ok) {
        ssl_log(s, SSL_LOG_ERROR, "Certificate Verification: Error (%d): %s",
                errnum, X509_verify_cert_error_string(errnum));
        ap_ctx_set(conn->client->ctx, "ssl::client::dn", NULL);
        ap_ctx_set(conn->client->ctx, "ssl::verify::error",
                   (void *)X509_verify_cert_error_string(errnum));
    }

    /*
     * Finally check the depth of the certificate verification
     */
    if (dc != NULL && dc->nVerifyDepth != UNSET)
        depth = dc->nVerifyDepth;
    else
        depth = sc->nVerifyDepth;
    if (errdepth > depth) {
        ssl_log(s, SSL_LOG_ERROR,
                "Certificate Verification: Certificate Chain too long "
                "(chain has %d certificates, but maximum allowed are only %d)",
                errdepth, depth);
        ap_ctx_set(conn->client->ctx, "ssl::verify::error",
                   (void *)X509_verify_cert_error_string(X509_V_ERR_CERT_CHAIN_TOO_LONG));
        ok = FALSE;
    }

    /*
     * And finally signal OpenSSL the (perhaps changed) state
     */
    return (ok);
}

int ssl_callback_SSLVerify_CRL(
    int ok, X509_STORE_CTX *ctx, server_rec *s)
{
    SSLSrvConfigRec *sc;
    X509_OBJECT obj;
    X509_NAME *subject;
    X509_NAME *issuer;
    X509 *xs;
    X509_CRL *crl;
    X509_REVOKED *revoked;
    long serial;
    BIO *bio;
    int i, n, rc;
    char *cp;
    char *cp2;

    /*
     * Unless a revocation store for CRLs was created we
     * cannot do any CRL-based verification, of course.
     */
    sc = mySrvConfig(s);
    if (sc->pRevocationStore == NULL)
        return ok;

    /*
     * Determine certificate ingredients in advance
     */
    xs      = X509_STORE_CTX_get_current_cert(ctx);
    subject = X509_get_subject_name(xs);
    issuer  = X509_get_issuer_name(xs);

    /*
     * OpenSSL provides the general mechanism to deal with CRLs but does not
     * use them automatically when verifying certificates, so we do it
     * explicitly here. We will check the CRL for the currently checked
     * certificate, if there is such a CRL in the store.
     *
     * We come through this procedure for each certificate in the certificate
     * chain, starting with the root-CA's certificate. At each step we've to
     * both verify the signature on the CRL (to make sure it's a valid CRL)
     * and it's revocation list (to make sure the current certificate isn't
     * revoked).  But because to check the signature on the CRL we need the
     * public key of the issuing CA certificate (which was already processed
     * one round before), we've a little problem. But we can both solve it and
     * at the same time optimize the processing by using the following
     * verification scheme (idea and code snippets borrowed from the GLOBUS
     * project):
     *
     * 1. We'll check the signature of a CRL in each step when we find a CRL
     *    through the _subject_ name of the current certificate. This CRL
     *    itself will be needed the first time in the next round, of course.
     *    But we do the signature processing one round before this where the
     *    public key of the CA is available.
     *
     * 2. We'll check the revocation list of a CRL in each step when
     *    we find a CRL through the _issuer_ name of the current certificate.
     *    This CRLs signature was then already verified one round before.
     *
     * This verification scheme allows a CA to revoke its own certificate as
     * well, of course.
     */

    /*
     * Try to retrieve a CRL corresponding to the _subject_ of
     * the current certificate in order to verify it's integrity.
     */
    memset((char *)&obj, 0, sizeof(obj));
    rc = SSL_X509_STORE_lookup(sc->pRevocationStore, X509_LU_CRL, subject, &obj);
    crl = obj.data.crl;
    if (rc > 0 && crl != NULL) {
        /*
         * Log information about CRL
         * (A little bit complicated because of ASN.1 and BIOs...)
         */
        if (ssl_log_applies(s, SSL_LOG_TRACE)) {
            bio = BIO_new(BIO_s_mem());
            BIO_printf(bio, "lastUpdate: ");
            ASN1_UTCTIME_print(bio, X509_CRL_get_lastUpdate(crl));
            BIO_printf(bio, ", nextUpdate: ");
            ASN1_UTCTIME_print(bio, X509_CRL_get_nextUpdate(crl));
            n = BIO_pending(bio);
            cp = malloc(n+1);
            n = BIO_read(bio, cp, n);
            cp[n] = NUL;
            BIO_free(bio);
            cp2 = X509_NAME_oneline(subject, NULL, 0);
            ssl_log(s, SSL_LOG_TRACE, "CA CRL: Issuer: %s, %s", cp2, cp);
            free(cp2);
            free(cp);
        }

        /*
         * Verify the signature on this CRL
         */
        if (X509_CRL_verify(crl, X509_get_pubkey(xs)) <= 0) {
            ssl_log(s, SSL_LOG_WARN, "Invalid signature on CRL");
            X509_STORE_CTX_set_error(ctx, X509_V_ERR_CRL_SIGNATURE_FAILURE);
            X509_OBJECT_free_contents(&obj);
            return FALSE;
        }

        /*
         * Check date of CRL to make sure it's not expired
         */
        i = X509_cmp_current_time(X509_CRL_get_nextUpdate(crl));
        if (i == 0) {
            ssl_log(s, SSL_LOG_WARN, "Found CRL has invalid nextUpdate field");
            X509_STORE_CTX_set_error(ctx, X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD);
            X509_OBJECT_free_contents(&obj);
            return FALSE;
        }
        if (i < 0) {
            ssl_log(s, SSL_LOG_WARN,
                    "Found CRL is expired - "
                    "revoking all certificates until you get updated CRL");
            X509_STORE_CTX_set_error(ctx, X509_V_ERR_CRL_HAS_EXPIRED);
            X509_OBJECT_free_contents(&obj);
            return FALSE;
        }
        X509_OBJECT_free_contents(&obj);
    }

    /*
     * Try to retrieve a CRL corresponding to the _issuer_ of
     * the current certificate in order to check for revocation.
     */
    memset((char *)&obj, 0, sizeof(obj));
    rc = SSL_X509_STORE_lookup(sc->pRevocationStore, X509_LU_CRL, issuer, &obj);
    crl = obj.data.crl;
    if (rc > 0 && crl != NULL) {
        /*
         * Check if the current certificate is revoked by this CRL
         */
#if SSL_LIBRARY_VERSION < 0x00904000
        n = sk_num(X509_CRL_get_REVOKED(crl));
#else
        n = sk_X509_REVOKED_num(X509_CRL_get_REVOKED(crl));
#endif
        for (i = 0; i < n; i++) {
#if SSL_LIBRARY_VERSION < 0x00904000
            revoked = (X509_REVOKED *)sk_value(X509_CRL_get_REVOKED(crl), i);
#else
            revoked = sk_X509_REVOKED_value(X509_CRL_get_REVOKED(crl), i);
#endif
            if (ASN1_INTEGER_cmp(revoked->serialNumber, X509_get_serialNumber(xs)) == 0) {

                serial = ASN1_INTEGER_get(revoked->serialNumber);
                cp = X509_NAME_oneline(issuer, NULL, 0);
                ssl_log(s, SSL_LOG_INFO,
                        "Certificate with serial %ld (0x%lX) "
                        "revoked per CRL from issuer %s",
                        serial, serial, cp);
                free(cp);

                X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_REVOKED);
                X509_OBJECT_free_contents(&obj);
                return FALSE;
            }
        }
        X509_OBJECT_free_contents(&obj);
    }
    return ok;
}

/*
 *  This callback function is executed by OpenSSL whenever a new SSL_SESSION is
 *  added to the internal OpenSSL session cache. We use this hook to spread the
 *  SSL_SESSION also to the inter-process disk-cache to make share it with our
 *  other Apache pre-forked server processes.
 */
int ssl_callback_NewSessionCacheEntry(SSL *ssl, SSL_SESSION *pNew)
{
    conn_rec *conn;
    server_rec *s;
    SSLSrvConfigRec *sc;
    long t;
    BOOL rc;

    /*
     * Get Apache context back through OpenSSL context
     */
    conn = (conn_rec *)SSL_get_app_data(ssl);
    s    = conn->server;
    sc   = mySrvConfig(s);

    /*
     * Set the timeout also for the internal OpenSSL cache, because this way
     * our inter-process cache is consulted only when it's really necessary.
     */
    t = sc->nSessionCacheTimeout;
    SSL_set_timeout(pNew, t);

    /*
     * Store the SSL_SESSION in the inter-process cache with the
     * same expire time, so it expires automatically there, too.
     */
    t = (SSL_get_time(pNew) + sc->nSessionCacheTimeout);
    rc = ssl_scache_store(s, pNew->session_id, pNew->session_id_length, t, pNew);

    /*
     * Log this cache operation
     */
    ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
            "request=SET status=%s id=%s timeout=%ds (session caching)",
            rc == TRUE ? "OK" : "BAD",
            SSL_SESSION_id2sz(pNew->session_id, pNew->session_id_length),
            t-time(NULL));

    /*
     * return 0 which means to OpenSSL that the pNew is still
     * valid and was not freed by us with SSL_SESSION_free().
     */
    return 0;
}

/*
 *  This callback function is executed by OpenSSL whenever a
 *  SSL_SESSION is looked up in the internal OpenSSL cache and it
 *  was not found. We use this to lookup the SSL_SESSION in the
 *  inter-process disk-cache where it was perhaps stored by one
 *  of our other Apache pre-forked server processes.
 */
SSL_SESSION *ssl_callback_GetSessionCacheEntry(
    SSL *ssl, unsigned char *id, int idlen, int *pCopy)
{
    conn_rec *conn;
    server_rec *s;
    SSL_SESSION *pSession;

    /*
     * Get Apache context back through OpenSSL context
     */
    conn = (conn_rec *)SSL_get_app_data(ssl);
    s    = conn->server;

    /*
     * Try to retrieve the SSL_SESSION from the inter-process cache
     */
    pSession = ssl_scache_retrieve(s, id, idlen);

    /*
     * Log this cache operation
     */
    if (pSession != NULL)
        ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
                "request=GET status=FOUND id=%s (session reuse)",
                SSL_SESSION_id2sz(id, idlen));
    else
        ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
                "request=GET status=MISSED id=%s (session renewal)",
                SSL_SESSION_id2sz(id, idlen));

    /*
     * Return NULL or the retrieved SSL_SESSION. But indicate (by
     * setting pCopy to 0) that the reference count on the
     * SSL_SESSION should not be incremented by the SSL library,
     * because we will no longer hold a reference to it ourself.
     */
    *pCopy = 0;
    return pSession;
}

/*
 *  This callback function is executed by OpenSSL whenever a
 *  SSL_SESSION is removed from the the internal OpenSSL cache.
 *  We use this to remove the SSL_SESSION in the inter-process
 *  disk-cache, too.
 */
void ssl_callback_DelSessionCacheEntry(
    SSL_CTX *ctx, SSL_SESSION *pSession)
{
    server_rec *s;

    /*
     * Get Apache context back through OpenSSL context
     */
    s = (server_rec *)SSL_CTX_get_app_data(ctx);
    if (s == NULL) /* on server shutdown Apache is already gone */
        return;

    /*
     * Remove the SSL_SESSION from the inter-process cache
     */
    ssl_scache_remove(s, pSession->session_id, pSession->session_id_length);

    /*
     * Log this cache operation
     */
    ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
            "request=REM status=OK id=%s (session dead)",
            SSL_SESSION_id2sz(pSession->session_id,
            pSession->session_id_length));

    return;
}

/*
 * This callback function is executed while OpenSSL processes the
 * SSL handshake and does SSL record layer stuff. We use it to
 * trace OpenSSL's processing in out SSL logfile.
 */
void ssl_callback_LogTracingState(SSL *ssl, int where, int rc)
{
    conn_rec *c;
    server_rec *s;
    SSLSrvConfigRec *sc;
    char *str;

    /*
     * find corresponding server
     */
    if ((c = (conn_rec *)SSL_get_app_data(ssl)) == NULL)
        return;
    s = c->server;
    if ((sc = mySrvConfig(s)) == NULL)
        return;

    /*
     * create the various trace messages
     */
    if (sc->nLogLevel >= SSL_LOG_TRACE) {
        if (where & SSL_CB_HANDSHAKE_START)
            ssl_log(s, SSL_LOG_TRACE, "%s: Handshake: start", SSL_LIBRARY_NAME);
        else if (where & SSL_CB_HANDSHAKE_DONE)
            ssl_log(s, SSL_LOG_TRACE, "%s: Handshake: done", SSL_LIBRARY_NAME);
        else if (where & SSL_CB_LOOP)
            ssl_log(s, SSL_LOG_TRACE, "%s: Loop: %s",
                    SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        else if (where & SSL_CB_READ)
            ssl_log(s, SSL_LOG_TRACE, "%s: Read: %s",
                    SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        else if (where & SSL_CB_WRITE)
            ssl_log(s, SSL_LOG_TRACE, "%s: Write: %s",
                    SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        else if (where & SSL_CB_ALERT) {
            str = (where & SSL_CB_READ) ? "read" : "write";
            ssl_log(s, SSL_LOG_TRACE, "%s: Alert: %s:%s:%s\n",
                    SSL_LIBRARY_NAME, str,
                    SSL_alert_type_string_long(rc),
                    SSL_alert_desc_string_long(rc));
        }
        else if (where & SSL_CB_EXIT) {
            if (rc == 0)
                ssl_log(s, SSL_LOG_TRACE, "%s: Exit: failed in %s",
                        SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
            else if (rc < 0)
                ssl_log(s, SSL_LOG_TRACE, "%s: Exit: error in %s",
                        SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        }
    }

    /*
     * Because SSL renegotations can happen at any time (not only after
     * SSL_accept()), the best way to log the current connection details is
     * right after a finished handshake.
     */
    if (where & SSL_CB_HANDSHAKE_DONE) {
        ssl_log(s, SSL_LOG_INFO,
                "Connection: Client IP: %s, Protocol: %s, Cipher: %s (%s/%s bits)",
                ssl_var_lookup(NULL, s, c, NULL, "REMOTE_ADDR"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_PROTOCOL"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER_USEKEYSIZE"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER_ALGKEYSIZE"));
    }

    return;
}

