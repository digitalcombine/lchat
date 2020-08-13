/*                                                                  -*- c++ -*-
 * Copyright (c) 2018-2020 Ron R Wills <ron.rwsoft@gmail.com>
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
#include <sstream>
#include <list>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <clocale>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

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

// Global data.
static sockets::iostream chatio;
static std::string sock_path = STATEDIR "/sock";
static std::string my_name;

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

  bool read_server();

  void redraw();

  static bool auto_scroll;
  static unsigned int scrollback;

  friend class status;

protected:

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
  void refresh_users(const std::string &list);

  void operator()();

private:
  void resize_event();

private:
  chat _chat;
  input _input;

  int _userlist_width;
  bool _refresh_users;
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
  _buffer_location(0) {
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
 * chat::read_server *
 *********************/

bool chat::read_server() {
  std::string line;
  for (;;) {
    try {
      // Attempt to read a line from the server.
      getline(chatio, line);
      if (line.empty()) {
        return false; // Nothing returned so return.
      }

      if (line.compare(0, 2, "~ ") == 0) {
        // User list update
        _lchat->refresh_users(line.substr(2, line.length() - 2));
        return true;
      }

      // User join message.
      if (line.length() > 21) {
        if (line.compare(line.length() - 21, 21, " has joined the chat.") == 0) {
          chatio << "/who" << std::endl;
        }
      }

      // User left message.
      if (line.length() > 19) {
        if (line.compare(line.length() - 19, 19, " has left the chat.") == 0) {
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

    } catch (sockets::ionotready &err) {
      // Nothing to read on the socket, so return.
      chatio.clear();
      return false;
    }
  }
  return true;
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
    for (int c = 0; c < h and it != _scroll_buffer.rend(); c++, it++) {
      this->draw(*it);
    }
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

/********************
 * userlist::update *
 ********************/

void userlist::update(const std::string &list) {
  // Clear the old user list.
  _users.clear();

  // Repopulate the user list with the servers response.
  size_t last = 0, pos;
  while ((pos = list.find(" ", last)) != list.npos) {
    _users.push_back(list.substr(last, pos));
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
 * class status
 */

status::status(chat &ch, userlist &ul, int x, int y, int width, int height)
  : curs::window(x, y, width, height), _chat(&ch), _userlist(&ul) {
  *this << curs::scrollok(false);
}

void status::redraw() {
  *this << curs::attron(curs::palette::pair(C_STATUS))
        << curs::bkgd(' ', curs::palette::pair(C_STATUS))
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
}

/******************************************************************************
 * class input
 */

 /****************
  * input::input *
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
 * input::key_event *
 ********************/

static bool busy = false;

#define CTRL(c) ((c) & 037)

void input::key_event(int ch) {
  switch (ch) {
  case ERR: // Keyboard input timeout
    if (not busy) usleep(1000);
    return;

  case '\n': // Send the line to the server and reset the input.
    chatio << _line << std::endl;
    _line = "";
    _insert = 0;
    break;

  case CTRL('a'):
    chat::auto_scroll = not chat::auto_scroll;
    break;

  case KEY_BACKSPACE:
    if (not _line.empty() and _insert > 0) {
      _line.erase(_insert - 1, 1);
      _insert--;
    }
    break;

  case KEY_DC:
    if (not _line.empty() and _insert < _line.size()) {
      _line.erase(_insert, 1);
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
    /* XXX Current isprint doesn't work with multibyte unicode characters, but
     * for the moment our _insert pointers is smart enough to manage multibyte
     * characters.
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
  _userlist(width() - 11, 1, 11, height() - 3),
  _status(_chat, _userlist, 0, height() - 2, width(), 1) {

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
    curs::palette::pair(C_STATUS, curs::palette::WHITE,
                        curs::palette::BLUE);
    curs::palette::pair(C_STATUS_ON, curs::palette::YELLOW,
                        curs::palette::BLUE);
    curs::palette::pair(C_STATUS_OFF, curs::palette::RED,
                        curs::palette::BLUE);
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
  // Our main application loop.
  while (chatio) {
    busy = _chat.read_server();

    _status.redraw();
    _input.redraw();
    curs::terminal::update();

    curs::events::process();

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

  _userlist << curs::move(w - _userlist_width, 1)
            << curs::resize(_userlist_width, h - 3);
  _userlist.redraw();

  _status << curs::move(0, h - 2)
          << curs::resize(w, 1);
  _status.redraw();

  _input << curs::move(0, h - 1)
         << curs::resize(w, 1);
  _input.redraw();
}

/****************
 * lchat::_draw *
 ****************/

void lchat::_draw() {
  int w, h;
  size(w, h);

  // Draw the frames around our windows.
  *this << curs::attron(curs::palette::pair(C_TITLE) | A_BOLD)
        << curs::cursor(0, 0) << "📡" << std::string(w - 1, ' ');
  std::string title("Local Chat v" VERSION);
  *this << curs::cursor((w - title.size()) / 2, 0) << title
        << curs::attroff(curs::palette::pair(C_TITLE) | A_BOLD)
        << curs::cursor(w - (_userlist_width + 1), 1)
        << curs::vline(h - 3, ACS_VLINE)
        << std::flush;
}

/***********
 * version *
 ***********/

static void version() {
  std::cout << "Local Chat v" VERSION << "\n"
            << "Copyright © 2018-2020 Ron R Wills <ron@digitalcombine.ca>.\n"
            << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\n"
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
            << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n\n"
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
  } catch (std::exception &err) {
    std::cerr << err.what() << std::endl;
    return EXIT_FAILURE;
  }

  if (mesg_stdin) {

    std::string line;
    while (getline(std::cin, line)) {
      send_message(line);
    }
    quit_chat();

  } else if (not message.empty()) {
    // If the -m option was given then send the message.

    send_message(message);
    quit_chat();

  } else if (not bot_command.empty()) {
    // If the -b option was given then run the bot.
    chatio >> sockets::block;
    return bot(bot_command);

  } else {
    // Interactive user interface.

    // Setup the terminal.
    curs::terminal::initialize();
    curs::terminal::cbreak(true);
    curs::terminal::echo(false);
    curs::terminal::halfdelay(10);

    try {
      lchat chat_ui;
      chatio << "/who" << std::endl;

      chat_ui();
    } catch (std::exception &err) {
      curs::terminal::restore();
      std::cerr << err.what() << std::endl;
      return EXIT_FAILURE;
    }

    // Restore the terminal.
    curs::terminal::restore();
  }

  return EXIT_SUCCESS;
}
