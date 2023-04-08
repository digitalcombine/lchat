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

#include "nstream"
#include "curses"
#include "autocomplete.h"
#include <iostream>
#include <sstream>
#include <list>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <clocale>
#include <mutex>
#include <pwd.h>
#include <thread>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#ifdef DEBUG
#include <fstream>

std::ofstream debug;
#endif

// Curses color numeric ids.
static int C_TITLE      = 1;
static int C_USERNAME   = 2;
static int C_MYMESSAGE  = 3;
static int C_HLPMSG     = 4;
static int C_SYSMSG     = 5;
static int C_PRVMSG     = 6;
static int C_STATUS     = 7;
static int C_STATUS_ON  = 8;
static int C_STATUS_OFF = 9;
static int C_HISTORY    = 10;

// Global data.
static sockets::iostream chatio;
static std::string sock_path = STATEDIR "/sock";
static std::string my_name;

static curs::cchar bgstatus(C_STATUS, U' ');

// Mutex to synchronize all curses drawing operations.
static std::mutex curs_mtx;

static autocomplete completion;

/******************************************************************************
 */

class lchat;
class status;

/** The Chat Window.
 */
class chat : public curs::window {
public:
  typedef enum {SCROLL_UP, SCROLL_DOWN, PAGE_UP, PAGE_DOWN} scroll_t;

  /** Creates a new chat window.
   */
  chat(lchat &chatw, int x, int y, int width, int height);

  /** Scroll the chat within the window, either by a single line or by a page.
   * @param value How the chat is scrolled.
   */
  void scroll(scroll_t value);

  static void thread_loop(chat *obj);

  void redraw();

  bool connected() const { return _connected; }

  static bool auto_scroll;
  static unsigned int scrollback;

  friend class status;

protected:

  void read_server();

  /** Display a line of the chat with in the window. This applies all the
   * syntax highlighting to the chat.
   * @param line The chat line to render to the window.
   */
  void draw(const std::string &line);

private:
  /** Reference to the toplevel local chat window. */
  lchat *_lchat;

  /** The buffered chat. */
  std::list<std::string> _scroll_buffer;

  /** The maximum lines kept by the scroll buffer. */
  unsigned int _buffer_size;

  /** The location in the scroll buffer to start displaying the chat from.
   * This is used to scroll the chat window.
   */
  unsigned int _buffer_location;

  bool _connected;
};

/** The Userlist Window.
 */
class userlist : public curs::window {
public:
  userlist(int x, int y, int width, int height);

  void update(const std::string &list);

  void redraw();

  friend class status;

private:
  std::list<std::string> _users;
  std::list<std::string> _autocomp;
};

/** The Status Line.
 *
 * This typically shows the number of unique users connected, scroll lines and
 * other information.
 */

class status : public curs::window {
public:
  status(chat &ch, userlist &ul, int x, int y, int width, int height);

  void redraw();

private:
  /** Reference to the chat window. */
  chat *_chat;
  /** Reference to the user list window. */
  userlist *_userlist;
};

/** The chat's input.
 */
class input : public curs::window, curs::keyboard_event_handler {
public:
  input(lchat &chat, int x, int y, int width, int height);

  void redraw();

protected:
  // Key event handler.
  void key_event(int ch);

private:
  // Reference to the chat interface.
  lchat *_lchat;

  // Input string.
  std::string _line;
  size_t _insert;

  std::string _suggest;

  // Message history support.
  std::list<std::string> _history;
  bool _history_scan;
  std::list<std::string>::iterator _history_iter;
};

/**
 */
class lchat : protected curs::window, protected curs::resize_event_handler {
public:
  lchat();

  typedef enum {D_UP, D_DOWN} scroll_dir_t;

  void scroll_chat(scroll_dir_t dir);
  void page_chat(scroll_dir_t dir);
  void refresh_users(const std::string &list);

  void operator()();

  void update();

  friend class input;

private:
  void resize_event();

private:
  chat _chat;
  input _input;

  int _userlist_width;
  userlist _userlist;

  status _status;

  void _draw();
};

/******************************************************************************
 * class chat
 */

