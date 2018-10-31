/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ae.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <cstdio>

class AE
{
	public:
		AE();
		~AE();
};

AE::AE()
{
   std::printf("E");
}

AE::~AE()
{
   std::printf("e");
}

AE e;
