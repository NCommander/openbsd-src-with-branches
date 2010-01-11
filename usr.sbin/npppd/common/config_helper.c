/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id: config_helper.c,v 1.2 2009/03/25 12:10:39 yasuoka Exp $ */
/**@file ����ե����إ�ѡ�
 * <p>
 * ���Ƥ��ޤ���</p>
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "properties.h"
#include "config_helper.h"
#include "debugmacro.h"

#define	KEYBUFSZ	512

/**
 * ����ե���������������뤿���ʸ����Ϣ��
 * (("prefix", "suffix") => "prefix.suffix") �򡢹Ԥ��ޤ��������Ǹ���ΥХåե�
 * �ΰ���֤��ޤ���
 */
const char *
config_key_prefix(const char *prefix, const char *suffix)
{
	static char keybuf[KEYBUFSZ];

	strlcpy(keybuf, prefix, sizeof(keybuf));
	strlcat(keybuf, ".", sizeof(keybuf));
	strlcat(keybuf, suffix, sizeof(keybuf));

	return keybuf;
}

/**
 * �����ʸ������֤��ޤ���
 *
 * @param   _this   {@link ::properties}�ؤΥݥ��󥿡�
 * @param   confKey ����ե�������������̾
 * @return  �����͡����꤬¸�ߤ��ʤ����ˤ� NULL ���֤�ޤ���
 */
const char *
config_str(struct properties *_this, const char *confKey)
{
	ASSERT(_this != NULL)

	return properties_get(_this, confKey);
}

/**
 * ����� int ���֤��ޤ���
 *
 * @param   _this   	{@link ::properties}�ؤΥݥ��󥿡�
 * @param   confKey 	����ե�������������̾
 * @param   defValue	���꤬��ά����Ƥ�����Υǥե���Ȥ���
 */
int
config_int(struct properties *_this, const char *confKey, int defValue)
{
	int rval, x;
	const char *val;

	val = config_str(_this, confKey);

	if (val == NULL)
		return defValue;

	x = sscanf(val, "%d", &rval);

	if (x != 1)
		/* �ؿ��Υ��󥿥ե��������ѹ����ơ����顼�϶��̤��٤����� */
		return defValue;

	return rval;
}

/**
 * ���꤬��������줿ʸ����Ȱ��פ��뤫�ɤ������֤��ޤ���
 *
 * @param   _this   	{@link ::properties}�ؤΥݥ��󥿡�
 * @param   confKey 	����ե�������������̾
 * @param   defValue	���꤬��ά����Ƥ�����Υǥե���Ȥ���
 * @return  ���פ�����ˤ� 1�����פ��ʤ����ˤ� 0 ���֤�ޤ���
 */
int
config_str_equal(struct properties *_this, const char *confKey,
    const char *str, int defValue)
{
	const char *val;

	val = config_str(_this, confKey);

	if (val == NULL)
		return defValue;

	return (strcmp(val, str) == 0)? 1 : 0;
}

/**
 * ���꤬��������줿ʸ����Ȱ��פ��뤫�ɤ������֤��ޤ���ASCII ʸ����
 * ��ʸ����ʸ����̵�뤷�ޤ���
 *
 * @param   _this   	{@link ::properties}�ؤΥݥ��󥿡�
 * @param   confKey 	����ե�������������̾
 * @param   defValue	���꤬��ά����Ƥ�����Υǥե���Ȥ���
 * @return  ���פ�����ˤ� 1�����פ��ʤ����ˤ� 0 ���֤�ޤ���
 */
int
config_str_equali(struct properties *_this, const char *confKey,
    const char *str, int defValue)
{
	const char *val;

	val = config_str(_this, confKey);

	if (val == NULL)
		return defValue;

	return (strcasecmp(val, str) == 0)? 1 : 0;
}