bool chat::auto_scroll = false;
unsigned int chat::scrollback = 500;
static bool insert_mode = true;

/**************
 * chat::chat *
 **************/

chat::chat(lchat &chatw, int x, int y, int width, int height)
  : curs::window(x, y, width, height),
  _lchat(&chatw),
  _buffer_size(scrollback),
    _buffer_location(0), _connected(true) {
  *this << curs::scrollok(true)
        << curs::cursor(0, height - 1)
        << std::flush;
}

/****************
 * chat::scroll *
 ****************/

void chat::scroll(scroll_t value) {
  // Get the chat window size.
  int w, h;
  size(w, h);

  // Set offset to the amount to shift the window by.
  int offset = 0;
  switch (value) {
  case SCROLL_UP:
    offset = 1;
    break;
  case SCROLL_DOWN:
    offset = -1;
    break;
  case PAGE_UP:
    offset = h;
    break;
  case PAGE_DOWN:
    offset = -h;
    break;
  }

  // Adjust the buffer location if it goes beyond the window or buffer sizes.
  if ((int)_buffer_location + offset < 0) {
    _buffer_location = 0;
  } else {
    _buffer_location += offset;

    if (_buffer_location > _scroll_buffer.size() - h)
      _buffer_location = _scroll_buffer.size() - h;

    if (h > (int)_scroll_buffer.size())
      _buffer_location = 0;
  }

  // Redraw the chat window.
  redraw();
}

/*********************
 * chat::thread_loop *
 *********************/

void chat::thread_loop(chat *obj) {
  obj->read_server();
}

/*********************
 * chat::read_server *
 *********************/

void chat::read_server() {
  std::string line;
  while (chatio) {
    //try {
      // Attempt to read a line from the server.
      getline(chatio, line);
      if (line.empty()) {
        continue; // Nothing returned so return.
      }

      if (line.compare(0, 2, "~ ") == 0) {
        // User list update
        _lchat->refresh_users(line.substr(2, line.length() - 2));
        continue;
      }

      // User join message.
      if (line.length() > 21) {
        if (line.compare(line.length() - 21, 21,
                         " has joined the chat.") == 0) {
          chatio << "/who" << std::endl;
        }
      }

      // User left message.
      if (line.length() > 19) {
        if (line.compare(line.length() - 19, 19,
                         " has left the chat.") == 0) {
          chatio << "/who" << std::endl;
        }
      }

      // Add the new line to the scroll buffer.
      _scroll_buffer.push_front(line);
      while (_scroll_buffer.size() > _buffer_size) {
        _scroll_buffer.pop_back();
      }

      // Handle message scrolling in the chat window.
      if (auto_scroll) {
        // If auto scroll the reposition buffer to the new line.
        _buffer_location = 0;
        redraw();
      } else if (_buffer_location > 0) {
        // Update the screen.
        scroll(SCROLL_UP);
      } else
        redraw();

      if (not chatio and not chatio.eof()) {
        chatio.clear();
      }
  }

#ifdef DEBUG
  debug << "Server closed the connection" << std::endl;
#endif
  _connected = false;
}

/****************
 * chat::redraw *
 ****************/

void chat::redraw() {
  int w, h;
  size(w, h);

  curs_mtx.lock();

  // Clear the chat window.
  *this << curs::erase << curs::cursor(0, h - 1) << curs::cursor(false);

  if (not _scroll_buffer.empty()) {
    auto it = _scroll_buffer.rend();

    // Find the starting point.
    for (int c = 0;
         c < h + (int)_buffer_location and it != _scroll_buffer.rbegin();
         c++, it--);

    // Draw the lines.
    for (int c = 0; c < h and it != _scroll_buffer.rend(); c++, it++) {
      this->draw(*it);
    }
  }

  *this << std::flush;

  curs::terminal::update();
  curs_mtx.unlock();

  _lchat->update();
}

/**************
 * chat::draw *
 **************/

