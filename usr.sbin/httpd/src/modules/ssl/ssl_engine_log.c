/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |  mod_ssl
** | '_ ` _ \ / _ \ / _` |   / __/ __| |  Apache Interface to OpenSSL
** | | | | | | (_) | (_| |   \__ \__ \ |  www.modssl.org
** |_| |_| |_|\___/ \__,_|___|___/___/_|  ftp.modssl.org
**                      |_____|
**  ssl_engine_log.c
**  Logging Facility
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
                             /* ``The difference between a computer
                                  industry job and open-source software
                                  hacking is about 30 hours a week.''
                                         -- Ralf S. Engelschall     */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  Logfile Support
**  _________________________________________________________________
*/

/*
 * Open the SSL logfile
 */
void ssl_log_open(server_rec *s_main, server_rec *s, pool *p)
{
    char *szLogFile;
    SSLSrvConfigRec *sc_main = mySrvConfig(s_main);
    SSLSrvConfigRec *sc = mySrvConfig(s);
    piped_log *pl;

    /* 
     * Short-circuit for inherited logfiles in order to save
     * filedescriptors in mass-vhost situation. Be careful, this works
     * fine because the close happens implicitly by the pool facility.
     */
    if (   s != s_main 
        && sc_main->fileLogFile != NULL
        && (   (sc->szLogFile == NULL)
            || (   sc->szLogFile != NULL 
                && sc_main->szLogFile != NULL 
                && strEQ(sc->szLogFile, sc_main->szLogFile)))) {
        sc->fileLogFile = sc_main->fileLogFile;
    }
    else if (sc->szLogFile != NULL) {
        if (strEQ(sc->szLogFile, "/dev/null"))
            return;
        else if (sc->szLogFile[0] == '|') {
            szLogFile = ssl_util_server_root_relative(p, "log", sc->szLogFile+1);
            if ((pl = ap_open_piped_log(p, szLogFile)) == NULL) {
                ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                        "Cannot open reliable pipe to SSL logfile filter %s", szLogFile);
                ssl_die();
            }
            sc->fileLogFile = ap_pfdopen(p, ap_piped_log_write_fd(pl), "a");
            setbuf(sc->fileLogFile, NULL);
        }
        else {
            szLogFile = ssl_util_server_root_relative(p, "log", sc->szLogFile);
            if ((sc->fileLogFile = ap_pfopen(p, szLogFile, "a")) == NULL) {
                ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                        "Cannot open SSL logfile %s", szLogFile);
                ssl_die();
            }
            setbuf(sc->fileLogFile, NULL);
        }
    }
    return;
}

static struct {
    int   nLevel;
    char *szLevel;
} ssl_log_level2string[] = {
    { SSL_LOG_ERROR, "error" },
    { SSL_LOG_WARN,  "warn"  },
    { SSL_LOG_INFO,  "info"  },
    { SSL_LOG_TRACE, "trace" },
    { SSL_LOG_DEBUG, "debug" },
    { 0, NULL }
};

static struct {
    char *cpPattern;
    char *cpAnnotation;
} ssl_log_annotate[] = {
    { "*envelope*bad*decrypt*", "wrong pass phrase!?" },
    { "*CLIENT_HELLO*unknown*protocol*", "speaking not SSL to HTTPS port!?" },
    { "*CLIENT_HELLO*http*request*", "speaking HTTP to HTTPS port!?" },
    { "*SSL3_READ_BYTES:sslv3*alert*bad*certificate*", "Subject CN in certificate not server name or identical to CA!?" },
    { "*self signed certificate in certificate chain*", "Client certificate signed by CA not known to server?" },
    { "*peer did not return a certificate*", "No CAs known to server for verification?" },
    { "*no shared cipher*", "Too restrictive SSLCipherSuite or using DSA server certificate?" },
    { "*no start line*", "Bad file contents or format - or even just a forgotten SSLCertificateKeyFile?" },
    { "*bad password read*", "You entered an incorrect pass phrase!?" },
    { "*bad mac decode*", "Browser still remembered details of a re-created server certificate?" },
    { NULL, NULL }
};

static char *ssl_log_annotation(char *error)
{
    char *errstr;
    int i;

    errstr = NULL;
    for (i = 0; ssl_log_annotate[i].cpPattern != NULL; i++) {
        if (ap_strcmp_match(error, ssl_log_annotate[i].cpPattern) == 0) {
            errstr = ssl_log_annotate[i].cpAnnotation;
            break;
        }
    }
    return errstr;
}

BOOL ssl_log_applies(server_rec *s, int level)
{
    SSLSrvConfigRec *sc;

    sc = mySrvConfig(s);
    if (   sc->fileLogFile == NULL
        && !(level & SSL_LOG_ERROR))
        return FALSE;
    if (   level > sc->nLogLevel
        && !(level & SSL_LOG_ERROR))
        return FALSE;
    return TRUE;
}

