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
#include <cstring>
#include <stdarg.h>

/*****************************************************************************
 * class curs::terminal
 */

/*******************************
 * static curs::terminal::type *
 *******************************/

std::string curs::terminal::type() {
  return ttytype;
}

/****************************
 * curs::terminal::terminal *
 ****************************/

curs::terminal::terminal() {
  ::initscr();
  ::use_default_colors();
  ::assume_default_colors(-1,-1);
}

/*****************************
 * curs::terminal::~terminal *
 *****************************/

curs::terminal::~terminal() {
  ::endwin();
  ::refresh();
}

/**************************
 * curs::terminal::cbreak *
 **************************/

void curs::terminal::cbreak(bool value) {
  if (value) ::cbreak();
  else ::nocbreak();
}

/************************
 * curs::terminal::echo *
 ************************/

void curs::terminal::echo(bool value) {
  if (value) ::echo();
  else ::noecho();
}

/*******************************
 * curs::terminal::show_cursor *
 *******************************/

void curs::terminal::show_cursor(bool value) {
  if (value ) ::curs_set(0);
  else :: curs_set(1);
}

/*****************************************************************************
 * class curs::windowbuf
 */

/******************************
 * curs::windowbuf::windowbuf *
 ******************************/

curs::windowbuf::windowbuf(WINDOW *win, bool free_window , size_t buffer)
  : _window(win), _free_window(free_window), _obuf(buffer) {

  if (win == nullptr)
    throw std::runtime_error("Null curses window");

  // Setup the stream buffers.
  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);
}

curs::windowbuf::~windowbuf() noexcept {
  if (_free_window) ::delwin(_window);
}

/*****************************
 * curs::windowbuf::overflow *
 *****************************/

curs::windowbuf::int_type curs::windowbuf::overflow(int_type ch) {
  if (ch != traits_type::eof()) {
    *pptr() = ch;
    pbump(1);
  }

  // The buffer is full, so time to flush it.
  if (pptr() > epptr())
  if (not oflush()) return traits_type::eof();

  return ch;
}

/*************************
 * curs::windowbuf::sync *
 *************************/

int curs::windowbuf::sync() {
  return (oflush() ? 0 : -1);
}

/***************************
 * curs::windowbuf::oflush *
 ***************************/

bool curs::windowbuf::oflush() {
  int wlen = pptr() - pbase();
  char *buf = pbase();

  if (wlen > 0) {
    int result = ::waddnstr(_window, buf, wlen);
    if (result == ERR) return false;

    wnoutrefresh(_window);
  }

  // Reset the buffers.
  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);

  return true;
}

/*****************************************************************************
 * class curs::padbuf
 */

/************************
 * curs::padbuf::padbuf *
 ************************/

curs::padbuf::padbuf(int width, int height, size_t buffer)
  : _pad(::newpad(height, width)), x(0), y(0),
    _obuf(buffer) {

  // Setup the stream buffers.
  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);
}

/*************************
 * curs::padbuf::~padbuf *
 *************************/

curs::padbuf::~padbuf() noexcept {
  ::delwin(_pad);
}

/**************************
 * curs::padbuf::overflow *
 **************************/

curs::padbuf::int_type curs::padbuf::overflow(int_type ch) {
  if (ch != traits_type::eof()) {
    *pptr() = ch;
    pbump(1);
  }

  // The buffer is full, so time to flush it.
  if (pptr() > epptr())
    if (not oflush()) return traits_type::eof();

  return ch;
}

/**********************
 * curs::padbuf::sync *
 **********************/

int curs::padbuf::sync() {
  return (oflush() ? 0 : -1);
}

/************************
 * curs::padbuf::oflush *
 ************************/

bool curs::padbuf::oflush() {
  int wlen = pptr() - pbase();
  char *buf = pbase();

  if (wlen > 0) {
    int result = ::waddnstr(_pad, buf, wlen);
    if (result == ERR) return false;

    pnoutrefresh(_pad, y, x, dest_y, dest_x,
                 dest_y + dest_height, dest_x + dest_width);
  }

  // Reset the buffers.
  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);

  return true;
}

/*****************************************************************************
 * class curs::window
 */

/************************
 * curs::window::window *
 ************************/

curs::window::window() : std::ostream(&_windowbuf), _windowbuf(::stdscr) {
}

curs::window::window(int x, int y, int width, int height)
  : std::ostream(&_windowbuf),
  _windowbuf(::newwin(height, width, y, x), true) {
}

curs::window::window(const window &parent, int x, int y, int width, int height)
  : std::ostream(&_windowbuf),
  _windowbuf(::derwin(parent._windowbuf.window(), height, width, y, x),
             true) {
  ::touchwin(parent._windowbuf.window());
}

/*************************
 * curs::window::~window *
 *************************/

curs::window::~window() noexcept {
}

/************************
 * curs::window::cursor *
 ************************/

void curs::window::cursor(int &x, int &y) const {
  ::getyx(_windowbuf.window(), y, x);
}

/**********************
 * curs::window::size *
 **********************/

void curs::window::size(int &width, int &height) const {
  ::getmaxyx(_windowbuf.window(), height, width);
}

/**************************
 * curs::window::position *
 **************************/

void curs::window::position(int &x, int &y) const {
  ::getparyx(_windowbuf.window(), y, x);
  if (x == -1 or y == -1)
    ::getbegyx(_windowbuf.window(), y, x);
}

