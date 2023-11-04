/*	$OpenBSD: application_internal.c,v 1.1 2023/11/04 09:22:52 martijn Exp $	*/

/*
 * Copyright (c) 2023 Martijn van Duren <martijn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/tree.h>

#include <event.h>
#include <stdlib.h>

#include "application.h"
#include "log.h"
#include "mib.h"
#include "smi.h"
#include "snmpd.h"

struct appl_internal_object {
	struct ber_oid			 oid;
	struct ber_element *		(*get)(struct ber_oid *);
	/* No getnext means the object is scalar */
	struct ber_element *		(*getnext)(int8_t, struct ber_oid *);

	RB_ENTRY(appl_internal_object)	 entry;
};

void appl_internal_region(struct ber_oid *);
void appl_internal_object(struct ber_oid *,
    struct ber_element *(*)(struct ber_oid *),
    struct ber_element *(*)(int8_t, struct ber_oid *));
void appl_internal_get(struct appl_backend *, int32_t, int32_t, const char *,
    struct appl_varbind *);
void appl_internal_getnext(struct appl_backend *, int32_t, int32_t,
    const char *, struct appl_varbind *);
struct ber_element *appl_internal_snmp(struct ber_oid *);
struct appl_internal_object *appl_internal_object_parent(struct ber_oid *);
int appl_internal_object_cmp(struct appl_internal_object *,
    struct appl_internal_object *);

struct appl_backend_functions appl_internal_functions = {
	.ab_get = appl_internal_get,
	.ab_getnext = appl_internal_getnext,
	.ab_getbulk = NULL, /* getbulk is too complex */
};

struct appl_backend appl_internal = {
	.ab_name = "internal",
	.ab_cookie = NULL,
	.ab_retries = 0,
	.ab_range = 1,
	.ab_fn = &appl_internal_functions
};

static RB_HEAD(appl_internal_objects, appl_internal_object)
    appl_internal_objects = RB_INITIALIZER(&appl_internal_objects);
RB_PROTOTYPE_STATIC(appl_internal_objects, appl_internal_object, entry,
    appl_internal_object_cmp);

void
appl_internal_init(void)
{
	appl_internal_region(&OID(MIB_snmp));
	appl_internal_object(&OID(MIB_snmpInPkts), appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpOutPkts), appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpInBadVersions), appl_internal_snmp,
	   NULL);
	appl_internal_object(&OID(MIB_snmpInBadCommunityNames),
	   appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpInBadCommunityUses),
	   appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpInASNParseErrs), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInTooBigs), appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpInNoSuchNames), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInBadValues), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInReadOnlys), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInReadOnlys), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInGenErrs), appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpInTotalReqVars), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInTotalSetVars), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInGetRequests), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInGetNexts), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInSetRequests), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInGetResponses), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpInTraps), appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpOutTooBigs), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutNoSuchNames), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutBadValues), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutGenErrs), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutGetRequests), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutGetNexts), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutSetRequests), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutGetResponses), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpOutTraps), appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpEnableAuthenTraps),
	    appl_internal_snmp, NULL);
	appl_internal_object(&OID(MIB_snmpSilentDrops), appl_internal_snmp,
	    NULL);
	appl_internal_object(&OID(MIB_snmpProxyDrops), appl_internal_snmp,
	    NULL);
}

void
appl_internal_shutdown(void)
{
	struct appl_internal_object *object;

	while ((object = RB_ROOT(&appl_internal_objects)) != NULL) {
		RB_REMOVE(appl_internal_objects, &appl_internal_objects,
		    object);
		free(object);
	}

	appl_close(&appl_internal);
}

void
appl_internal_region(struct ber_oid *oid)
{
	enum appl_error error;
	char oidbuf[1024];

	error = appl_register(NULL, 150, 1, oid, 0, 1, 0, 0, &appl_internal);
	/*
	 * Ignore requestDenied, duplicateRegistration, and unsupportedContext
	 */
	if (error == APPL_ERROR_PROCESSINGERROR ||
	    error == APPL_ERROR_PARSEERROR) {
		smi_oid2string(oid, oidbuf, sizeof(oidbuf), 0);
		fatalx("internal: Failed to register %s", oidbuf);
	}
}

void
appl_internal_object(struct ber_oid *oid,
    struct ber_element *(*get)(struct ber_oid *),
    struct ber_element *(*getnext)(int8_t, struct ber_oid *))
{
	struct appl_internal_object *obj;

