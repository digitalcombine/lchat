# Local Chat Suite

The local chat suite is a very simple *'irc-ish'* chat system over a unix socket
for a single computer/server. The target for this software is for headless
servers allowing admins or other users to easily communicate on the console.

## Requirements

GNU Automake & Autoconf tools
GNU C++ Compiler Suite
Curses Library

## Install

Building and installing uses the standard GNU automake/autoconf tools. So the
following configure/make will work.

```shell
./configure
make
make check
make install
```

The above will work, but ideally the following compiling flags shown be set
during the configure stage.

```shell
CXXCFLAGS="-Wall -pedantic -O2" ./configure
```

## Usage

First the chat daemon need to be started. Ideally it should be started with it's
own system user and all the users that will be using the chat system should have
a common group.

To start the server as a daemon with a unix socket at `/var/lib/lchat/sock` for
the group *'users'*.

```shell
lchatd -d -g users
lchatd -d -s /var/lib/lchat/sock -g users
```

To use that chat system any user in the *'users'* group can now simply use the
lchat program.

```shell
lchat
lchat -s /var/lib/lchat/sock
```

## License

Copyright (c) 2018-2020 Ron R Wills <ron@digitalcombine.ca>

Meat is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Meat is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Meat.  If not, see <http://www.gnu.org/licenses/>.
