// Tests for the -*- C++ -*- string classes.
// Copyright (C) 1994 Free Software Foundation

// This file is part of the GNU ANSI C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software
// Foundation; either version 2, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
#include <std/string.h>
#include <iostream.h>
#include <std/cstdlib.h>
#include <std/cassert.h>

string X = "Hello";
string Y = "world";
string N = "123";
string c;
char*  s = ",";

void decltest()
{
  string x;
  cout << "an empty string:" << x << "\n";
  assert(x == "");

  string y = "Hello";
  cout << "A string initialized to Hello:" << y << "\n";
  assert(y == "Hello");

  if (y[y.length()-1] == 'o')
	y = y + '\n';
  assert(y == "Hello\n");
  y = "Hello";

  string a = y;
  cout << "A string initialized to previous string:" << a << "\n";
  assert(a == "Hello");
  assert(a == y);

  string b (a, 1, 2);
  cout << "A string initialized to (previous string, 1, 2):" << b << "\n";
  assert(b == "el");

  char ch = '@';
  string z (1, ch);
  cout << "A string initialized to @:" << z << "\n";
  assert (z == "@");

  string n ("20");
  cout << "A string initialized to 20:" << n << "\n";
  assert(n == "20");

  int i = atoi(n.c_str ());
  double f = atof(n.c_str ());
  cout << "n = " << n << " atoi(n) = " << i << " atof(n) = " << f << "\n";
  assert(i == 20);
  assert(f == 20);

}

void cattest()
{
  string x = X;
  string y = Y;
  string z = x + y;
  cout << "z = x + y = " << z << "\n";
  assert(z == "Helloworld");

  x += y;
  cout << "x += y; x = " << x << "\n";
  assert(x == "Helloworld");

  y = Y;
  x = X;
  y.insert (0, x);
  cout << "y.insert (0, x); y = " << y << "\n";
  assert(y == "Helloworld");

  y = Y;
  x = X;
  x = x + y + x;
  cout << "x = x + y + x; x = " << x << "\n";
  assert(x == "HelloworldHello");

  y = Y;
  x = X;
  x = y + x + x;
  cout << "x = y + x + x; x = " << x << "\n";
  assert(x == "worldHelloHello");

  x = X;
  y = Y;
  z = x + s + ' ' + y.substr (y.find ('w'), 1) + y.substr (y.find ('w') + 1) + ".";
  cout << "z = x + s +  + y.substr (y.find (w), 1) + y.substr (y.find (w) + 1) + . = " << z << "\n";
  assert(z == "Hello, world.");
}

void comparetest()
{  
  string x = X;
  string y = Y;
  string n = N;
  string z = x + y;

  assert(x != y);
  assert(x == "Hello");
  assert(x != z.substr (0, 4));
  assert(x.compare (y) < 0);
  assert(x.compare (z.substr (0, 6)) < 0);

  assert(x.find ("lo") == 3);
  assert(x.find ("l", 2) == 2);
  assert(x.rfind ("l") == 3);
}

void substrtest()
{
  string x = X;

  char ch = x[0];
  cout << "ch = x[0] = " << ch << "\n";
  assert(ch == 'H');

  string z = x.substr (2, 3);
  cout << "z = x.substr (2, 3) = " << z << "\n";
  assert(z == "llo");

  x.replace (2, 2, "r");
  cout << "x.replace (2, 2, r); x = " << x << "\n";
  assert(x == "Hero");

  x = X;
  x.replace (0, 1, 'j');
  cout << "x.replace (0, 1, 'j'); x = " << x << "\n";
  assert(x == "jello");
}

void iotest()
{
  string z;
  cout << "enter a word:";
  cin >> z;
  cout << "word =" << z << " ";
  cout << "length = " << z.length() << "\n";
}

void identitytest(string a, string b)
{
  string x = a;
  string y = b;
  x += b;
  y.insert (0, a);
  assert((a + b) == x);
  assert((a + b) == y);
  assert(x == y);
  
  assert((a + b + a) == (a + (b + a)));

  x.remove (x.rfind (b));
  assert(x == a);

  y.replace (0, y.rfind (b), b);
  assert(y == (b + b));
  y.replace (y.find (b), b.length (), a);
  assert(y == (a + b));
}

int main()
{
  decltest();
  cattest();
  comparetest();
  substrtest();
  identitytest(X, X);
  identitytest(X, Y);
  identitytest(X+Y+N+X+Y+N, "A string that will be used in identitytest but is otherwise just another useless string.");
  iotest();
  cout << "\nEnd of test\n";
  return 0;
}
