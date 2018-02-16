/*                                                                  -*- c++ -*-
 * Copyright (c) 2018 Ron R Wills <ron.rwsoft@gmail.com>
 *
 * This file is part of the Curses C++ Library.
 *
 * Curses C++ Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Meat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Network Streams Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "curses"
#include <stdarg.h>

int curses::Window::printw(const char *mesg, ...) {
	va_list args;
	va_start(args, mesg);

	int result = ::vwprintw(_win, mesg, args);

	va_end(args);
  return result;
}

int curses::Window::printw(int y, int x, const char *mesg, ...) {
	va_list args;
	va_start(args, mesg);

	::wmove(_win, y, x);
	int result = ::vwprintw(_win, mesg, args);

	va_end(args);
  return result;
}
