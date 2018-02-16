/*                                                                  -*- c++ -*-
 * Copyright © Ron R Wills
 * All rights reserved
 */

#include "nstream"
#include "curses"
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <pwd.h>
#include <unistd.h>

static int C_TITLE     = 1;
static int C_USERNAME  = 2;
static int C_MYMESSAGE = 3;
static int C_HLPMSG    = 4;
static int C_SYSMSG    = 5;
static int C_PRVMSG    = 6;

sockets::iostream chatio;
std::string sock_path = "/var/lib/lchat/sock";
std::string my_name;
bool update_user_list = false;

class Input : public curses::Window {
public:
  Input(int width, int height, int startx, int starty)
    : curses::Window(width, height, startx, starty) {
    move(0, 0);
    addch("> ");
    refresh();
  }

  void operator ()(curses::Terminal &term) {
    for (;;) {
      int ch = term.getch();
      switch (ch) {
      case ERR:
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
      default:
        if (isprint(ch)) {
          _line += ch;
          update();
        }
        break;
      }
    }
  }

protected:

  void update() {
    move(0, 2);
    clrtoeol();
    addch(0, 2, _line);
    refresh();
  }

private:
  std::string _line;
  bool _idle;
};

/**
 */

class Chat : public curses::Window {
public:
  Chat(int width, int height, int startx, int starty)
    : curses::Window(width, height, startx, starty) {
    scrollok(true);
    move(height - 1, 0);
    refresh();
  }

  void operator ()(curses::Window &input) {
    std::string in;
    for (;;) {
      try {
        getline(chatio, in);
        if (in.empty()) return;

        this->print(in);
        input.refresh();
      } catch (sockets::ionotready &err) {
        chatio.clear();
        return;
      }
    }
  }

protected:

  void print(const std::string &line);
};

class UserList : public curses::Window {
public:
  UserList(int width, int height, int startx, int starty)
    : curses::Window(width, height, startx, starty) { }

  void update(curses::Window &input) {
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

    this->clear();
    move(0,0);
    size_t pos;
    while ((pos = users.find(" ")) != users.npos)
      users[pos] = '\n';
    addch(users);

    refresh();
    input.refresh();
    update_user_list = false;
  }
};

/******************************************************************************
 * class Chat
 */

/***************
 * Chat::print *
 ***************/

void Chat::print(const std::string &line) {
  size_t pos = line.find(": ");
  if (line.compare(0, 2, "? ") == 0) {
    // Help message.
    addch('\n');
    attron(curses::Colors::pair(C_HLPMSG) | A_BOLD);
    addch(line.substr(2, line.length() - 2));
    attroff(curses::Colors::pair(C_HLPMSG) | A_BOLD);

  } else if (line.compare(0, 2, "! ") == 0) {
    // Private message.
    addch('\n');
    attron(curses::Colors::pair(C_USERNAME));
    addch(line.substr(2, pos - 1));
    attroff(curses::Colors::pair(C_USERNAME));
    attron(curses::Colors::pair(C_PRVMSG) | A_BOLD);
    addch(line.substr(pos + 1, line.length() - pos - 1));
    attroff(curses::Colors::pair(C_PRVMSG) | A_BOLD);

  } else if (line.compare(0, my_name.length() + 1, my_name + ":") == 0) {
    // A message that was sent by this user.
    addch('\n');
    attron(curses::Colors::pair(C_USERNAME));
    addch(line.substr(0, pos + 1));
    attroff(curses::Colors::pair(C_USERNAME));
    attron(curses::Colors::pair(C_MYMESSAGE));
    addch(line.substr(pos + 1, line.length() - pos));
    attroff(curses::Colors::pair(C_MYMESSAGE));

  } else if (pos != line.npos) {
    // Color the senders name.
    addch('\n');
    attron(curses::Colors::pair(C_USERNAME));
    addch(line.substr(0, pos + 1));
    attroff(curses::Colors::pair(C_USERNAME));
    addch(line.substr(pos + 1, line.length() - pos));

  } else {
    addch('\n');
    attron(curses::Colors::pair(C_SYSMSG));
    addch(line);
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
  refresh();
}

/******************************************************************************
 */

int main(int argc, char *argv[]) {

  // Parse the options.
  int opt;
  while ((opt = getopt(argc, argv, "s:")) != -1) {
    switch (opt) {
    case 's':
      sock_path = optarg;
      break;
    default:
      std::cerr << "Unknown option -" << optopt << std::endl;
      return EXIT_FAILURE;
    }
  }

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
  curses::Colors::start();
  term.cbreak();
  term.noecho();
  term.halfdelay(10);

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
  term_win.addch(0, (w - title.length()) / 2, title);
  term_win.attroff(curses::Colors::pair(C_TITLE) | A_BOLD);
  term_win.hline(1, 0, ACS_HLINE, w);
  term_win.hline(h - 2, 0, ACS_HLINE, w);
  term_win.refresh();

  Chat chat(w - 12, h - 4, 0, 2);
  term_win.vline(2, w - 11, ACS_VLINE, h - 4);

  UserList users(10, h - 4, w - 10, 2);

  Input input(w, 1, 0, h - 1);

  // The main loop.
  while(chatio) {
    input(term);
    chat(input);
    if (update_user_list) users.update(input);
  }

  return EXIT_SUCCESS;
}
