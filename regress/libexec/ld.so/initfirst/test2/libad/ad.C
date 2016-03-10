/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD$
 */

#include <iostream>

class AD
{
	public:
		AD();
		~AD();
};

AD::AD()
{
   std::cout << "D";
}

AD::~AD()
{
   std::cout << "d";
}

AD d;