void chat::draw(const std::string &line) {
  size_t pos = line.find(": ");

  if (line.compare(0, 2, "? ") == 0) {
    // Help message.
    *this << '\n'
          << curs::attron(curs::colors::pair(C_HLPMSG) | A_BOLD)
          << line.substr(2, line.length() - 2)
          << curs::attroff(curs::colors::pair(C_HLPMSG) | A_BOLD);

  } else if (line.compare(0, 2, "! ") == 0) {
    // Private message.
    *this << '\n'
          << curs::attron(curs::colors::pair(C_USERNAME))
          << line.substr(2, pos - 1)
          << curs::attroff(curs::colors::pair(C_USERNAME))
          << curs::attron(curs::colors::pair(C_PRVMSG) | A_BOLD)
          << line.substr(pos + 1, line.length() - pos - 1)
          << curs::attroff(curs::colors::pair(C_PRVMSG) | A_BOLD);

  } else if (line.compare(0, my_name.length() + 1, my_name + ":") == 0) {
    // A message that was sent by this user.
    *this << '\n'
          << curs::attron(curs::colors::pair(C_USERNAME))
          << line.substr(0, pos + 1)
          << curs::attroff(curs::colors::pair(C_USERNAME))
          << curs::attron(curs::colors::pair(C_MYMESSAGE))
          << line.substr(pos + 1, line.length() - pos)
          << curs::attroff(curs::colors::pair(C_MYMESSAGE));

  } else if (pos != line.npos) {
    // Color the senders name.
    *this << '\n'
          << curs::attron(curs::colors::pair(C_USERNAME))
          << line.substr(0, pos + 1)
          << curs::attroff(curs::colors::pair(C_USERNAME))
          << line.substr(pos + 1, line.length() - pos);

  } else {
    // System message.
    *this << '\n'
          << curs::attron(curs::colors::pair(C_SYSMSG))
          << line
          << curs::attroff(curs::colors::pair(C_SYSMSG));
  }
}

/******************************************************************************
 * class userlist
 */

/**********************
 * userlist::userlist *
 **********************/

userlist::userlist(int x, int y, int width, int height)
  : curs::window(x, y, width, height) {
  completion.add(_autocomp);
}

/********************
 * userlist::update *
 ********************/

void userlist::update(const std::string &list) {
  // Clear the old user list.
  _users.clear();
  _autocomp.clear();

#ifdef DEBUG
  debug << "Parsing user list: " << list << std::endl;
#endif

  // Repopulate the user list with the servers response.
  size_t last = 0, pos;
  while ((pos = list.find(" ", last)) != list.npos) {
    std::string user = list.substr(last, pos - last);
#ifdef DEBUG
    debug << " " << last << ":" << pos << " " << user << std::endl;
#endif
    _users.push_back(user);
    _autocomp.push_back("/msg " + user);
    _autocomp.push_back("/priv " + user);
    last = pos += 1;
  }

  // Redraw the list.
  redraw();
}

/********************
 * userlist::redraw *
 ********************/

void userlist::redraw() {
  curs_mtx.lock();

  // Reset the window.
  *this << curs::erase << curs::cursor(0, 0) << curs::cursor(false);

  // Write the user list.
  for (auto &user: _users) {
    *this << user << "\n";
  }

  // Flush it to the screen.
  *this << std::flush;

  curs::terminal::update();
  curs_mtx.unlock();
}

/******************************************************************************
 * class status
 */

status::status(chat &ch, userlist &ul, int x, int y, int width, int height)
  : curs::window(x, y, width, height), _chat(&ch), _userlist(&ul) {
  *this << curs::scrollok(false);
}

void status::redraw() {
  curs_mtx.lock();

  *this << curs::attron(curs::colors::pair(C_STATUS))
        << curs::bkgrnd(bgstatus)
        << curs::cursor(0, 0) << curs::erase;

  std::ostringstream msg;

  if (_userlist->_users.size() == 1)
    msg << _userlist->_users.size() << " user";
  else
    msg << _userlist->_users.size() << " users";
  *this << curs::cursor(1, 0) << msg.str();

  msg.str("");
  msg << (_chat->_scroll_buffer.size() - _chat->_buffer_location) << "/"
      << chat::scrollback;
  *this << curs::cursor(width() - 4 - msg.str().length(), 0)
        << msg.str();

  if (chat::auto_scroll)
    *this << curs::cursor(width() - 3, 0) << "↧";

  if (insert_mode)
    *this << curs::cursor(width() - 2, 0) << "i";
  else
    *this << curs::cursor(width() - 2, 0) << "o";

  *this << std::flush;

  curs::terminal::update();
  curs_mtx.unlock();
}