/***********************
 * curs::window::width *
 ***********************/

int curs::window::width() const {
  int w, h;
  size(w, h);
  return w;
}

/************************
 * curs::window::height *
 ************************/

int curs::window::height() const {
  int w, h;
  size(w, h);
  return h;
}

/*******************
 * curs::window::x *
 *******************/

int curs::window::x() const {
  int _x, _y;
  position(_x, _y);
  return _x;
}

/*******************
 * curs::window::y *
 *******************/

int curs::window::y() const {
  int _x, _y;
  position(_x, _y);
  return _y;
}

/*****************************************************************************
 */

std::ostream &curs::clear(std::ostream &os) {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wclear(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::erase(std::ostream &os) {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::werase(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::clrtobot(std::ostream &os) {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wclrtobot(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::clrtoeol(std::ostream &os) {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wclrtoeol(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::touch(std::ostream &os) {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    //os << std::flush;
    ::touchwin(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::noutrefresh(std::ostream &os) {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wnoutrefresh(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::keypad::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL)
    ::keypad(osptr->_windowbuf.window(), _use);
  return os;
}

std::ostream &curs::nodelay::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL)
    ::nodelay(osptr->_windowbuf.window(), _value);
  return os;
}

std::ostream &curs::scrollok::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL)
    ::scrollok(osptr->_windowbuf.window(), _value);
  return os;
}

std::ostream &curs::attron::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattron(osptr->_windowbuf.window(), _attr);
  }
  return os;
}

std::ostream &curs::attroff::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattroff(osptr->_windowbuf.window(), _attr);
  }
  return os;
}

std::ostream &curs::attrset::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattrset(osptr->_windowbuf.window(), _attr);
  }
  return os;
}

std::ostream &curs::bkgd::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;

    cchar_t c;
    std::memset(&c, 0, sizeof(c));
    c.attr = _attr;
    c.chars[0] = _character;
    ::wbkgrnd(osptr->_windowbuf.window(), &c);
    ::wnoutrefresh(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::cursor::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    if (_op == POSITION)
      ::wmove(osptr->_windowbuf.window(), _y, _x);
    if (_op == VISIBLITY)
      if (_show) ::curs_set(0);
      else ::curs_set(1);
  }
  return os;
}

std::ostream &curs::hline::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::whline(osptr->_windowbuf.window(), _character, _length);
    ::wnoutrefresh(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::vline::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wvline(osptr->_windowbuf.window(), _character, _length);
    ::wnoutrefresh(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::box::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::box(osptr->_windowbuf.window(), _verch, _horch);
    ::wnoutrefresh(osptr->_windowbuf.window());
  }
  return os;
}

std::ostream &curs::border::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    wborder(osptr->_windowbuf.window(), _ls, _rs, _ts, _bs, _tl, _tr, _bl,
            _br);
  }
  return os;
}

std::ostream &curs::resize::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wresize(osptr->_windowbuf.window(), _height, _width);
  }
  return os;
}

std::ostream &curs::move::operator()(std::ostream &os) const {
  window *osptr = dynamic_cast<window *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::mvwin(osptr->_windowbuf.window(), _y, _x);
  }
  return os;
}

/***************
 * operator << *
 ***************/

std::ostream &operator <<(std::ostream &ios, const curs::osmanip &manip) {
  return manip(ios);
}

/*****************************************************************************
 * class curs::palette
 */

/*****************************
 * curs::palette::has_colors *
 *****************************/

bool curs::palette::has_colors() {
  return (::has_colors() == TRUE);
}

/***********************************
 * curs::palette::can_change_color *
 ***********************************/

bool curs::palette::can_change_color() {
  return (::can_change_color() == TRUE);
}

/*****************************************************************************
 * class curs::keyboard_event_hander
 */

static curs::keyboard_event_hander *_focused = nullptr;

curs::keyboard_event_hander::keyboard_event_hander() {}

void curs::keyboard_event_hander::focus() {
  if (_focused != nullptr)
    _focused->lose_focus();

  _focused = this;

  gain_focus();
}

bool curs::keyboard_event_hander::has_focus() const {
}

/*****************************************************************************
 * class curs::mouse_event_hander
 */

static std::vector<curs::mouse_event_handler *> mouse_handlers;

curs::mouse_event_handler::mouse_event_handler() {
  mouse_handlers.push_back(this);
  mouse_handlers.shrink_to_fit();
}

curs::mouse_event_handler::~mouse_event_handler() noexcept {
  auto it = mouse_handlers.begin();
  for (; it != mouse_handlers.end(); ++it) {
    if (*it == this) {
      mouse_handlers.erase(it);
      break;
    }
  }
}

static bool _do_events = true;

/**********************
 * curs::events::main *
 **********************/

void curs::events::main() {

  int c;
  MEVENT event;

  while (_do_events) {
    c = ::getch();
    if (c == KEY_MOUSE) {
      if (getmouse(&event) == OK) {
        for (auto &evt: mouse_handlers)
          evt->event(event.id, event.x, event.y, event.bstate);
      }
    } else if (_focused != nullptr) {
      _focused->event(c);
    }
  }
}

/**********************
 * curs::events::quit *
 **********************/

void curs::events::quit() {
  _do_events = false;
}
