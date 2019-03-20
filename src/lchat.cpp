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
sockets::iostream chatio;
std::string sock_path = "/var/lib/lchat/sock";
std::string my_name;
curs::terminal *terminal;

// Global event flags.
bool update_user_list = false;
int terminal_resize = 0;

/**
 */

class Chat : public curs::window {
public:
  Chat(int width, int height, int startx, int starty);

  void operator ()(curs::window &input);

  void update();
  void update(int offset);

  void scroll_buffer(unsigned int size);
  void auto_scroll(bool on);

protected:

  void print(const std::string &line);

private:
  std::list<std::string> _scroll_buffer;
  unsigned int _buffer_size;
  unsigned int _buffer_location;
  bool _auto_scroll;
};

/**
 */

class UserList : public curs::window {
public:
  UserList(int width, int height, int startx, int starty);

  void update(curs::window &input);
};

/**
 */

class Input : public curs::window {
public:
  Input(int width, int height, int startx, int starty);

  void operator ()(curs::terminal &term, Chat &chat);
  void update();

private:
  std::string _line;
  bool _idle;
};

/******************************************************************************
 * class Chat
 */

/**************
 * Chat::Chat *
 **************/

Chat::Chat(int startx, int starty, int width, int height)
  : curs::window(startx, starty, width, height), _buffer_size(500),
  _buffer_location(0), _auto_scroll(false) {
  *this << curs::scrollok(true)
        << curs::cursor(0, height - 1)
        << std::flush;
}

/*********************
 * Chat::operator () *
 *********************/

void Chat::operator ()(curs::window &input) {
  std::string in;
  for (;;) {
    try {
      getline(chatio, in);
      if (in.empty()) return;

      _scroll_buffer.push_front(in);
      if (_scroll_buffer.size() > _buffer_size)
        _scroll_buffer.resize(_buffer_size);
      if (_auto_scroll)
        _buffer_location = 0;

      if (_buffer_location > 0)
        update(1);
      else
        update();
      input << std::flush;
    } catch (sockets::ionotready &err) {
      chatio.clear();
      return;
    }
  }
}

/***************
* Chat::update *
***************/

void Chat::update() {
  int w, h;
  size(w, h);

  *this << curs::erase << curs::cursor(0, h - 1);

  if (not _scroll_buffer.empty()) {
    auto it = _scroll_buffer.rend();
    for (int c = 0;
         c < h + (int)_buffer_location and it != _scroll_buffer.rbegin();
         c++, it--);

    for (int c = 0; c < h and it != _scroll_buffer.rend(); c++, it++)
      this->print(*it);
  }

  *this << std::flush;
}

void Chat::update(int offset) {
  int w, h;
  size(h, w);

  if ((int)_buffer_location + offset < 0) {
    _buffer_location = 0;
  } else {
    _buffer_location += offset;

    if (_buffer_location > _scroll_buffer.size() - h)
      _buffer_location = _scroll_buffer.size() - h;
  }
  terminal->update();
}

/***********************
 * Chat::scroll_buffer *
 ***********************/

void Chat::scroll_buffer(unsigned int size) {
  _buffer_size = size;
  if (_buffer_size == 0)
    throw std::range_error("Invalid value for scroll buffer");
}

/*********************
 * Chat::auto_scroll *
 *********************/

void Chat::auto_scroll(bool on) {
  _auto_scroll = on;
}

/***************
 * Chat::print *
 ***************/

void Chat::print(const std::string &line) {
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
    *this << '\n'
          << curs::attron(curs::palette::pair(C_SYSMSG))
          << line
          << curs::attroff(curs::palette::pair(C_SYSMSG));

    // User join message.
    if (line.length() > 21) {
      if (line.compare(line.length() - 21, 21, " has joined the chat.") == 0)
        update_user_list = true;
    }

    // User left message.
    if (line.length() > 19) {
      if (line.compare(line.length() - 19, 19, " has left the chat.") == 0)
        update_user_list = true;
    }
  }
}

/******************************************************************************
 * class UserList
 */

/**********************
 * UserList::UserList *
 **********************/

UserList::UserList(int startx, int starty, int width, int height)
  : curs::window(startx, starty, width, height) {
}

/********************
 * UserList::update *
 ********************/

void UserList::update(curs::window &input) {
  std::string users;

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

  size_t pos;
  while ((pos = users.find(" ")) != users.npos)
    users[pos] = '\n';
  *this << curs::erase << curs::cursor(0, 0)
        << users << std::flush;
  input << std::flush;

  update_user_list = false;
}

/******************************************************************************
 * class Input
 */

 /****************
  * Input::Input *
  ****************/

Input::Input(int width, int height, int startx, int starty)
  : curs::window(width, height, startx, starty) {
  terminal->update();
}

/**********************
 * Input::operator () *
 **********************/