/******************************************************************************
 * class input
 */

 /****************
  * input::input *
  ****************/

input::input(lchat &chat, int x, int y, int width, int height)
  : curs::window(x, y, width, height), _lchat(&chat), _line(), _insert(0),
    _history_scan(false) {
  *this << curs::leaveok(false);

  completion.add("/exit");
  completion.add("/quit");
  completion.add("/help");
  completion.add("/version");
  completion.add("/about");
  completion.add("/who");

  completion.add(_history);
}

/*****************
 * input::redraw *
 *****************/

void input::redraw() {
  curs_mtx.lock();

  if (_history_scan) {
    *this << curs::erase
          << curs::cursor(0, 0) << "? "
          << curs::pairon(C_HISTORY) << _line
          << curs::cursor(_insert + 2, 0) << curs::cursor(true)
          << curs::pairoff(C_HISTORY);
  } else {
    *this << curs::erase
          << curs::cursor(0, 0) << "> ";

    if (not _suggest.empty())
      *this << curs::pairon(C_HISTORY) << _suggest
            << curs::pairoff(C_HISTORY);

    *this << curs::cursor(2, 0) << _line;
    *this << curs::cursor(_insert + 2, 0) << curs::cursor(true);
  }

  curs::terminal::update();
  curs_mtx.unlock();
}

/********************
 * input::key_event *
 ********************/

#define CTRL(c) ((c) & 037)

void input::key_event(int ch) {
  if (_history_scan) {
    // We're in history input mode here.
    switch (ch) {
    case ERR: // Keyboard input timeout
      return;

    case KEY_UP:
      if (_history_iter != _history.begin()) --_history_iter;
      _line = *_history_iter;
      _insert = _line.length();
      break;

    case KEY_DOWN:
      if (_history_iter != _history.end()) ++_history_iter;
      if (_history_iter != _history.end()) {
        _line = *_history_iter;
      } else {
        _line = "";
      }
      _insert = _line.length();
      break;

    case '\n':
      _history_scan = false;
      break;
    }

  } else {
    // Normal input mode.
    switch (ch) {
    case ERR: // Keyboard input timeout
      return;

    case '\n': // Send the line to the server and reset the input.
      chatio << _line << std::endl;
      _history.push_back(_line);
      while (_history.size() > 100) _history.pop_front();
      _line = "";
      _insert = 0;
      _suggest = "";
      break;

    case CTRL('a'): // Toggle auto scroll.
      chat::auto_scroll = not chat::auto_scroll;
      break;

    case CTRL('p'): // Enter into history mode.
      _history_scan = true;
      _history_iter = _history.end();
      _line = "";
      _insert = 0;
      break;

    case CTRL('r'): // Force a redraw of the client.
      // We cheat a little here by using the resize event handler.
      _lchat->resize_event();
      break;

    case '\x9': {// Tab key
      bool is_more;
      _line = completion(_line, _suggest, is_more);
      if (not _suggest.empty() and not is_more) {
        _line = _suggest;
        _insert = _line.size();
      }
      _insert = _line.size();
      break;
    }
    case KEY_BACKSPACE: // Backspace key and variants.
    case '\b':
    case '\x7f':
      if (not _line.empty() and _insert > 0) {
        _line.erase(_insert - 1, 1);
        _insert--;
        _suggest = "";
      }
      break;

    case KEY_DC: // Delete key.
      if (not _line.empty() and _insert < _line.size()) {
        _line.erase(_insert, 1);
        _suggest = "";
      }
      break;

    case KEY_IC: // Insert key, toggles insert/overwrite mode
      insert_mode = (insert_mode ? false : true);
      break;

    case KEY_UP:
      _lchat->scroll_chat(lchat::D_UP);
      break;

    case KEY_DOWN:
      _lchat->scroll_chat(lchat::D_DOWN);
      break;

    case KEY_LEFT:
      if (_insert > 0) _insert--;
      break;

    case KEY_RIGHT:
      if (_insert < _line.size()) _insert++;
      break;

    case KEY_PPAGE: // Page up key
      _lchat->page_chat(lchat::D_UP);
      break;

    case KEY_NPAGE: // Page down key
      _lchat->page_chat(lchat::D_DOWN);
      break;

    case KEY_HOME:
      _insert = 0;
      break;

    case KEY_END:
      _insert = _line.size();
      break;

    default:
      /* XXX Current isprint doesn't work with multibyte unicode
       * characters, but for the moment our _insert pointers is smart
       * enough to manage multibyte characters.
       */
      if (isprint(ch)) {
        std::string in;
        in += ch; // This is sillyness, but it's what C++ wants.

        if (insert_mode or _insert >= _line.size())
          _line.insert(_insert, in);
        else
          _line.replace(_insert, 1, in);
        _insert++;
      }
      break;
    }
  }

  redraw();
}

