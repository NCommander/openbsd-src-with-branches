/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD$
 */

#include <iostream>

class AC
{
	public:
		AC();
		~AC();
};

AC::AC()
{
   std::cout << "C";
}

AC::~AC()
{
   std::cout << "c";
}

AC c;
