/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD$
 */

#include <iostream>

class P
{
	public:
		P();
		~P();
};

P::P()
{
	std::cout << "P";
}

P::~P()
{
	std::cout << "p";
}

P p;

main()
{
	return 0;
}