void Input::operator ()(curs::terminal &term, Chat &chat) {
  for (;;) {
    update();
    int ch = term.getch();
    switch (ch) {
    case ERR: // Keyboard input timeout
      return;
    case '\n':
      chatio << _line << std::endl;
      _line = "";
      update();
      usleep(100);
      return;
    case KEY_BACKSPACE:
      if (not _line.empty()) {
        _line.resize(_line.length() - 1);
        update();
      }
      break;
    case KEY_UP:
      chat.update(1);
      update();
      break;
    case KEY_DOWN:
      chat.update(-1);
      update();
      break;
    case KEY_PPAGE: { // Page up key
      int w, h;
      chat.size(w, h);
      chat.update(h);
      update();
      break;
    }
    case KEY_NPAGE: { // Page down key
      int w, h;
      chat.size(w, h);
      chat.update(-h);
      update();
      break;
    }
    default:
      if (isprint(ch)) {
        _line += ch;
        update();
      }
      break;
    }
  }
}

/*****************
 * Input::update *
 *****************/

void Input::update() {
  *this << curs::erase
        << curs::cursor(0, 0) << "> " << _line
        << std::flush;
  terminal->update();
}

/******************************************************************************
 */

Chat *chat_window;
UserList *users_window;
Input *input_window;

/******************
 * signal_handler *
 ******************/

static void signal_handler(int signal) {
  if (signal == SIGWINCH) terminal_resize++;
}

/*************
 * resize_ui *
 *************/

static void resize_ui() {
  // Update curses with the new terminal size information.
  endwin();
  refresh();

  int w, h;
  curs::window terminal_window;
  terminal_window.size(w, h);
  terminal_window << curs::erase;

  // Update the terminal window
  terminal_window << curs::attron(curs::palette::pair(C_TITLE) | A_BOLD)
                  << move(0, 0) << std::string(w, ' ');
  std::string title("ⲯⲮ Local Chat v" VERSION " Ⲯⲯ");
  //std::string title("Local Chat v" VERSION);
  terminal_window << curs::cursor((w - title.length()) / 2, 0) << title
                  << curs::attroff(curs::palette::pair(C_TITLE) | A_BOLD)
                  << curs::cursor(0, h - 2) << curs::hline(w, ACS_HLINE)
                  << curs::cursor(w - 11, 1) << curs::vline(h - 3, ACS_VLINE)
                  << std::flush;

  // Resize the chat window.
  *chat_window << curs::move(0, 1)
               << curs::resize(w - 12, h - 3);
  chat_window->update();

  // Resize the user list window.
  //users_window->erase();
  *users_window << curs::move(w - 10, 1)
                << curs::resize(10, h - 3)
                << std::flush;

  // Resize the input window.
  *input_window << curs::move(0, h - 1)
                << curs::resize(w, 1);
  input_window->update();

  update_user_list = true;
  terminal_resize--;
}

/******************************************************************************
 * Program entry point.
 */

int main(int argc, char *argv[]) {
  std::setlocale(LC_ALL, "");

  // Parse the command line options.
  int opt;
  unsigned int scrollback = 500;
  bool auto_scroll = false;

  // Get the command line arguments.
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
  curs::terminal term;
  terminal = &term;
  curs::palette::start();
  term.cbreak(true);
  term.echo(false);
  term.halfdelay(10);

  // The color theme.
  if (curs::palette::has_colors()) {
    curs::palette::pair(C_TITLE, curs::palette::WHITE,
                        curs::palette::BLUE);
    curs::palette::pair(C_USERNAME, curs::palette::CYAN, -1);
    curs::palette::pair(C_MYMESSAGE, curs::palette::GREEN, -1);
    curs::palette::pair(C_HLPMSG, curs::palette::CYAN, -1);
    curs::palette::pair(C_SYSMSG, curs::palette::BLUE, -1);
    curs::palette::pair(C_PRVMSG, curs::palette::YELLOW, -1);
  }

  // Setup the user interface.
  int w, h;
  curs::window term_win;
  term_win << curs::keypad(true);
  term_win.size(w, h);

  term_win << curs::attron(curs::palette::pair(C_TITLE) | A_BOLD)
           << curs::cursor(0, 0) << std::string(w, ' ');
  std::string title("🛰 Local Chat v" VERSION " 📡");
  term_win << curs::cursor((w - title.size()) / 2, 0) << title
           << curs::attroff(curs::palette::pair(C_TITLE) | A_BOLD)
           << curs::cursor(0, h - 2) << curs::hline(w, ACS_HLINE)
           << curs::cursor(w - 11, 1) << curs::vline(h - 3, ACS_VLINE)
           << std::flush;

  Chat chat(0, 1, w - 12, h - 3);
  chat.scroll_buffer(scrollback);
  chat.auto_scroll(auto_scroll);

  UserList users(w - 10, 1, 10, h - 3);

  Input input(0, h - 1, w, 1);
  input.update();
  term.update();

  // Setup for terminal resizing events.
  chat_window = &chat;
  users_window = &users;
  input_window = &input;
  signal(SIGWINCH, signal_handler);

  // The main loop.
  while(chatio) {
    input(term, chat);
    chat(input);
    if (update_user_list) users.update(input);
    if (terminal_resize) resize_ui();
    term.update();
  }

  return EXIT_SUCCESS;
}