	if ((obj = calloc(1, sizeof(*obj))) == NULL)
		fatal(NULL);
	obj->oid = *oid;
	obj->get = get;
	obj->getnext = getnext;

	RB_INSERT(appl_internal_objects, &appl_internal_objects, obj);
}

void
appl_internal_get(struct appl_backend *backend, __unused int32_t transactionid,
    int32_t requestid, __unused const char *ctx, struct appl_varbind *vblist)
{
	struct ber_oid oid;
	struct appl_internal_object *object;
	struct appl_varbind *vb, *resp;
	size_t i;
	int r;

	for (i = 0, vb = vblist; vb != NULL; vb = vb->av_next, i++)
		continue;

	if ((resp = calloc(i, sizeof(*resp))) == NULL) {
		log_warn("%s", backend->ab_name);
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vblist);
		return;
	}

	for (i = 0, vb = vblist; vb != NULL; vb = vb->av_next, i++) {
		resp[i].av_oid = vb->av_oid;
		if ((object = appl_internal_object_parent(&vb->av_oid)) == NULL)
			resp[i].av_value =
			    appl_exception(APPL_EXC_NOSUCHOBJECT);
		else {
			oid = object->oid;
			/* Add 0 element for scalar */
			if (object->getnext == NULL)
				oid.bo_id[oid.bo_n++] = 0;
			r = ober_oid_cmp(&vb->av_oid, &oid);
			if ((r == 0 && object->getnext == NULL) ||
			    (r == 2 && object->getnext != NULL))
				resp[i].av_value = object->get(&resp[i].av_oid);
			else
				resp[i].av_value =
				    appl_exception(APPL_EXC_NOSUCHINSTANCE);
		}
		if (resp[i].av_value == NULL) {
			log_warnx("%s: Failed to get value", backend->ab_name);
			goto fail;
		}
		resp[i].av_next = &resp[i + 1];
	}
	resp[i - 1].av_next = NULL;

	appl_response(backend, requestid, APPL_ERROR_NOERROR, 0, resp);
	return;

 fail:
	for (vb = resp; vb != NULL; vb = vb->av_next)
		ober_free_elements(vb->av_value);
	free(resp);
	appl_response(backend, requestid, APPL_ERROR_GENERR, i + 1, vblist);
}

void
appl_internal_getnext(struct appl_backend *backend,
    __unused int32_t transactionid, int32_t requestid, __unused const char *ctx,
    struct appl_varbind *vblist)
{
	struct ber_oid oid;
	struct appl_internal_object *object, search;
	struct appl_varbind *vb, *resp;
	size_t i;
	int r;
	int8_t include;

	for (i = 0, vb = vblist; vb != NULL; vb = vb->av_next, i++)
		continue;

	if ((resp = calloc(i, sizeof(*resp))) == NULL) {
		log_warn("%s", backend->ab_name);
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vblist);
		return;
	}

	for (i = 0, vb = vblist; vb != NULL; vb = vb->av_next, i++) {
		resp[i].av_oid = vb->av_oid;
		object = appl_internal_object_parent(&vb->av_oid);
		if (object == NULL) {
			search.oid = vb->av_oid;
			object = RB_NFIND(appl_internal_objects,
			    &appl_internal_objects, &search);
		}

		include = vb->av_include;
		for (; object != NULL; object = RB_NEXT(appl_internal_objects,
		    &appl_internal_objects, object), include = 1) {
			if (object->getnext == NULL) {
				oid = object->oid;
				oid.bo_id[oid.bo_n++] = 0;
				r = ober_oid_cmp(&resp[i].av_oid, &oid);
				if (r > 0 || (r == 0 && !include))
					continue;
				resp[i].av_oid = oid;
				resp[i].av_value = object->get(&oid);
				break;
			}
			/* non-scalar */
			fatalx("%s: not implemented", backend->ab_name);
		}
		if (ober_oid_cmp(&resp[i].av_oid, &vb->av_oid_end) >= 0 ||
		    object == NULL) {
			resp[i].av_oid = vb->av_oid;
			ober_free_elements(resp[i].av_value);
			resp[i].av_value =
			    appl_exception(APPL_EXC_ENDOFMIBVIEW);
		}
		if (resp[i].av_value == NULL) {
			log_warnx("%s: Failed to get value", backend->ab_name);
			goto fail;
		}
		resp[i].av_next = &resp[i + 1];
	}
	resp[i - 1].av_next = NULL;

	appl_response(backend, requestid, APPL_ERROR_NOERROR, 0, resp);
	return;

 fail:
	for (vb = resp; vb != NULL; vb = vb->av_next)
		ober_free_elements(vb->av_value);
	free(resp);
	appl_response(backend, requestid, APPL_ERROR_GENERR, i + 1, vblist);
}