/***********************************************************************
 * �������̾�˻��ꤷ���ץ�ե��å�����Ĥ������������������꤬�ʤ����
 * �ץ�ե��å����ʤ���������ܤ������������뤿��δؿ��Ǥ���
 *
 * ���Ȥ���
 * 
 * pppoe.service_name: default_service
 * PPPoE0.pppoe.service_name: my_service
 *
 * �Ȥ������꤬���ä���硢
 *  config_prefixed_str(prop, "PPPoE0", "service_name")
 * ��ƤӽФ��� "my_service" �������Ǥ��ޤ�������ˡ�
 *
 * PPPoE0.pppoe.service_name: my_service
 *
 * ���ʤ����ˤϡ�"default_service" �������Ǥ��ޤ���
 *
 * config_helper.h ���������Ƥ��� PREFIXED_CONFIG_FUNCTIONS �ޥ����
 * �Ȥäơ��ץ�ե��å�����ʬ�λ�����ˡ����ꤷ�ƻȤ����Ȥ�Ǥ��ޤ���
 ***********************************************************************/
const char  *
config_prefixed_str(struct properties *_this, const char *prefix, const char *confKey)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return val;
	}

	return config_str(_this, confKey);
}

int         
config_prefixed_int(struct properties *_this, const char *prefix, const char *confKey, int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_int(_this, keybuf, defValue);
	}

	return config_int(_this, confKey, defValue);
}

int         
config_prefixed_str_equal(struct properties *_this, const char *prefix, const char *confKey, const char *str,
    int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_str_equal(_this, keybuf, str,
			    defValue);
	}

	return config_str_equal(_this, confKey, str, defValue);
}

int         
config_prefixed_str_equali(struct properties *_this, const char *prefix,
    const char *confKey, const char *str, int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	ASSERT(_this != NULL);

	if (prefix != NULL) {
		snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_str_equali(_this, keybuf, str,
			    defValue);
	}

	return config_str_equali(_this, confKey, str, defValue);
}

/***********************************************************************
 * �������̾�˻��ꤷ���ץ�ե��å����Ȼ��ꤷ��̾����Ĥ���������������
 * ���꤬�ʤ���Хץ�ե��å�����������ܤ������������뤿��δؿ��Ǥ���
 *
 * ���Ȥ���
 * 
 * ipcp.dns_primary: 192.168.0.1
 * ipcp.ipcp0.dns_primary: 192.168.0.2
 *
 * �Ȥ������꤬���ä���硢
 *  config_named_prefix_str(prop, "ipcp", "ipcp0", "dns_primary");
 * ��ƤӽФ��� "192.168.0.2" �������Ǥ��ޤ�������ˡ�
 *
 * ipcp.ipcp0.dns_primary: 192.168.0.2
 *
 * ���ʤ����ˤϡ�"192.168.0.1" �������Ǥ��ޤ���
 *
 * config_helper.h ���������Ƥ��� NAMED_PREFIX_CONFIG_FUNCTIONS �ޥ���
 * ��Ȥäơ��ץ�ե��å�����ʬ�λ�����ˡ����ꤷ�ƻȤ����Ȥ�Ǥ��ޤ���
 ***********************************************************************/
const char  *
config_named_prefix_str(struct properties *_this, const char *prefix,
    const char *name, const char *confKey)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (name != NULL && name[0] != '\0') {
		snprintf(keybuf, sizeof(keybuf), "%s.%s.%s", prefix, name,
		    confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return val;
	}

	snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
	return config_str(_this, keybuf);
}

int         
config_named_prefix_int(struct properties *_this, const char *prefix,
    const char *name, const char *confKey, int defValue)
{
	char keybuf[KEYBUFSZ];
	const char *val;

	if (name != NULL && name[0] != '\0') {
		snprintf(keybuf, sizeof(keybuf), "%s.%s.%s", prefix, name,
		    confKey);
		val = config_str(_this, keybuf);
		if (val != NULL)
			return config_int(_this, keybuf, defValue);
	}

	snprintf(keybuf, sizeof(keybuf), "%s.%s", prefix, confKey);
	return config_int(_this, keybuf, defValue);
}

int         
config_named_prefix_str_equal(struct properties *_this, const char *prefix,
    const char *name, const char *confKey, const char *str, int defValue)
{
	const char *val;

	val = config_named_prefix_str(_this, prefix, name, confKey);
	if (val == NULL)
		return defValue;

	return (strcmp(val, str) == 0)? 1 : 0;
}

int         
config_named_prefix_str_equali(struct properties *_this, const char *prefix,
    const char *name, const char *confKey, const char *str, int defValue)
{
	const char *val;

	val = config_named_prefix_str(_this, prefix, name, confKey);
	if (val == NULL)
		return defValue;

	return (strcasecmp(val, str) == 0)? 1 : 0;
}
