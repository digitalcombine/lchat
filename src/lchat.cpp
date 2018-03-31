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
 * along with the Network Streams Library.  If not, see
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
curses::Terminal *terminal;

// Global event flags.
bool update_user_list = false;
int terminal_resize = 0;

/**
 */

class Chat : public curses::Window {
public:
  Chat(int width, int height, int startx, int starty);

  void operator ()(curses::Window &input);

  void update();
  void update(int offset);

protected:

  void print(const std::string &line);

private:
  std::list<std::string> _scroll_buffer;
  unsigned int _buffer_size;
  unsigned int _buffer_location;
};

/**
 */

class UserList : public curses::Window {
public:
  UserList(int width, int height, int startx, int starty);

  void update(curses::Window &input);
};

/**
 */

class Input : public curses::Window {
public:
  Input(int width, int height, int startx, int starty);

  void operator ()(curses::Terminal &term, Chat &chat);
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

Chat::Chat(int width, int height, int startx, int starty)
  : curses::Window(width, height, startx, starty), _buffer_size(500),
    _buffer_location(0) {
  scrollok(true);
  move(height - 1, 0);
  noutrefresh();
}

/*********************
 * Chat::operator () *
 *********************/

void Chat::operator ()(curses::Window &input) {
  std::string in;
  for (;;) {
    try {
      getline(chatio, in);
      if (in.empty()) return;

      _scroll_buffer.push_front(in);
      if (_scroll_buffer.size() > _buffer_size)
        _scroll_buffer.resize(_buffer_size);

      update();
      input.noutrefresh();
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
  getmaxyx(h, w);

  erase();
  move(h - 1, 0);

  if (not _scroll_buffer.empty()) {
    auto it = _scroll_buffer.rend();
    for (int c = 0;
         c < h + (int)_buffer_location and it != _scroll_buffer.rbegin();
         c++, it--);

    for (int c = 0; c < h and it != _scroll_buffer.rend(); c++, it++)
      this->print(*it);
  }

  noutrefresh();
}

void Chat::update(int offset) {
  int w, h;
  getmaxyx(h, w);

  if ((int)_buffer_location + offset < 0) {
    _buffer_location = 0;
  } else {
    _buffer_location += offset;

    if (_buffer_location > _scroll_buffer.size() - h)
      _buffer_location = _scroll_buffer.size() - h;
  }
  update();
}

/***************
* Chat::print *
***************/

void Chat::print(const std::string &line) {
  size_t pos = line.find(": ");
  if (line.compare(0, 2, "? ") == 0) {
    // Help message.
    addch('\n');
    attron(curses::Colors::pair(C_HLPMSG) | A_BOLD);
    printw(line.substr(2, line.length() - 2).c_str());
    attroff(curses::Colors::pair(C_HLPMSG) | A_BOLD);

  } else if (line.compare(0, 2, "! ") == 0) {
    // Private message.
    addch('\n');
    attron(curses::Colors::pair(C_USERNAME));
    printw(line.substr(2, pos - 1).c_str());
    attroff(curses::Colors::pair(C_USERNAME));

    attron(curses::Colors::pair(C_PRVMSG) | A_BOLD);
    printw(line.substr(pos + 1, line.length() - pos - 1).c_str());
    attroff(curses::Colors::pair(C_PRVMSG) | A_BOLD);

  } else if (line.compare(0, my_name.length() + 1, my_name + ":") == 0) {
    // A message that was sent by this user.
    addch('\n');
    attron(curses::Colors::pair(C_USERNAME));
    printw(line.substr(0, pos + 1).c_str());
    attroff(curses::Colors::pair(C_USERNAME));

    attron(curses::Colors::pair(C_MYMESSAGE));
    printw(line.substr(pos + 1, line.length() - pos).c_str());
    attroff(curses::Colors::pair(C_MYMESSAGE));

  } else if (pos != line.npos) {
    // Color the senders name.
    addch('\n');
    attron(curses::Colors::pair(C_USERNAME));
    printw(line.substr(0, pos + 1).c_str());
    attroff(curses::Colors::pair(C_USERNAME));

    printw(line.substr(pos + 1, line.length() - pos).c_str());

  } else {
    addch('\n');
    attron(curses::Colors::pair(C_SYSMSG));
    printw(line.c_str());
    attroff(curses::Colors::pair(C_SYSMSG));

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

UserList::UserList(int width, int height, int startx, int starty)
 : curses::Window(width, height, startx, starty) {
}

/********************
 * UserList::update *
 ********************/

void UserList::update(curses::Window &input) {
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

  erase();
  move(0, 0);
  size_t pos;
  while ((pos = users.find(" ")) != users.npos)
    users[pos] = '\n';
  printw(users.c_str());

  noutrefresh();
  input.noutrefresh();
  update_user_list = false;
}

/******************************************************************************
 * class Input
 */

 /****************
  * Input::Input *
  ****************/

Input::Input(int width, int height, int startx, int starty)
  : curses::Window(width, height, startx, starty) {
  update();
}

/**********************
 * Input::operator () *
 **********************/

void Input::operator ()(curses::Terminal &term, Chat &chat) {
  for (;;) {
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
      chat.getmaxyx(h, w);
      chat.update(h);
      update();
      break;
    }
    case KEY_NPAGE: { // Page down key
      int w, h;
      chat.getmaxyx(h, w);
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
  erase();
  printw(0, 0, "> %s", _line.c_str());
  noutrefresh();
  terminal->doupdate();
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
  curses::Window terminal_window;
  terminal_window.getmaxyx(h, w);
  terminal_window.erase();

  // Update the terminal window
  terminal_window.attron(curses::Colors::pair(C_TITLE) | A_BOLD);
  terminal_window.addch(0, 0, std::string(w, ' '));
  std::string title("Local Chat v" VERSION);
  terminal_window.printw(0, (w - title.length()) / 2, title.c_str());
  terminal_window.attroff(curses::Colors::pair(C_TITLE) | A_BOLD);
  terminal_window.hline(h - 2, 0, ACS_HLINE, w);
  terminal_window.vline(1, w - 11, ACS_VLINE, h - 3);
  terminal_window.noutrefresh();

  // Resize the chat window.
  chat_window->mvwin(1, 0);
  chat_window->resize(h - 3, w - 12);
  chat_window->update();

  // Resize the user list window.
  //users_window->erase();
  users_window->mvwin(1, w - 10);
  users_window->resize(h - 3, 10);
  users_window->noutrefresh();

  // Resize the input window.
  input_window->mvwin(h - 1, 0);
  input_window->resize(1, w);
  input_window->update();

  update_user_list = true;
  terminal_resize--;
}

/******************************************************************************
 * Program entry point.
 */

int main(int argc, char *argv[]) {

  // Parse the command line options.
  int opt;
  while ((opt = getopt(argc, argv, "s:")) != -1) {
    switch (opt) {
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
  curses::Terminal term;
  terminal = &term;
  curses::Colors::start();
  term.cbreak();
  term.noecho();
  term.halfdelay(10);

  // The color theme.
  if (curses::Colors::has_colors()) {
    curses::Colors::pair(C_TITLE, curses::Colors::WHITE,
                         curses::Colors::BLUE);
    curses::Colors::pair(C_USERNAME, curses::Colors::CYAN,
                         curses::Colors::BLACK);
    curses::Colors::pair(C_MYMESSAGE, curses::Colors::GREEN,
                         curses::Colors::BLACK);
    curses::Colors::pair(C_HLPMSG, curses::Colors::CYAN,
                         curses::Colors::BLACK);
    curses::Colors::pair(C_SYSMSG, curses::Colors::BLUE,
                         curses::Colors::BLACK);
    curses::Colors::pair(C_PRVMSG, curses::Colors::YELLOW,
                         curses::Colors::BLACK);
  }

  // Setup the user interface.
  int w, h;
  curses::Window term_win;
  term_win.keypad(true);
  term_win.getmaxyx(h, w);

  term_win.attron(curses::Colors::pair(C_TITLE) | A_BOLD);
  term_win.addch(0, 0, std::string(w, ' '));
  std::string title("Local Chat v" VERSION);
  term_win.printw(0, (w - title.length()) / 2, title.c_str());
  term_win.attroff(curses::Colors::pair(C_TITLE) | A_BOLD);
  term_win.hline(h - 2, 0, ACS_HLINE, w);
  term_win.noutrefresh();

  Chat chat(w - 12, h - 3, 0, 1);
  term_win.vline(1, w - 11, ACS_VLINE, h - 3);

  UserList users(10, h - 3, w - 10, 1);

  Input input(w, 1, 0, h - 1);
  term.doupdate();

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
    term.doupdate();
  }

  return EXIT_SUCCESS;
}