struct ber_element *
appl_internal_snmp(struct ber_oid *oid)
{
	struct snmp_stats *stats = &snmpd_env->sc_stats;
	struct ber_element *value = NULL;

	if (ober_oid_cmp(oid, &OID(MIB_snmpEnableAuthenTraps, 0)) == 0)
		return ober_add_integer(NULL,
		    stats->snmp_enableauthentraps ? 1 : 2);
	if (ober_oid_cmp(&OID(MIB_snmpInPkts, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_inpkts);
	else if (ober_oid_cmp(&OID(MIB_snmpOutPkts, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outpkts);
	else if (ober_oid_cmp(&OID(MIB_snmpInBadVersions, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_inbadversions);
	else if (ober_oid_cmp(&OID(MIB_snmpInBadCommunityNames, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_inbadcommunitynames);
	else if (ober_oid_cmp(&OID(MIB_snmpInBadCommunityUses, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_inbadcommunityuses);
	else if (ober_oid_cmp(&OID(MIB_snmpInASNParseErrs, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_inasnparseerrs);
	else if (ober_oid_cmp(&OID(MIB_snmpInTooBigs, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_intoobigs);
	else if (ober_oid_cmp(&OID(MIB_snmpInNoSuchNames, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_innosuchnames);
	else if (ober_oid_cmp(&OID(MIB_snmpInBadValues, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_inbadvalues);
	else if (ober_oid_cmp(&OID(MIB_snmpInReadOnlys, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_inreadonlys);
	else if (ober_oid_cmp(&OID(MIB_snmpInGenErrs, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_ingenerrs);
	else if (ober_oid_cmp(&OID(MIB_snmpInTotalReqVars, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_intotalreqvars);
	else if (ober_oid_cmp(&OID(MIB_snmpInTotalSetVars, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_intotalsetvars);
	else if (ober_oid_cmp(&OID(MIB_snmpInGetRequests, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_ingetrequests);
	else if (ober_oid_cmp(&OID(MIB_snmpInGetNexts, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_ingetnexts);
	else if (ober_oid_cmp(&OID(MIB_snmpInSetRequests, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_insetrequests);
	else if (ober_oid_cmp(&OID(MIB_snmpInGetResponses, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_ingetresponses);
	else if (ober_oid_cmp(&OID(MIB_snmpInTraps, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_intraps);
	else if (ober_oid_cmp(&OID(MIB_snmpOutTooBigs, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outtoobigs);
	else if (ober_oid_cmp(&OID(MIB_snmpOutNoSuchNames, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outnosuchnames);
	else if (ober_oid_cmp(&OID(MIB_snmpOutBadValues, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outbadvalues);
	else if (ober_oid_cmp(&OID(MIB_snmpOutGenErrs, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outgenerrs);
	else if (ober_oid_cmp(&OID(MIB_snmpOutGetRequests, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outgetrequests);
	else if (ober_oid_cmp(&OID(MIB_snmpOutGetNexts, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outgetnexts);
	else if (ober_oid_cmp(&OID(MIB_snmpOutSetRequests, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outsetrequests);
	else if (ober_oid_cmp(&OID(MIB_snmpOutGetResponses, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outgetresponses);
	else if (ober_oid_cmp(&OID(MIB_snmpOutTraps, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_outtraps);
	else if (ober_oid_cmp(&OID(MIB_snmpSilentDrops, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_silentdrops);
	else if (ober_oid_cmp(&OID(MIB_snmpProxyDrops, 0), oid) == 0)
		value = ober_add_integer(NULL, stats->snmp_proxydrops);

	if (value != NULL)
		ober_set_header(value, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
	return value;
}

struct appl_internal_object *
appl_internal_object_parent(struct ber_oid *oid)
{
	struct appl_internal_object *object, search;

	search.oid = *oid;
	do {
		if ((object = RB_FIND(appl_internal_objects,
		    &appl_internal_objects, &search)) != NULL)
			return object;
	} while (--search.oid.bo_n > 0);

	return NULL;
}

int
appl_internal_object_cmp(struct appl_internal_object *o1,
    struct appl_internal_object *o2)
{
	return ober_oid_cmp(&o1->oid, &o2->oid);
}

RB_GENERATE_STATIC(appl_internal_objects, appl_internal_object, entry,
    appl_internal_object_cmp);
