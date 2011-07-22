#! /bin/sh
# $Id: mkdirs.sh 1.6 Thu, 17 May 2007 15:53:22 -0700 dickey $
# -----------------------------------------------------------------------------
# mkinstalldirs --- make directory hierarchy
# Author: Noah Friedman <friedman@prep.ai.mit.edu>
# Created: 1993-05-16
# Last modified: 1994-03-25
# Public domain
# -----------------------------------------------------------------------------

errstatus=0
umask 022

for file in ${1+"$@"} ; do
   set fnord `echo ":$file" | sed -ne 's/^:\//#/;s/^://;s/\// /g;s/^#/\//;p'`
   shift

   pathcomp=
   for d in ${1+"$@"} ; do
     pathcomp="$pathcomp$d"
     case "$pathcomp" in
       -* ) pathcomp=./$pathcomp ;;
     esac

     if test ! -d "$pathcomp"; then
        echo "mkdir $pathcomp" 1>&2
        case "$pathcomp" in
          [abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ]: )
            ;;               # DOSISH systems
          * )
            mkdir "$pathcomp"
            errstatus=$?
            if test $errstatus != 0
            then
               # may have failed if invoked in a parallel "make -j# install"
               if test -d "$pathcomp"
               then
                  errstatus=0
               fi
            fi
            ;;
        esac
     fi

     pathcomp="$pathcomp/"
   done
done

exit $errstatus

# mkinstalldirs ends here
