/*
 * $Id: loadalias.h,v 1.3 1997/10/26 01:03:02 brian Exp $
 */

struct aliasHandlers {
  char *(*PacketAliasGetFragment)(char *);
  void (*PacketAliasInit)(void);
  int (*PacketAliasIn)(char *, int);
  int (*PacketAliasOut)(char *, int);
  struct alias_link *(*PacketAliasRedirectAddr)(struct in_addr, struct in_addr);
  struct alias_link *(*PacketAliasRedirectPort)
    (struct in_addr, u_short, struct in_addr, u_short,
     struct in_addr, u_short, u_char);
  int (*PacketAliasSaveFragment)(char *);
  void (*PacketAliasSetAddress)(struct in_addr);
  unsigned (*PacketAliasSetMode)(unsigned, unsigned);
  void (*PacketAliasFragmentIn)(char *, char *);
};

extern int loadAliasHandlers(struct aliasHandlers *);
extern void unloadAliasHandlers(void);
