/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ad.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <cstdio>

class AD
{
	public:
		AD();
		~AD();
};

AD::AD()
{
   std::printf("D");
}

AD::~AD()
{
   std::printf("d");
}

AD d;