/******************************************************************************
 * class lchat
 */

/****************
 * lchat::lchat *
 ****************/

lchat::lchat()
  : curs::window(),
  _chat(*this, 0, 1, width()  - (12), height() - 3),
  _input(*this, 0, height() - 1, width(), 1),
  _userlist_width(11),
  _userlist(width() - 11, 1, 11, height() - 3),
  _status(_chat, _userlist, 0, height() - 2, width(), 1) {

  // Enable keypad translation.
  *this << curs::keypad(true);

  // Configure the color theme.
  curs::colors::start();
  if (curs::colors::have()) {
    curs::colors::pair(C_TITLE, curs::colors::WHITE,
                        curs::colors::BLUE);
    curs::colors::pair(C_USERNAME, curs::colors::CYAN, -1);
    curs::colors::pair(C_MYMESSAGE, curs::colors::GREEN, -1);
    curs::colors::pair(C_HLPMSG, curs::colors::CYAN, -1);
    curs::colors::pair(C_SYSMSG, curs::colors::BLUE, -1);
    curs::colors::pair(C_PRVMSG, curs::colors::YELLOW, -1);
    curs::colors::pair(C_STATUS, curs::colors::WHITE,
                        curs::colors::BLUE);
    curs::colors::pair(C_STATUS_ON, curs::colors::YELLOW,
                        curs::colors::BLUE);
    curs::colors::pair(C_STATUS_OFF, curs::colors::RED,
                        curs::colors::BLUE);
    curs::colors::pair(C_HISTORY, curs::colors::CYAN, -1);
  }

  _draw();
}

/**********************
 * lchat::scroll_chat *
 **********************/

void lchat::scroll_chat(scroll_dir_t dir) {
  switch(dir) {
  case D_UP:
    _chat.scroll(chat::SCROLL_UP);
    break;
  case D_DOWN:
    _chat.scroll(chat::SCROLL_DOWN);
    break;
  }
}

/********************
 * lchat::page_chat *
 ********************/

void lchat::page_chat(scroll_dir_t dir) {
  switch(dir) {
  case D_UP:
    _chat.scroll(chat::PAGE_UP);
    break;
  case D_DOWN:
    _chat.scroll(chat::PAGE_DOWN);
    break;
  }
}

/*************************
 *  lchat::refresh_users *
 *************************/

void lchat::refresh_users(const std::string &list) {
  _userlist.update(list);
}

/***********************
 *  lchat::operator () *
 ***********************/

void lchat::operator()() {

  // Create the chat window thread.
  std::thread chat_view(&chat::thread_loop, &_chat);

  update();

  // Our main application loop.
  while (_chat.connected()) {
    curs::events::process();
    //update();
  }

#ifdef DEBUG
  debug << "The chat is disconnected" << std::endl;
#endif

  // Clean up the chat window thread.
  chat_view.join();
}

/******************
 *  lchat::update *
 ******************/

void lchat::update() {
  _status.redraw();
  _input.redraw();

  //update_term();
}

/************************
 *  lchat::resize_event *
 ************************/

