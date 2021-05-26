/*                                                                  -*- c++ -*-
 * Copyright © 2018-2021 Ron R Wills <ron@digitalcombine.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "curses"
#include <term.h>
#include <cstring>
#include <stdarg.h>
#include <clocale>

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

#ifdef DEBUG
#include <fstream>

static std::ofstream debug_log;
#endif

/*****************************************************************************
 * class curs::terminal
 */

/*******************************
 * static curs::terminal::type *
 *******************************/

std::string curs::terminal::type() {
  return ttytype;
}

/*************************
 * curs::terminal::clear *
 *************************/

void curs::terminal::clear() {
#ifdef DEBUG
  //debug_log << "TERM: clear" << std::endl;
#endif
  ::clear();
}

/**************************
 * curs::terminal::update *
 **************************/

void curs::terminal::update() {
#ifdef DEBUG
  //debug_log << "TERM: update" << std::endl;
#endif
  ::doupdate();
  //::refresh();
}

/**************************
 * curs::terminal::cbreak *
 **************************/

void curs::terminal::cbreak(bool value) {
#ifdef DEBUG
  debug_log << "TERM: cbreak = " << std::boolalpha << value
            << std::endl;
#endif
  if (value) ::cbreak();
  else ::nocbreak();
}

/***********************
 * curs::terminal::raw *
 ***********************/

void curs::terminal::raw(bool value) {
#ifdef DEBUG
  debug_log << "TERM: raw = " << std::boolalpha << value
            << std::endl;
#endif
  if (value) ::raw();
  else ::noraw();
}

/************************
 * curs::terminal::echo *
 ************************/

void curs::terminal::echo(bool value) {
#ifdef DEBUG
  debug_log << "TERM: echo = " << std::boolalpha << value
            << std::endl;
#endif
  if (value) ::echo();
  else ::noecho();
}

/**************************
 * curs::terminal::cursor *
 **************************/

void curs::terminal::cursor(bool show) {
#ifdef DEBUG
  debug_log << "TERM: cursor = " << std::boolalpha << show
            << std::endl;
#endif
  if (show) ::curs_set(1);
  else :: curs_set(0);
}

/****************************
 * curs::terminal::terminal *
 ****************************/

curs::terminal::terminal() : screen(nullptr) {
#ifdef DEBUG
  if (not debug_log.is_open())
      debug_log.open("curses.log");
  debug_log << "TERM: Initializing" << std::endl;
#endif
  //std::setlocale(LC_ALL, "");
  ::initscr();

  screen = set_term(nullptr);
  set_term(screen);

  ::use_default_colors();
  ::assume_default_colors(-1, -1);
}

curs::terminal::terminal(int outfd, int infd) : screen(nullptr) {
#ifdef DEBUG
  if (not debug_log.is_open())
      debug_log.open("curses.log");
  debug_log << "TERM: New terminal" << std::endl;
#endif

  screen = newterm(nullptr, fdopen(outfd, "w"), fdopen(infd, "r"));
  if (not screen)
    throw std::runtime_error("Unable to allocate new terminal");

  ::use_default_colors();
  ::assume_default_colors(-1, -1);
}

curs::terminal::terminal(FILE *outfile, FILE *infile) : screen(nullptr) {
#ifdef DEBUG
  if (not debug_log.is_open())
      debug_log.open("curses.log");
  debug_log << "TERM: New terminal" << std::endl;
#endif

  screen = newterm(nullptr, outfile, infile);
  if (not screen)
    throw std::runtime_error("Unable to allocate new terminal");

  ::use_default_colors();
  ::assume_default_colors(-1, -1);
}

curs::terminal::terminal(const std::string &term, int outfd, int infd) {
#ifdef DEBUG
  if (not debug_log.is_open())
      debug_log.open("curses.log");
  debug_log << "TERM: New terminal" << std::endl;
#endif

  screen = newterm(term.c_str(), fdopen(outfd, "w"), fdopen(infd, "r"));
  if (not screen)
    throw std::runtime_error("Unable to allocate new terminal");

  ::use_default_colors();
  ::assume_default_colors(-1, -1);
}

curs::terminal::terminal(const std::string &term, FILE *outfile, FILE *infile) {
#ifdef DEBUG
  if (not debug_log.is_open())
      debug_log.open("curses.log");
  debug_log << "TERM: New terminal" << std::endl;
#endif

  screen = newterm(term.c_str(), outfile, infile);
  if (not screen)
    throw std::runtime_error("Unable to allocate new terminal");

  ::use_default_colors();
  ::assume_default_colors(-1, -1);
}

