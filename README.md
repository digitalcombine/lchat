# Local Chat Suite

The local chat suite is a very simple *'irc-ish'* chat system over a unix socket
for a single computer/server. The target for this software is for headless
servers allowing admins or other users to easily communicate on the console.

## Requirements

* GNU Automake & Autoconf tools
* GNU C++ Compiler Suite
* Curses Library

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

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
