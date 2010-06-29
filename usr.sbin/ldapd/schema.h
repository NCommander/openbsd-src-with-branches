/*	$OpenBSD$ */

/*
 * Copyright (c) 2010 Martin Hedenfalk <martinh@openbsd.org>
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

#ifndef _schema_h_
#define _schema_h_

enum usage {
	USAGE_USER_APP,
	USAGE_DIR_OP,		/* operational attribute */
	USAGE_DIST_OP,		/* operational attribute */
	USAGE_DSA_OP		/* operational attribute */
};

struct name {
	SLIST_ENTRY(name)	 next;
	const char		*name;
};
SLIST_HEAD(name_list, name);

struct attr_type {
	RB_ENTRY(attr_type)	 link;
	char			*oid;
	struct name_list	*names;
	char			*desc;
	int			 obsolete;
	struct attr_type	*sup;
	char			*equality;
	char			*ordering;
	char			*substr;
	char			*syntax;
	int			 single;
	int			 collective;
	int			 immutable;	/* no-user-modification */
	enum usage		 usage;
};
RB_HEAD(attr_type_tree, attr_type);
RB_PROTOTYPE(attr_type_tree, attr_type, link, attr_oid_cmp);

struct attr_ptr {
	SLIST_ENTRY(attr_ptr)	 next;
	struct attr_type	*attr_type;
};
SLIST_HEAD(attr_list, attr_ptr);

enum object_kind {
	KIND_ABSTRACT,
	KIND_STRUCTURAL,
	KIND_AUXILIARY
};

struct object;
struct obj_ptr {
	SLIST_ENTRY(obj_ptr)	 next;
	struct object		*object;
};
SLIST_HEAD(obj_list, obj_ptr);

struct object {
	RB_ENTRY(object)	 link;
	char			*oid;
	struct name_list	*names;
	char			*desc;
	int			 obsolete;
	struct obj_list		*sup;
	enum object_kind	 kind;
	struct attr_list	*must;
	struct attr_list	*may;
};
RB_HEAD(object_tree, object);
RB_PROTOTYPE(object_tree, object, link, obj_oid_cmp);

struct oidname {
	RB_ENTRY(oidname)	 link;
	const char		*on_name;
#define	on_attr_type		 on_ptr.ou_attr_type
#define	on_object		 on_ptr.ou_object
	union	{
		struct attr_type	*ou_attr_type;
		struct object		*ou_object;
	} on_ptr;
};
RB_HEAD(oidname_tree, oidname);
RB_PROTOTYPE(oidname_tree, oidname, link, oidname_cmp);

struct symoid {
	RB_ENTRY(symoid)	 link;
	char			*name;		/* symbolic name */
	char			*oid;
};
RB_HEAD(symoid_tree, symoid);
RB_PROTOTYPE(symoid_tree, symoid, link, symoid_cmp);

#define SCHEMA_MAXPUSHBACK	128

struct schema
{
	struct attr_type_tree	 attr_types;
	struct oidname_tree	 attr_names;
	struct object_tree	 objects;
	struct oidname_tree	 object_names;
	struct symoid_tree		 symbolic_oids;

	FILE			*fp;
	const char		*filename;
	char			 pushback_buffer[SCHEMA_MAXPUSHBACK];
	int			 pushback_index;
	int			 lineno;
	int			 error;
};

struct schema		*schema_new(void);
int			 schema_parse(struct schema *schema,
			    const char *filename);

struct attr_type	*lookup_attribute_by_oid(struct schema *schema, char *oid);
struct attr_type	*lookup_attribute_by_name(struct schema *schema, char *name);
struct attr_type	*lookup_attribute(struct schema *schema, char *oid_or_name);
struct object		*lookup_object_by_oid(struct schema *schema, char *oid);
struct object		*lookup_object_by_name(struct schema *schema, char *name);
struct object		*lookup_object(struct schema *schema, char *oid_or_name);
int			 is_oidstr(const char *oidstr);

#endif

