/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD$
 */

#include <iostream>

class AA
{
	public:
		AA();
		~AA();
};

AA::AA()
{
   std::cout << "A";
}

AA::~AA()
{
   std::cout << "a";
}

AA a;
