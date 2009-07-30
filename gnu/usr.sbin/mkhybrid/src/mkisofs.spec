# $Id: mkisofs.spec,v 1.6 1999/03/02 03:41:26 eric Exp $
Summary: Creates a ISO9660 filesystem image
Name: mkisofs
Version: 1.12b5
Release: 1
Copyright: GPL
Group: Utilities/System
Source: tsx-11.mit.edu:/pub/linux/packages/mkisofs/mkisofs-1.12b5.tar.gz

%description
This is the mkisofs package.  It is used to create ISO 9660
file system images for creating CD-ROMs. Now includes support
for making bootable "El Torito" CD-ROMs.

%prep
%setup

%build
./configure --prefix=/usr
make

%install
make install
strip /usr/bin/mkisofs

%changelog

* Tue Feb 25 1997 Michael Fulbright <msf@redhat.com>

 Updated to 1.10b7.

* Wed Feb 12 1997 Michael Fulbright <msf@redhat.com>

 Updated to 1.10b3.

* Wed Feb 12 1997 Michael Fulbright <msf@redhat.com>

 Added %doc line to spec file (was missing all docs before).

%files
%doc COPYING ChangeLog README README.eltorito TODO
/usr/bin/mkisofs
/usr/man/man8/mkisofs.8

