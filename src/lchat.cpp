/*                                                                  -*- c++ -*-
 * Copyright (c) 2018 Ron R Wills <ron.rwsoft@gmail.com>
 *
 * This file is part of the Local Chat Suite.
 *
 * Local Chat is free software: you can redistribute it and/or modify
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
 * along with the Local Chat Suite.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "nstream"
#include "curses"
#include <iostream>
#include <list>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <clocale>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>

// Curses color numeric ids.
static int C_TITLE     = 1;
static int C_USERNAME  = 2;
static int C_MYMESSAGE = 3;
static int C_HLPMSG    = 4;
static int C_SYSMSG    = 5;
static int C_PRVMSG    = 6;

// Global data.
static sockets::iostream chatio;
static std::string sock_path = "/var/lib/lchat/sock";
static std::string my_name;
static unsigned int scrollback = 500;
static bool auto_scroll = false;

/******************************************************************************
 */

class lchat;

/**
 */
class chat : public curs::window {
public:
  typedef enum {SCROLL_UP, SCROLL_DOWN, PAGE_UP, PAGE_DOWN} scroll_t;

  chat(lchat &chatw, int x, int y, int width, int height);

  void scroll(scroll_t value);

  void scroll_buffer(unsigned int size);
  void auto_scroll(bool value = true);

  void read_server();

  void redraw();

protected:

  void draw(const std::string &line);

private:
  lchat *_lchat;

  std::list<std::string> _scroll_buffer;
  unsigned int _buffer_size;
  unsigned int _buffer_location;
  bool _auto_scroll;
};

/**
 */
class userlist : public curs::window {
public:
  userlist(int x, int y, int width, int height);

  void query_server();

  void redraw();

private:
  std::list<std::string> _users;
};

/**
 */
class input : public curs::window, curs::keyboard_event_handler {
public:
  input(lchat &chat, int x, int y, int width, int height);

  void redraw();

protected:
  void key_event(int ch);

private:
  lchat *_lchat;

  std::string _line;
  size_t _insert;
};

/**
 */
class lchat : protected curs::window, protected curs::resize_event_handler {
public:
  lchat();

  typedef enum {D_UP, D_DOWN} scroll_dir_t;

  void scroll_chat(scroll_dir_t dir);
  void page_chat(scroll_dir_t dir);
  void refresh_users();

  void operator()();

private:
  void resize_event();

private:
  chat _chat;
  input _input;

  int _userlist_width;
  bool _refresh_users;
  userlist _userlist;

  void _draw();
};

/******************************************************************************
 * class chat
 */

/**************
 * chat::chat *
 **************/

chat::chat(lchat &chatw, int x, int y, int width, int height)
  : curs::window(x, y, width, height),
  _lchat(&chatw),
  _buffer_size(500),
  _buffer_location(0),
  _auto_scroll(false) {
  *this << curs::scrollok(true)
        << curs::cursor(0, height - 1)
        << std::flush;
}

/****************
 * chat::scroll *
 ****************/

void chat::scroll(scroll_t value) {
  int w, h;
  size(h, w);

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

  if ((int)_buffer_location + offset < 0) {
    _buffer_location = 0;
  } else {
    _buffer_location += offset;

    if (_buffer_location > _scroll_buffer.size() - h)
      _buffer_location = _scroll_buffer.size() - h;
  }

  redraw();
  curs::terminal::update();
}

/***********************
 * chat::scroll_buffer *
 ***********************/

void chat::scroll_buffer(unsigned int size) {
  _buffer_size = size;
  if (_buffer_size == 0)
    throw std::range_error("Invalid value for scroll buffer");
}

/*********************
 * chat::auto_scroll *
 *********************/

void chat::auto_scroll(bool value) {
  _auto_scroll = value;
}

/*********************
 * chat::read_server *
 *********************/

void chat::read_server() {
  std::string line;
  for (;;) {
    try {
      // Attempt to read a line from the server.
      getline(chatio, line);
      if (line.empty()) {
        usleep(100);
        return; // Nothing returned so return.
      }

      // Add the new line to the scroll buffer.
      _scroll_buffer.push_front(line);
      if (_scroll_buffer.size() > _buffer_size)
        _scroll_buffer.resize(_buffer_size);

      // If auto scroll the reposition buffer to the new line.
      if (_auto_scroll)
        _buffer_location = 0;

      // Update the screen.
      if (_buffer_location > 0)
        scroll(SCROLL_UP);
      else
        redraw();

    } catch (sockets::ionotready &err) {
      // Nothing to read on the socket, so return.
      chatio.clear();
      usleep(100);
      return;
    }
  }
}

/****************
 * chat::redraw *
 ****************/