void lchat::resize_event() {
#ifdef DEBUG
  debug << "Resize event to ";
#endif

  curs_mtx.lock();
  // Get the new window size.
  int w, h;
  size(w, h);

#ifdef DEBUG
  debug << w << "x" << h << std::endl;
#endif

  // Resize and refresh all the other windows.
  _chat << curs::move(0, 1)
        << curs::resize(w - (_userlist_width + 1), h - 3);

  _userlist << curs::move(w - _userlist_width, 1)
            << curs::resize(_userlist_width, h - 3);

  _status << curs::move(0, h - 2)
          << curs::resize(w, 1);

  _input << curs::move(0, h - 1)
         << curs::resize(w, 1);

  curs_mtx.unlock();

#ifdef DEBUG
  debug << " redrawing screen" << std::endl;
#endif

  _draw(); // Redraw ourself.
  _chat.redraw();
  _userlist.redraw();
  _status.redraw();
  _input.redraw();
}

/****************
 * lchat::_draw *
 ****************/

void lchat::_draw() {
  curs_mtx.lock();

  int w, h;
  size(w, h);

  // Draw the frames around our windows.
  *this << curs::attron(curs::colors::pair(C_TITLE) | A_BOLD)
        << curs::cursor(0, 0) << std::string(w, ' ');
  std::string title("Local Chat v" VERSION);
  *this << curs::cursor((w - title.size()) / 2, 0) << title
        << curs::attroff(curs::colors::pair(C_TITLE) | A_BOLD)
        << curs::cursor(w - (_userlist_width + 1), 1)
        << curs::vline(h - 3)
        << std::flush;

  curs::terminal::update();
  curs_mtx.unlock();
}

/***********
 * version *
 ***********/

static void version() {
  std::cout << "Local Chat v" VERSION << "\n"
            << "Copyright © 2018-2020 Ron R Wills <ron@digitalcombine.ca>.\n"
            << "License BSD: 3-Clause BSD License <https://opensource.org/licenses/BSD-3-Clause>.\n\n"
            << "This is free software: you are free to change and redistribute it.\n"
            << "There is NO WARRANTY, to the extent permitted by law."
            << std::endl;
}

/********
 * help *
 ********/

static void help() {
  std::cout << "Local Chat v" VERSION << "\n"
            << "  lchat [-s path] [-a] [-l scrollback lines]\n"
            << "  lchat [-s path] [-m|-m message]\n"
            << "  lchat [-s path] [-b bot command]\n"
            << "  lchat -V\n"
            << "  lchat -h|-?\n\n"
            << "Copyright © 2018-2020 Ron R Wills <ron@digitalcombine.ca>.\n"
            << "License BSD: 3-Clause BSD License <https://opensource.org/licenses/BSD-3-Clause>.\n\n"
            << "This is free software: you are free to change and redistribute it.\n"
            << "There is NO WARRANTY, to the extent permitted by law."
            << std::endl;
}

/****************
 * send_message *
 ****************/

static void send_message(const std::string &line) {
  // Send the message to the dispatcher.
  chatio << line << std::endl;
#ifdef DEBUG
  std::clog << "> " << line << std::endl;
#endif

  // Pass control to the kernel for a moment to actually deliver the
  // message.
  usleep(10);

  std::string in;
  for (;;) {
    try {
      // Read anything coming in from the dispatcher.
      getline(chatio, in);

      // If the line is empty, we're out of here.
      if (in.empty()) break;

#ifdef DEBUG
      std::clog << "< " << in << std::endl;
#endif
    } catch (sockets::ionotready &err) {
      // Nothing ready from the dispatcher, we're done here.
      chatio.clear();
      usleep(100);
      break;
    }
  }
}

/*************
 * quit_chat *
 *************/

/** The proper way to quit the chat session, usually used when lchat is being
 * used in a script.
 */