/*****************************
 * curs::terminal::~terminal *
 *****************************/

curs::terminal::~terminal() noexcept {
  if (screen) ::set_term(screen);
  ::endwin();
  if (screen) ::delscreen(screen);
}

/***********************
 * curs::terminal::set *
 ***********************/

void curs::terminal::set() const {
  set_term(screen);
}

/*****************************************************************************
 * class curs::windowbuf
 */

/******************************
 * curs::windowbuf::windowbuf *
 ******************************/

curs::windowbuf::windowbuf(WINDOW *win, bool free_window , size_t buffer)
  : _window(win), _free_window(free_window), _use_stdscr(false),
    _obuf(buffer) {
  if (win == nullptr) {
    throw std::runtime_error("Null curses window");
  }

  if (win == ::stdscr) _use_stdscr = true;

#ifdef DEBUG
  debug_log << "WIN: new " << (void *)_window << std::endl;
#endif

  // Setup the stream buffers.
  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);
}

/*******************************
 * curs::windowbuf::~windowbuf *
 *******************************/

curs::windowbuf::~windowbuf() noexcept {
  if (_free_window and not _use_stdscr) {
#ifdef DEBUG
    debug_log << "WIN: free " << (void *)_window << std::endl;
#endif
    ::delwin(_window);
  }
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
    if (_use_stdscr) {
      int result = ::waddnstr(::stdscr, buf, wlen);
      if (result == ERR) return false;

      ::wnoutrefresh(::stdscr);
    } else {
      int result = ::waddnstr(_window, buf, wlen);
      if (result == ERR) return false;

      ::wnoutrefresh(_window);
    }
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

curs::padbuf::padbuf(WINDOW *win, int x, int y, int width, int height, size_t buffer)
  : _pad(::subpad(win, height, width, y, x)), _x(x), _y(y),
    _obuf(buffer) {
}

curs::padbuf::padbuf(int width, int height, size_t buffer)
  : _pad(::newpad(height, width)), _x(0), _y(0),
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

    pnoutrefresh(_pad, _y, _x, dest_y, dest_x,
                 dest_y + dest_height, dest_x + dest_width);
  }

  // Reset the buffers.
  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);

  return true;
}

/*****************************************************************************
 * class curs::base_ostream
 */

/*************************************
 * curs::base_ostream::~base_ostream *
 *************************************/

curs::base_ostream::~base_ostream() noexcept {}

/******************************
 * curs::base_ostream::cursor *
 ******************************/

void curs::base_ostream::cursor(int &x, int &y) const {
  ::getyx(*this, y, x);
}

/****************************
 * curs::base_ostream::size *
 ****************************/

void curs::base_ostream::size(int &width, int &height) const {
  ::getmaxyx(*this, height, width);
}

/********************************
 * curs::base_ostream::position *
 ********************************/

void curs::base_ostream::position(int &x, int &y) const {
  ::getparyx(*this, y, x);
  if (x == -1 or y == -1)
    ::getbegyx(*this, y, x);
}

/*****************************
 * curs::base_ostream::width *
 *****************************/

int curs::base_ostream::width() const {
  int w, h;
  size(w, h);
  return w;
}

/******************************
 * curs::base_ostream::height *
 ******************************/

int curs::base_ostream::height() const {
  int w, h;
  size(w, h);
  return h;
}

/*************************
 * curs::base_ostream::x *
 *************************/

int curs::base_ostream::x() const {
  int _x, _y;
  position(_x, _y);
  return _x;
}

/*************************
 * curs::base_ostream::y *
 *************************/

int curs::base_ostream::y() const {
  int _x, _y;
  position(_x, _y);
  return _y;
}

/*****************************************************************************
 * class curs::window
 */

/************************
 * curs::window::window *
 ************************/

curs::window::window() : base_ostream(&_windowbuf), _windowbuf(::stdscr) {
#ifdef DEBUG
  debug_log << "WIN: Getting stdscr" << std::endl;
#endif
}

curs::window::window(int x, int y, int width, int height)
  : base_ostream(&_windowbuf),
  _windowbuf(::newwin(height, width, y, x), true) {
#ifdef DEBUG
  debug_log << "WIN: (" << x << ", " << y << ", " << width << ", "
            << height << ")" << std::endl;
#endif
}

curs::window::window(const window &parent, int x, int y, int width, int height)
  : base_ostream(&_windowbuf),
  _windowbuf(::derwin(parent._windowbuf.window(), height, width, y, x),
             true) {
  ::touchwin(parent._windowbuf.window());
}

/*************************
 * curs::window::~window *
 *************************/

curs::window::~window() noexcept {
}

