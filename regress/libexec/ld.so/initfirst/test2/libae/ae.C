/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD$
 */

#include <iostream>

class AE
{
	public:
		AE();
		~AE();
};

AE::AE()
{
   std::cout << "E";
}

AE::~AE()
{
   std::cout << "e";
}

AE e;
