/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD$
 */

#include <iostream>

class AB
{
	public:
		AB();
		~AB();
};

AB::AB()
{
   std::cout << "B";
}

AB::~AB()
{
   std::cout << "b";
}

AB b;
