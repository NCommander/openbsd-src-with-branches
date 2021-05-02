/*	$OpenBSD: exceptions.cc,v 1.1 2007/01/28 19:10:06 kettenis Exp $	*/
/*
 *	Written by Otto Moerbeek <otto@drijf.net> 2021 Public Domain
 */

#include <string>
#include <iostream>
#include <err.h>
#include <pthread.h>

void
a()
{
	try {
		throw std::string("foo");
        }
	catch (const std::string& ex) {
		if (ex != "foo")
			errx(1, "foo");
	}
}

void
b()
{
	a();
}

void *
c(void *)
{
	b();
	return nullptr;
}

#define N 100

int
main()
{
	int i;
	pthread_t p[N];

	for (i = 0; i < N; i++)
		if (pthread_create(&p[i], nullptr, c, nullptr) != 0)
			err(1, nullptr);
	for (i = 0; i < N; i++)
		if (pthread_join(p[i], nullptr) != 0)
			err(1, nullptr);
	std::cout << ".";
	return 0;
}