/*************************
 * curs::window::cwindow *
 *************************/

curs::window::operator WINDOW *() const {
  return _windowbuf.window();
}

void curs::window::noutrefresh() const {
  ::wnoutrefresh(_windowbuf.window());
}

void curs::window::refresh() const {
  ::wrefresh(_windowbuf.window());
}

/*****************************************************************************
 * class curs::pad
 */

/******************
 * curs::pad::pad *
 ******************/

curs::pad::pad(int width, int height)
  : base_ostream(&_padbuf),
    _padbuf(width, height) {
#ifdef DEBUG
  debug_log << "PAD: (" << x << ", " << y << ", " << width << ", "
            << height << ")" << std::endl;
#endif
}

curs::pad::pad(const base_ostream &parent, int x, int y, int width, int height)
  : base_ostream(&_padbuf),
    _padbuf(parent, x, y, width, height) {
  ::touchwin(parent);
}

/*******************
 * curs::pad::~pad *
 *******************/

curs::pad::~pad() noexcept { }

curs::pad::operator WINDOW *() const {
  return _padbuf.window();
}

void curs::pad::noutrefresh() const {
  //::pnoutrefresh(cwindow());
}

void curs::pad::refresh() const {
  //::prefresh(cwindow());
}

/*****************************************************************************
 * Window IO Manipulators
 */

curs::osmanip::~osmanip() noexcept {}

curs::cchar::cchar(wchar_t ch) {
#ifdef HAVE_NCURSESW_H
  ::setcchar(&_character, &ch, 0, 0, nullptr);
#else
  _character = ch & 0xff;
#endif
}

curs::cchar::cchar(short color_pair, wchar_t ch) {
#ifdef HAVE_NCURSESW_H
  ::setcchar(&_character, &ch, 0, color_pair, nullptr);
#else
  _character = (ch & 0xff) | attr;
#endif
}

#ifdef HAVE_NCURSESW_H
curs::cchar::cchar(const cchar_t *ch) {
  if (ch == nullptr)
    std::memset(&_character, 0, sizeof(_character));
  else
    std::memcpy(&_character, ch, sizeof(_character));
}
#else
curs::cchar::cchar(chtype ch) {
  _character = ch;
}
#endif

curs::cchar::cchar(const cchar &other) {
#ifdef HAVE_NCURSESW_H
  std::memcpy(&_character, &other._character, sizeof(_character));
#else
  _character = other._character;
#endif
}

std::ostream &curs::clear(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wclear(*osptr);
  }
  return os;
}

std::ostream &curs::erase(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::werase(*osptr);
  }
  return os;
}

std::ostream &curs::clrtobot(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wclrtobot(*osptr);
  }
  return os;
}

std::ostream &curs::clrtoeol(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wclrtoeol(*osptr);
  }
  return os;
}

std::ostream &curs::touch(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    //os << std::flush;
    ::touchwin(*osptr);
  }
  return os;
}

std::ostream &curs::cursyncup(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    ::wcursyncup(*osptr);
  }
  return os;
}

std::ostream &curs::syncdown(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    ::wsyncdown(*osptr);
  }
  return os;
}

std::ostream &curs::syncup(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    ::wsyncup(*osptr);
  }
  return os;
}

std::ostream &curs::noutrefresh(std::ostream &os) {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != nullptr) {
    os << std::flush;
    osptr->noutrefresh();
  }
  return os;
}

std::ostream &curs::keypad::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL)
    ::keypad(*osptr, _use);
  return os;
}

std::ostream &curs::nodelay::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL)
    ::nodelay(*osptr, _value);
  return os;
}

std::ostream &curs::scrollok::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL)
    ::scrollok(*osptr, _value);
  return os;
}

std::ostream &curs::leaveok::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL)
    ::leaveok(*osptr, _value);
  return os;
}

std::ostream &curs::idlok::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL)
    ::idlok(*osptr, _value);
  return os;
}

std::ostream &curs::syncok::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL)
    ::syncok(*osptr, _value);
  return os;
}

std::ostream &curs::setscrreg::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL)
    ::wsetscrreg(*osptr, _top, _bottom);
  return os;
}

std::ostream &curs::attron::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattron(*osptr, _attr);
  }
  return os;
}

std::ostream &curs::attroff::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattroff(*osptr, _attr);
  }
  return os;
}

std::ostream &curs::attrset::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattrset(*osptr, _attr);
  }
  return os;
}

std::ostream &curs::pairon::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattron(*osptr, colors::pair(_color_pair));
  }
  return os;
}

std::ostream &curs::pairoff::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wattroff(*osptr, colors::pair(_color_pair));
  }
  return os;
}

