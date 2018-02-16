#! /bin/sh
#
#  Initialize the automake/autoconf build framework for the script virtual
# machine.
#
# Written by Ron R Wills

aclocal
#libtoolize --force
automake -a -c --foreign
autoconf
