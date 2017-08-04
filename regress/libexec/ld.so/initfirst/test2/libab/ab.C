/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: ab.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <cstdio>

class AB
{
	public:
		AB();
		~AB();
};

AB::AB()
{
   std::printf("B");
}

AB::~AB()
{
   std::printf("b");
}

AB b;
