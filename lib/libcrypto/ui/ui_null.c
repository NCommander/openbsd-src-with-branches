/*	$OpenBSD$ */

/*
 * Written by Theo Buehler. Public domain.
 */

#include "ui_local.h"

static const UI_METHOD ui_null = {
	.name = "OpenSSL NULL UI",
};

const UI_METHOD *
UI_null(void)
{
	return &ui_null;
}
LCRYPTO_ALIAS(UI_null)
