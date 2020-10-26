/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ac.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <cstdio>

class AC
{
	public:
		AC();
		~AC();
};

AC::AC()
{
   std::printf("C");
}

AC::~AC()
{
   std::printf("c");
}

AC c;