void ssl_log(server_rec *s, int level, const char *msg, ...)
{
    char tstr[80];
    char lstr[20];
    char vstr[1024];
    char str[1024];
    char nstr[2];
    int timz;
    struct tm *t;
    va_list ap;
    int add;
    int i;
    char *astr;
    int safe_errno;
    unsigned long e;
    SSLSrvConfigRec *sc;
    char *cpE;
    char *cpA;

    /*  initialization  */
    va_start(ap, msg);
    safe_errno = errno;
    sc = mySrvConfig(s);

    /*  strip out additional flags  */
    add   = (level & ~SSL_LOG_MASK);
    level = (level & SSL_LOG_MASK);

    /*  reduce flags when not reasonable in context  */
    if (add & SSL_ADD_ERRNO && errno == 0)
        add &= ~SSL_ADD_ERRNO;
    if (add & SSL_ADD_SSLERR && ERR_peek_error() == 0)
        add &= ~SSL_ADD_SSLERR;

    /*  we log only levels below, except for errors */
    if (   sc->fileLogFile == NULL
        && !(level & SSL_LOG_ERROR))
        return;
    if (   level > sc->nLogLevel
        && !(level & SSL_LOG_ERROR))
        return;

    /*  determine the time entry string  */
    if (add & SSL_NO_TIMESTAMP)
        tstr[0] = NUL;
    else {
        t = ap_get_gmtoff(&timz);
        strftime(tstr, 80, "[%d/%b/%Y %H:%M:%S", t);
        i = strlen(tstr);
        ap_snprintf(tstr+i, 80-i, " %05d] ", (unsigned int)getpid());
    }

    /*  determine whether newline should be written */
    if (add & SSL_NO_NEWLINE)
        nstr[0] = NUL;
    else {
        nstr[0] = '\n';
        nstr[1] = NUL;
    }

    /*  determine level name  */
    lstr[0] = NUL;
    if (!(add & SSL_NO_LEVELID)) {
        for (i = 0; ssl_log_level2string[i].nLevel != 0; i++) {
            if (ssl_log_level2string[i].nLevel == level) {
                ap_snprintf(lstr, sizeof(lstr), "[%s]", ssl_log_level2string[i].szLevel);
                break;
            }
        }
        for (i = strlen(lstr); i <= 7; i++)
            lstr[i] = ' ';
        lstr[i] = NUL;
    }

    /*  create custom message  */
    ap_vsnprintf(vstr, sizeof(vstr), msg, ap);

    /*  write out SSLog message  */
    if ((add & SSL_ADD_ERRNO) && (add & SSL_ADD_SSLERR))
        astr = " (System and " SSL_LIBRARY_NAME " library errors follow)";
    else if (add & SSL_ADD_ERRNO)
        astr = " (System error follows)";
    else if (add & SSL_ADD_SSLERR)
        astr = " (" SSL_LIBRARY_NAME " library error follows)";
    else
        astr = "";
    if (level <= sc->nLogLevel && sc->fileLogFile != NULL) {
        ap_snprintf(str, sizeof(str), "%s%s%s%s%s", tstr, lstr, vstr, astr, nstr);
        fprintf(sc->fileLogFile, "%s", str);
    }
    if (level & SSL_LOG_ERROR)
        ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, s,
                     "mod_ssl: %s%s", vstr, astr);

    /*  write out additional attachment messages  */
    if (add & SSL_ADD_ERRNO) {
        if (level <= sc->nLogLevel && sc->fileLogFile != NULL) {
            ap_snprintf(str, sizeof(str), "%s%sSystem: %s (errno: %d)%s",
                        tstr, lstr, strerror(safe_errno), safe_errno, nstr);
            fprintf(sc->fileLogFile, "%s", str);
        }
        if (level & SSL_LOG_ERROR)
            ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, s,
                         "System: %s (errno: %d)",
                         strerror(safe_errno), safe_errno);
    }
    if (add & SSL_ADD_SSLERR) {
        while ((e = ERR_get_error())) {
            cpE = ERR_error_string(e, NULL);
            cpA = ssl_log_annotation(cpE);
            if (level <= sc->nLogLevel && sc->fileLogFile != NULL) {
                ap_snprintf(str, sizeof(str), "%s%s%s: %s%s%s%s%s",
                            tstr, lstr, SSL_LIBRARY_NAME, cpE,
                            cpA != NULL ? " [Hint: " : "",
                            cpA != NULL ? cpA : "", cpA != NULL ? "]" : "",
                            nstr);
                fprintf(sc->fileLogFile, "%s", str);
            }
            if (level & SSL_LOG_ERROR)
                ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, s,
                             "%s: %s%s%s%s", SSL_LIBRARY_NAME, cpE,
                             cpA != NULL ? " [Hint: " : "",
                             cpA != NULL ? cpA : "", cpA != NULL ? "]" : "");
        }
    }
    /* make sure the next log starts from a clean base */
    /* ERR_clear_error(); */

    /*  cleanup and return  */
    if (sc->fileLogFile != NULL)
        fflush(sc->fileLogFile);
    errno = safe_errno;
    va_end(ap);
    return;
}

void ssl_die(void)
{
    /*
     * This is used for fatal errors and here
     * it is common module practice to really
     * exit from the complete program.
     */
    exit(1);
}