std::ostream &curs::bkgrnd::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;

#ifdef HAVE_NCURSESW_H
    ::wbkgrnd(*osptr, _character);
#else
    ::wbkgd(*osptr, _character);
#endif
    ::wnoutrefresh(*osptr);
  }
  return os;
}

std::ostream &curs::bkgrndset::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;

#ifdef HAVE_NCURSESW_H
    ::wbkgrndset(*osptr, _character);
#else
    ::wbkgdset(*osptr, _character);
#endif
    ::wnoutrefresh(*osptr);
  }
  return os;
}

std::ostream &curs::cursor::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    if (_op == POSITION) {
      ::wmove(*osptr, _y, _x);
      ::wrefresh(*osptr);
    }
    if (_op == VISIBLITY) {
      if (_show) ::curs_set(1);
      else ::curs_set(0);
    }
  }
  return os;
}

std::ostream &curs::hline::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    if (_move)
      ::mvwhline_set(*osptr, _y, _x, _character, _length);
    else
      ::whline_set(*osptr, _character, _length);
    ::wnoutrefresh(*osptr);
  }
  return os;
}

std::ostream &curs::vline::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    if (_move)
      ::mvwvline_set(*osptr, _y, _x, _character, _length);
    else
      ::wvline_set(*osptr, _character, _length);
    ::wnoutrefresh(*osptr);
  }
  return os;
}

std::ostream &curs::box::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::box_set(*osptr, _verch, _horch);
    ::wnoutrefresh(*osptr);
  }
  return os;
}

std::ostream &curs::border::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    wborder_set(*osptr, _ls, _rs, _ts, _bs, _tl, _tr, _bl,
                _br);
  }
  return os;
}

std::ostream &curs::resize::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::wresize(*osptr, _height, _width);
  }
  return os;
}

std::ostream &curs::move::operator()(std::ostream &os) const {
  base_ostream *osptr = dynamic_cast<base_ostream *>(&os);
  if (osptr != NULL) {
    os << std::flush;
    ::mvwin(*osptr, _y, _x);
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
 * class curs::resize_event_hander
 */

curs::resize_event_handler::resize_event_handler()
  : _orig_handler(signal(SIGWINCH, curs::resize_event_handler::_callback)) {
  _handler = this;
}

curs::resize_event_handler::~resize_event_handler() noexcept {
}

void curs::resize_event_handler::resize_event() {
}

curs::resize_event_handler *curs::resize_event_handler::_handler;

void curs::resize_event_handler::_callback(int signal) {
  if (signal == SIGWINCH and _handler) {
    ::endwin();  // Recreate stdscr
    ::refresh();

    _handler->resize_event(); // Call the resize event handler.
  }
}

/*****************************************************************************
 * class curs::keyboard_event_hander
 */

static curs::keyboard_event_handler *_focused = nullptr;

/********************************************************
 * curs::keyboard_event_handler::keyboard_event_handler *
 ********************************************************/

curs::keyboard_event_handler::keyboard_event_handler() {
  // Just make sure we have something with keyboard focus.
  if (not _focused) _focused = this;
}

/***************************************
 * curs::keyboard_event_handler::focus *
 ***************************************/

void curs::keyboard_event_handler::focus() {
  // Someone is losing focus.
  if (_focused != nullptr) _focused->lose_focus();

  // Someone is gaining focus.
  _focused = this;
  gain_focus();
}

/*******************************************
 * curs::keyboard_event_handler::has_focus *
 *******************************************/

bool curs::keyboard_event_handler::has_focus() const {
  return (_focused == this);
}

/********************************************
 * curs::keyboard_event_handler::lose_focus *
 ********************************************/

void curs::keyboard_event_handler::lose_focus() {}

/********************************************
 * curs::keyboard_event_handler::gain_focus *
 ********************************************/

void curs::keyboard_event_handler::gain_focus() {}

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

/*************************
 * curs::events::process *
 *************************/

void curs::events::process() {
  int c;
  MEVENT event;

  c = ::getch();
  if (c == KEY_MOUSE) {
    // Dispatch mouse events.
    if (getmouse(&event) == OK) {
      for (auto &evt: mouse_handlers)
        evt->event(event.id, event.x, event.y, event.bstate);
    }
  } else if (_focused != nullptr) {
    // Dispatch the key event.
    _focused->key_event(c);
  }
}

/**********************
 * curs::events::main *
 **********************/

void curs::events::main() {
  while (_do_events) process();
}

/**********************
 * curs::events::quit *
 **********************/

void curs::events::quit() {
  _do_events = false;
}