static void quit_chat() {
  // Send the quit command to the dispatcher.
  send_message("/quit");

  // Loop until the dispatcher disconnects the line.
  std::string line;
  while (chatio) {
    try {
      // Read anything coming in from the dispatcher.
      getline(chatio, line);

      // If the line is empty, we're out of here.
      if (line.empty()) break;
#ifdef DEBUG
      std::clog << "< " << line << std::endl;
#endif

    } catch (sockets::ionotready &err) {
      // Nothing ready from the dispatcher.
      chatio.clear();
      usleep(100);
    }
  }
}

/*******
 * bot *
 *******/

/** Fork and execute a shell command piping its IO through the chat's unix
 * socket.
 */
static int bot(const std::string &command) {
  auto pid = fork();
  switch (pid) {
    case 0: // Child Process
      // Redirect the childs IO through the socket.
      dup2(chatio.socket(), STDIN_FILENO);
      dup2(chatio.socket(), STDOUT_FILENO);

      // Execute the bot command.
      execlp("sh", "sh", "-c", command.c_str(), NULL);
      break; // Stops compiler warnings.
    case -1:
      std::cerr << "Bot command failed\n - " << strerror(errno) << std::endl;
      return -1;
    default: {
      int status;
      waitpid(pid, &status, 0);
      return status;
    }
  }
  return 0; // Stops compiler warnings.
}

/******************************************************************************
 * Program entry point.
 */

int main(int argc, char *argv[]) {
  std::setlocale(LC_ALL, "");

  std::string bot_command, message;
  bool mesg_stdin = false;

#ifdef DEBUG
  debug.open("lchat.log");
#endif


  // Get the command line arguments.
  int opt;
  while ((opt = getopt(argc, argv, ":as:l:b:m:hV?")) != -1) {
    switch (opt) {
    case 'a':
      chat::auto_scroll = true;
      break;
    case 'b':
      bot_command = optarg;
      break;
    case '?':
    case 'h':
      help();
      return EXIT_SUCCESS;
    case 'l':
      chat::scrollback = atoi(optarg);
      if (chat::scrollback == 0) {
        std::cerr << "OPTIONS ERROR: Invalid value \"" << optarg
                  << "\" for the number of scrollback lines"
                  << '\n';
        return EXIT_FAILURE;
      }
      break;
    case 'm':
      message = optarg;
      break;
    case 's':
      sock_path = optarg;
      break;
    case 'V':
      version();
      return EXIT_SUCCESS;
    case ':':
      if (optopt == 'm') {
        mesg_stdin = true;
      } else {
        std::cout << "Missing value for option -" << (char)optopt
                  << std::endl;
        help();
        return EXIT_FAILURE;
      };
      break;
    default:
      std::cerr << "Unknown option -" << (char)optopt << std::endl;
      help();
      return EXIT_FAILURE;
    }
  }

  // Check if our input is from a tty.
  if (not isatty(STDIN_FILENO)) {
    mesg_stdin = true;
  }

  // Read the password file to get our user name.
  struct passwd *pw_entry = getpwuid(getuid());
  if (pw_entry == NULL) {
    std::cerr << "Unable to determine who you are:"
              << strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }
  my_name = pw_entry->pw_name;

  // Try to connect to the chat unix socket.
  try {
    chatio.open(sock_path);
  } catch (std::exception &err) {
    std::cerr << err.what() << std::endl;
    return EXIT_FAILURE;
  }

  if (mesg_stdin) {
    chatio >> sockets::nonblock;
    std::string line;
    while (getline(std::cin, line)) {
      send_message(line);
    }
    quit_chat();

  } else if (not message.empty()) {
    // If the -m option was given then send the message.
    chatio >> sockets::nonblock;
    send_message(message);
    quit_chat();

  } else if (not bot_command.empty()) {
    // If the -b option was given then run the bot.
    chatio >> sockets::block;
    return bot(bot_command);

  } else {
    // Interactive user interface.
    chatio >> sockets::block;

    try {
      // Setup the terminal.
      curs::terminal terminal;

      terminal.cbreak(true);
      terminal.echo(false);
      terminal.halfdelay(10);

      lchat chat_ui;
      chatio << "/who" << std::endl;

      chat_ui();
    } catch (std::exception &err) {
      std::cerr << err.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

#ifdef DEBUG
  debug.close();
#endif

  return EXIT_SUCCESS;
}