void chat::redraw() {
  int w, h;
  size(w, h);

  // Clear the chat window.
  *this << curs::erase << curs::cursor(0, h - 1) << curs::cursor(false);

  if (not _scroll_buffer.empty()) {
    auto it = _scroll_buffer.rend();

    // Find the starting point.
    for (int c = 0;
         c < h + (int)_buffer_location and it != _scroll_buffer.rbegin();
         c++, it--);

    // Draw the lines.
    for (int c = 0; c < h and it != _scroll_buffer.rend(); c++, it++)
      this->draw(*it);
  }

  *this << std::flush;
}

/**************
 * chat::draw *
 **************/

void chat::draw(const std::string &line) {
  size_t pos = line.find(": ");
  if (line.compare(0, 2, "? ") == 0) {
    // Help message.
    *this << '\n'
          << curs::attron(curs::palette::pair(C_HLPMSG) | A_BOLD)
          << line.substr(2, line.length() - 2)
          << curs::attroff(curs::palette::pair(C_HLPMSG) | A_BOLD);

  } else if (line.compare(0, 2, "! ") == 0) {
    // Private message.
    *this << '\n'
          << curs::attron(curs::palette::pair(C_USERNAME))
          << line.substr(2, pos - 1)
          << curs::attroff(curs::palette::pair(C_USERNAME))
          << curs::attron(curs::palette::pair(C_PRVMSG) | A_BOLD)
          << line.substr(pos + 1, line.length() - pos - 1)
          << curs::attroff(curs::palette::pair(C_PRVMSG) | A_BOLD);

  } else if (line.compare(0, my_name.length() + 1, my_name + ":") == 0) {
    // A message that was sent by this user.
    *this << '\n'
          << curs::attron(curs::palette::pair(C_USERNAME))
          << line.substr(0, pos + 1)
          << curs::attroff(curs::palette::pair(C_USERNAME))
          << curs::attron(curs::palette::pair(C_MYMESSAGE))
          << line.substr(pos + 1, line.length() - pos)
          << curs::attroff(curs::palette::pair(C_MYMESSAGE));

  } else if (pos != line.npos) {
    // Color the senders name.
    *this << '\n'
          << curs::attron(curs::palette::pair(C_USERNAME))
          << line.substr(0, pos + 1)
          << curs::attroff(curs::palette::pair(C_USERNAME))
          << line.substr(pos + 1, line.length() - pos);

  } else {
    // System message.
    *this << '\n'
          << curs::attron(curs::palette::pair(C_SYSMSG))
          << line
          << curs::attroff(curs::palette::pair(C_SYSMSG));

    // User join message.
    if (line.length() > 21) {
      if (line.compare(line.length() - 21, 21, " has joined the chat.") == 0) {
        _lchat->refresh_users();
      }
    }

    // User left message.
    if (line.length() > 19) {
      if (line.compare(line.length() - 19, 19, " has left the chat.") == 0) {
        _lchat->refresh_users();
      }
    }
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
}

/**************************
 * userlist::query_server *
 **************************/

void userlist::query_server() {
  std::string users;

  // Query the server.
  chatio << "/who" << std::endl;
  for (;;) {
    try {
      getline(chatio, users);
      break;
    } catch (sockets::ionotready &err) {
      chatio.clear();
      usleep(1000);
    }
  }

  // Clear the old user list.
  _users.clear();

  // Repopulate the user list with the servers response.
  size_t last = 0, pos;
  while ((pos = users.find(" ", last)) != users.npos) {
    _users.push_back(users.substr(last, pos));
    last = pos += 1;
  }

  // Redraw the list.
  redraw();
}

/********************
 * userlist::redraw *
 ********************/

void userlist::redraw() {
  // Reset the window.
  *this << curs::erase << curs::cursor(0, 0) << curs::cursor(false);

  // Write the user list.
  for (auto &user: _users) {
    *this << user << "\n";
  }

  // Flush it to the screen.
  *this << std::flush;
}

/******************************************************************************
 * class input
 */

 /****************
  * Input::Input *
  ****************/

input::input(lchat &chat, int x, int y, int width, int height)
  : curs::window(x, y, width, height), _lchat(&chat), _line(), _insert(0) {
  *this << curs::leaveok(false);
}

/*****************
 * input::redraw *
 *****************/

void input::redraw() {
  *this << curs::erase
        << curs::cursor(0, 0) << "> " << _line
        << curs::cursor(_insert + 2, 0) << curs::cursor(true);
}

/********************
 * Input::key_event *
 ********************/

void input::key_event(int ch) {
  redraw();
  curs::terminal::update();

  switch (ch) {
  case ERR: // Keyboard input timeout
    usleep(100);
    return;

  case '\n': // Send the line to the server and reset the input.
    chatio << _line << std::endl;
    _line = "";
    _insert = 0;
    redraw();
    usleep(100);
    return;

  case KEY_BACKSPACE:
    if (not _line.empty() and _insert > 0) {
      _line.erase(_insert - 1, 1);
      _insert--;
    }
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
    if (isprint(ch)) {
      std::string in;
      in += ch; // This is sillyness, but it's what C++ wants.

      _line.insert(_insert, in);
      _insert++;
    }
    break;
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
  _refresh_users(true),
  _userlist(width() - 11, 1, 11, height() - 3) {

  // Enable keypad translation.
  *this << curs::keypad(true);

  // Configure the color theme.
  curs::palette::start();
  if (curs::palette::has_colors()) {
    curs::palette::pair(C_TITLE, curs::palette::WHITE,
                        curs::palette::BLUE);
    curs::palette::pair(C_USERNAME, curs::palette::CYAN, -1);
    curs::palette::pair(C_MYMESSAGE, curs::palette::GREEN, -1);
    curs::palette::pair(C_HLPMSG, curs::palette::CYAN, -1);
    curs::palette::pair(C_SYSMSG, curs::palette::BLUE, -1);
    curs::palette::pair(C_PRVMSG, curs::palette::YELLOW, -1);
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

void lchat::refresh_users() {
  _refresh_users = true;
}

/***********************
 *  lchat::operator () *
 ***********************/

void lchat::operator()() {
  // Our main application loop.
  while (chatio) {
    _chat.read_server();

    curs::events::process();

    if (chatio and _refresh_users) {
      _userlist.query_server();
      _refresh_users = false;
    }

    curs::terminal::update();
  }
}

/************************
 *  lchat::resize_event *
 ************************/

void lchat::resize_event() {
  // Redraw ourself.
  _draw();

  // Get the new window size.
  int w, h;
  size(w, h);

  // Resize and refresh all the other windows.
  _chat << curs::move(0, 1)
        << curs::resize(w - (_userlist_width + 1), h - 3);
  _chat.redraw();
  _input << curs::move(0, h - 1)
         << curs::resize(w, 1);
  _input.redraw();
  _userlist << curs::move(w - _userlist_width, 1)
            << curs::resize(_userlist_width, h - 3);
  _userlist.redraw();
}

/****************
 * lchat::_draw *
 ****************/

void lchat::_draw() {
  int w, h;
  size(w, h);

  // Draw the frames around our windows.
  *this << curs::attron(curs::palette::pair(C_TITLE) | A_BOLD)
        << curs::cursor(0, 0) << u"📡" << std::string(w - 1, ' ');
  std::string title("Local Chat v" VERSION);
  *this << curs::cursor((w - title.size()) / 2, 0) << title
        << curs::attroff(curs::palette::pair(C_TITLE) | A_BOLD)
        << curs::cursor(0, h - 2)
        << curs::hline(w, ACS_HLINE)
        << curs::cursor(w - (_userlist_width + 1), 1)
        << curs::vline(h - 3, ACS_VLINE)
        << std::flush;
}

/******************************************************************************
 * Program entry point.
 */

int main(int argc, char *argv[]) {
  std::setlocale(LC_ALL, "");

  // Get the command line arguments.
  int opt;
  while ((opt = getopt(argc, argv, "as:l:")) != -1) {
    switch (opt) {
    case 'a':
      auto_scroll = true;
      break;
    case 'l':
      scrollback = atoi(optarg);
      if (scrollback == 0) {
        std::cerr << "OPTIONS ERROR: Invalid value \"" << optarg
                  << "\" for the number of scrollback lines"
                  << '\n';
        return EXIT_FAILURE;
      }
      break;
    case 's':
      sock_path = optarg;
      break;
    default:
      std::cerr << "Unknown option -" << (char)optopt << std::endl;
      return EXIT_FAILURE;
    }
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
    chatio >> sockets::nonblock;
    chatio >> sockets::recvattempts(1);
  } catch (std::exception &err) {
    std::cerr << err.what() << std::endl;
    return EXIT_FAILURE;
  }

  // Setup the terminal.
  curs::terminal::initialize();
  curs::terminal::cbreak(true);
  curs::terminal::echo(false);
  curs::terminal::halfdelay(1);

  try {
    lchat chat_ui;
    chat_ui();
  } catch (std::exception &err) {
    curs::terminal::restore();
    std::cerr << err.what() << std::endl;
    return EXIT_FAILURE;
  }

  // Restore the terminal.
  curs::terminal::restore();
  return EXIT_SUCCESS;
}
