/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ac.h,v 1.1 2005/09/17 02:58:54 drahn Exp $
 */

class AC {
public:
        AC(const char *);
        ~AC();
private:
	const char *_name;
};

