#include <stdio.h>
#include "ber.h"

#define LDAP_URL "ldap://"
#define LDAP_PORT 389

struct aldap {
	int		msgid;
	struct ber	ber;
};

struct aldap_message {
	int msgid;
	int message_type;

	struct ber_element	*msg;

	struct ber_element	*header;
	struct ber_element	*protocol_op;

	struct ber_element	*dn;

	union {
		struct {
			long long		 rescode;
			struct ber_element	*diagmsg;
		}			 res;
		struct {
			struct ber_element	*iter;
			struct ber_element	*entries;
		}			 search;
		struct ber_element	*references;
	} body;
};

enum aldap_protocol {
	LDAP,
	LDAPS
};

struct aldap_url {
	int	protocol;
	char	*host;
	int	 port;
	char	*dn;
#define MAXATTR 1024
	char	*attributes[MAXATTR];
	int	 scope;
	char	*filter;
};

enum protocol_op {
	LDAP_REQ_BIND		= 0,
	LDAP_RES_BIND		= 1,
	LDAP_REQ_UNBIND_30	= 2,
	LDAP_REQ_SEARCH		= 3,
	LDAP_RES_SEARCH_ENTRY	= 4,
	LDAP_RES_SEARCH_RESULT	= 5,
	LDAP_REQ_MODIFY		= 6,
	LDAP_RES_MODIFY		= 7,
	LDAP_REQ_ADD		= 8,
	LDAP_RES_ADD		= 9,
	LDAP_REQ_DELETE_30	= 10,
	LDAP_RES_DELETE		= 11,
	LDAP_REQ_MODRDN		= 12,
	LDAP_RES_MODRDN		= 13,
	LDAP_REQ_COMPARE	= 14,
	LDAP_RES_COMPARE	= 15,
	LDAP_REQ_ABANDON_30	= 16,

	LDAP_RES_SEARCH_REFERENCE = 19,
};

enum deref_aliases {
	LDAP_DEREF_NEVER	= 0,
	LDAP_DEREF_SEARCHING	= 1,
	LDAP_DEREF_FINDING	= 2,
	LDAP_DEREF_ALWAYS	= 3,
};

enum authentication_choice {
	LDAP_AUTH_SIMPLE	= 0,
};

enum scope {
	LDAP_SCOPE_BASE		= 0,
	LDAP_SCOPE_ONELEVEL	= 1,
	LDAP_SCOPE_SUBTREE	= 2,
};

enum result_code {
	LDAP_SUCCESS				= 0,
	LDAP_OPERATIONS_ERROR			= 1,
	LDAP_PROTOCOL_ERROR			= 2,
	LDAP_TIMELIMIT_EXCEEDED			= 3,
	LDAP_SIZELIMIT_EXCEEDED			= 4,
	LDAP_COMPARE_FALSE			= 5,
	LDAP_COMPARE_TRUE			= 6,
	LDAP_STRONG_AUTH_NOT_SUPPORTED		= 7,
	LDAP_STRONG_AUTH_REQUIRED		= 8,

	LDAP_REFERRAL				= 10,
	LDAP_ADMINLIMIT_EXCEEDED		= 11,
	LDAP_UNAVAILABLE_CRITICAL_EXTENSION	= 12,
	LDAP_CONFIDENTIALITY_REQUIRED		= 13,
	LDAP_SASL_BIND_IN_PROGRESS		= 14,
	LDAP_NO_SUCH_ATTRIBUTE			= 16,
	LDAP_UNDEFINED_TYPE			= 17,
	LDAP_INAPPROPRIATE_MATCHING		= 18,
	LDAP_CONSTRAINT_VIOLATION		= 19,
	LDAP_TYPE_OR_VALUE_EXISTS		= 20,
	LDAP_INVALID_SYNTAX			= 21,

	LDAP_NO_SUCH_OBJECT			= 32,
	LDAP_ALIAS_PROBLEM			= 33,
	LDAP_INVALID_DN_SYNTAX			= 34,

	LDAP_ALIAS_DEREF_PROBLEM		= 36,

	LDAP_INAPPROPRIATE_AUTH			= 48,
	LDAP_INVALID_CREDENTIALS		= 49,
	LDAP_INSUFFICIENT_ACCESS		= 50,
	LDAP_BUSY				= 51,
	LDAP_UNAVAILABLE			= 52,
	LDAP_UNWILLING_TO_PERFORM		= 53,
	LDAP_LOOP_DETECT			= 54,

	LDAP_NAMING_VIOLATION			= 64,
	LDAP_OBJECT_CLASS_VIOLATION		= 65,
	LDAP_NOT_ALLOWED_ON_NONLEAF		= 66,
	LDAP_NOT_ALLOWED_ON_RDN			= 67,
	LDAP_ALREADY_EXISTS			= 68,
	LDAP_NO_OBJECT_CLASS_MODS		= 69,

	LDAP_AFFECTS_MULTIPLE_DSAS		= 71,

	LDAP_OTHER				= 80,
};

enum filter {
	LDAP_FILT_AND		= 0,
	LDAP_FILT_OR		= 1,
	LDAP_FILT_NOT		= 2,
	LDAP_FILT_EQ		= 3,
	LDAP_FILT_SUBS		= 4,
	LDAP_FILT_GE		= 5,
	LDAP_FILT_LE		= 6,
	LDAP_FILT_PRES		= 7,
	LDAP_FILT_APPR		= 8,
};

enum subfilter {
	LDAP_FILT_SUBS_INIT	= 0,
	LDAP_FILT_SUBS_ANY	= 1,
	LDAP_FILT_SUBS_FIN	= 2,
};

struct aldap		*aldap_init(int fd);
struct aldap_message	*aldap_parse(struct aldap *);
void			 aldap_freemsg(struct aldap_message *);

int	 aldap_bind(struct aldap *, char *, char *);
int	 aldap_unbind(struct aldap *);
int	 aldap_search(struct aldap *, char *, enum scope, char *, char **, int, int, int);

int	 aldap_get_resultcode(struct aldap_message *);
char	*aldap_get_dn(struct aldap_message *);
char	*aldap_get_diagmsg(struct aldap_message *);
char	**aldap_get_references(struct aldap_message *);
void	 aldap_free_references(char **values);
int	 aldap_parse_url(char *, struct aldap_url *);
void	 aldap_free_url(struct aldap_url *);

int	 aldap_count_entries(struct aldap_message *);
int	 aldap_match_entry(struct aldap_message *, char *, char ***);
int	 aldap_first_entry(struct aldap_message *, char **, char ***);
int	 aldap_next_entry(struct aldap_message *, char **, char ***);
int	 aldap_free_entry(char **);
