/*                                                                  -*- c++ -*-
 * Copyright © 2018-2020 Ron R Wills <ron@digitalcombine.ca>
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
#include <iostream>
#include <set>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/un.h>
#include <sys/ucred.h>

#ifndef LOG_ERROR
#define LOG_ERROR LOG_ERR
#endif

#endif
#ifdef DEBUG
#include <fcntl.h>
#endif

// Global settings.
static std::string sock_path = STATEDIR "/sock";
static std::string cwd_path = STATEDIR;
static std::string chat_group;
static std::string chat_user;
static bool running = true;

/** Client Connection Class.
 */
class ChatClient : public sockets::connection {
public:
  ChatClient(int sockfd) : sockets::connection(sockfd) {}
  virtual ~ChatClient() noexcept;

  std::string name() const { return _name; }

protected:
  std::string _name;

  virtual void connect(int sockfd);
  virtual void recv();

private:
  void send_private(const std::string &who, const std::string &mesg);
};

sockets::server<ChatClient> chat_server;

/***************
 * connections *
 ***************/

static unsigned int connections(const std::string &name) {
  /* Count the number of connections a user has to the chat server.
   */
  unsigned int count = 0;

  // Count the connections.
  for (auto &it: chat_server)
    if ((dynamic_cast<ChatClient *>(it.second))->name() == name)
      count++;

#ifdef DEBUG
  std::clog << "User " << name << " has " << count << " connections."
              << std::endl;
#endif

  return count;
}

/******************************************************************************
 * class ChatClient
 */

/***************************
 * ChatClient::~ChatClient *
 ***************************/

ChatClient::~ChatClient() noexcept {}

/***********************
 * ChatClient::connect *
 ***********************/

void ChatClient::connect(int sockfd) {

  // Prepare the credentials of the user that connected over the unix socket.
#if defined(__FreeBSD__)
  struct xucred ucred;
  int len = sizeof(struct xucred);
  int oval;

  if (setsockopt(sockfd, 0, LOCAL_CREDS, &oval, sizeof(oval)) == -1) {
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERROR),
           "Unable to determine connected peer: %s", strerror(errno));
#ifdef DEBUG
    std::cerr << "Unable to determine connected peer: " << strerror(errno)
              << std::endl;
#endif
    throw sockets::exception(
      std::string("Unable to determine connected peer: ") +
      strerror(errno));
  }
#else
  struct ucred ucred;
  int len = sizeof(struct ucred);
#endif

  // Log a new connection was made.
  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO), "New client connected");
#ifdef DEBUG
  std::clog << "New client connected" << std::endl;
#endif

  // Get the clients UID.
#if defined(__FreeBSD__)
  if (getsockopt(sockfd, 0, LOCAL_PEERCRED,
                 &ucred, (socklen_t *)&len) == -1) {
#else
  if (getsockopt(sockfd, SOL_SOCKET, SO_PEERCRED,
                 &ucred, (socklen_t *)&len) == -1) {
#endif
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE),
           "Unable to determine connected peer: %s", strerror(errno));
    throw sockets::exception(
      std::string("Unable to determine connected peer: ") +
      strerror(errno));
  }

  // Now get the clients username.
#if defined(__FreeBSD__)
  struct passwd *pw_entry = getpwuid(ucred.cr_uid);
#else
  struct passwd *pw_entry = getpwuid(ucred.uid);
#endif
  if (pw_entry == NULL) {
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE),
           "Unable to determine connected user: %s", strerror(errno));
#ifdef DEBUG
    std::cerr << "Unable to determine connected peer: " << strerror(errno)
              << std::endl;
#endif
    throw sockets::exception(
      std::string("Unable to determine connected user: ") +
      strerror(errno));
  }

  // Log the connection.
  _name = pw_entry->pw_name;
  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO),
         "%s has joined the chat", _name.c_str());
#ifdef DEBUG
  std::clog << _name << " has joined the chat." << std::endl;
#endif

  /* Send out notices about the new connection if it is the users first
   * connection to the server. If there is already another connection don't
   * send this message, it gets way to spammy.
   */
  if (connections(_name) == 1) {
    for (auto &it: chat_server) {
      (sockets::iostream &)(*it.second) << _name << " has joined the chat."
                                        << std::endl;
    }
    ios << "? Type '/help' to get a list of chat commands." << std::endl;
  }
}

/********************
 * ChatClient::recv *
 ********************/

void ChatClient::recv() {
  std::string in;

#ifdef DEBUG
  std::clog << "Client recv from " << _name << std::endl;
#endif

  while (getline(ios, in, '\n')) {

#ifdef DEBUG
    std::clog << "From " << _name << ": " << in << std::endl;
#endif

    if (in[0] == '/') {
      // Parse the command sent.
      size_t pos = in.find(' ');
      std::string cmd(in.substr(1, in.npos));
      if (pos != in.npos) cmd = in.substr(1, pos - 1);

      // Server side commands.

      if (cmd == "quit" or cmd == "exit") {
        // Close the connection.
        if (connections(_name) < 2) {
          for (auto &it: chat_server) {
            (sockets::iostream &)(*it.second) << _name
                                              << " has left the chat."
                                              << std::endl;
          }
        }
        ios.clear();
        this->close();
        return;

      } else if (cmd == "who") {
        // Request a list of connected users.
        std::string result;
        std::set<std::string> people;

        // Use a std::set to prevent duplicates.
        for (auto &it: chat_server) {
          people.insert((dynamic_cast<ChatClient *>(it.second))->name());
        }
        for (auto &it: people) {
          result += it + " ";
        }
        ios << "~ " << result << std::endl;

      } else if (cmd == "help") {
        // Help requested.
        ios << "? All server commands start with the '/' character.\n"
            << "? /help                  - Displays this help dialog.\n"
            << "? /who                   - Displays a list of all the users in "
            << "the chat.\n"
            << "? /quit or /exit         - Leaves the chat.\n"
            << "? /version or /about     - Version information about this "
            << "server.\n"
            << "? /msg user message...\n"
            << "? /priv user message...\n"
            << "? /query user message... - Sends a private message to user.\n"
            << std::endl;

      } else if (cmd == "version" or cmd == "about") {
        // Server information.
        ios << "Local Chat Server v" << VERSION << "\n"
            << "Copyright (c) 2018 Ron R Wills <ron.rwsoft@gmail.com>\n"
            << "License BSD: 3-Clause BSD License "
            << "<https://opensource.org/licenses/BSD-3-Clause>.\n"
            << "This is free software, you are free to change and "
            << "redistribute it.\n"
            << "There is NO WARRANTY, to the extent permitted by law."
            << std::endl;

      } else if (cmd == "msg" or cmd == "priv" or cmd == "query") {
        // Private message.
        std::string pmesg(in.substr(pos + 1, in.npos));
        size_t piv = pmesg.find(' ');

        if (piv != pmesg.npos) {
          send_private(pmesg.substr(0, piv), pmesg.substr(piv + 1,
                                                          pmesg.npos));
        } else {
          ios << "? Invalid private message, the command is:\n"
              << "? /" << cmd << " user message..." << std::endl;
        }

      } else {
        ios << "? Unknown chat command '" << in << "'\n"
            << "? Type '/help' to get a list of chat commands." << std::endl;
      }

    } else {
      // A message for everyone to see.
      for (auto &it: chat_server) {
        (sockets::iostream &)(*it.second) << _name << ": " << in
                                          << std::endl;
      }
    }
  }

  if (ios.eof()) {
    // If the socket closed from the client side.
#ifdef DEBUG
    std::clog << "Client closed the socket" << std::endl;
#endif
    if (connections(_name) < 2) {
      for (auto &it: chat_server) {
        (sockets::iostream &)(*it.second) << _name
                                          << " has left the chat."
                                          << std::endl;
      }
    }
    ios.clear();
    this->close();
    return;

  } else {
    // ionotready will be thrown, so clear it.
    ios.clear();
  }
}

/****************************
 * ChatClient::send_private *
 ****************************/

void ChatClient::send_private(const std::string &who,
                              const std::string &mesg) {
  bool has_user = false;

    // Send the private message to all the users connections.
  for (auto &it: chat_server) {
    if ((dynamic_cast<ChatClient *>(it.second))->name() == who) {
      (sockets::iostream &)(*it.second) << "! "
                                        << _name << ": "
                                        << mesg << std::endl;
      has_user = true;
    }
  }

  if (has_user) {
    // If a message was sent, send in to all our connections as well.
    for (auto &it: chat_server) {
      if ((dynamic_cast<ChatClient *>(it.second))->name() == _name) {
        (sockets::iostream &)(*it.second) << "! ^"
                                          << who << ": "
                                          << mesg << std::endl;
      }
    }
  } else {
    // If a message wasn't sent, send an error message our connections.
    for (auto &it: chat_server) {
      if ((dynamic_cast<ChatClient *>(it.second))->name() == _name) {
        (sockets::iostream &)(*it.second) << "User "
                                          << who
                                          << " is not available, "
                                          << "private message not sent:\n "
                                          << mesg << std::endl;
      }
    }
  }
}

/*******************
 * test_for_server *
 *******************/

static bool test_for_server() {
  /*  If a chat server goes down hard, the unix domain socket could have been
   * left in the filesystem. In this case, when we attempt to create the socket
   * we will fail to bind to it.
   *  Here we attempt to connect as a client to see if we can talk to the
   * chat server. If we can't then we have a dead socket to clean up.
   */
  static sockets::iostream chatio;

#ifdef DEBUG
    std::clog << "Testing for existing chat server" << std::endl;
#endif

  try {
    chatio.open(sock_path);

    chatio << "/who" << std::endl;
  } catch (...) {
#ifdef DEBUG
    std::clog << "Failed to talk to server, possible dead socket" << std::endl;
#endif
    return false;
  }

  return true;
}

/********************
 * open_unix_socket *
 ********************/

static void open_unix_socket(bool second_attempt = false) {
  /*  Here we attempt to create and open the unix domain socket for the chat
   * server. During hard shutdowns a dead socket could remain on the filesystem
   * preventing the chat server from starting. So here we make two attempts to
   * get things rolling. If the first attempt fails, we check for a dead
   * socket, delete it if necessary then make a second final attempt.
   */

  try {
    // Open the unix domain socket.
    chat_server.open(sock_path);

  } catch (std::exception &err) {
    // We failed to make the socket and listen on it.

    if (not second_attempt) {
      syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING), "%s", err.what());
      syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING), "Attempting to recover");
#ifdef DEBUG
      std::cerr << err.what() << "\n Attempting to recover" << std::endl;
#endif
    }

    if (not second_attempt and not test_for_server()) {
      // Looks like we have a dead socket.
#ifdef DEBUG
      std::clog << "Dead socket found, " << sock_path << ", cleaning it up"
                << std::endl;
#endif
      syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING),
             "Dead socket found, %s, cleaning it up",
             sock_path.c_str());

      // Remove the socket and try to connect again.
      if (remove(sock_path.c_str()) == -1) {
        std::cerr << strerror(errno) << std::endl;
      }
      open_unix_socket(true);

    } else {
      // Complete failure to create the server socket.
      throw;
    }
  }
}

/**********
 * daemon *
 **********/

static void daemon() {
  /* Here we disconnect ourself from any controlling tty or parent process to
   * put ourself into the background.
   */
  pid_t pid, sid;

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO),
         "Forking server creating daemon");
#ifdef DEBUG
  std::clog << "Forking server creating daemon" << std::endl;
#endif

  // Fork to create our new process.
  pid = fork();
  if (pid < 0) {
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING),
           "Failed to fork: %s", strerror(errno));
#ifdef DEBUG
    std::cerr << "Failed to fork: " << strerror(errno) << std::endl;
#endif
    throw std::runtime_error(std::string("Failed to fork: ") +
                             strerror(errno));
  }
  if (pid > 0) {
    // We got a child process so exit the original process.
    exit(EXIT_SUCCESS);
  }

  umask(0117);

  // Set the new process as the new session owner.
  sid = setsid();
  if (sid < 0) {
    throw std::runtime_error(std::string("Failed to set session id: ") +
                             strerror(errno));
  }

  // Switch to our new working directory.
  if ((chdir(cwd_path.c_str())) < 0) {
    throw std::runtime_error(std::string("Failed to change working directory: ") +
                             strerror(errno));
  }

  // Close all the standard IO files.
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}

/****************
 * change_group *
 ****************/

static void change_group(const std::string &group_name) {
  /* Change the group ownership of the domain socket and the group which we are
   * executing under.
   */
  struct group *group_entry = getgrnam(group_name.c_str());

  // Lookup the system group entry.
  if (group_entry == NULL) {
    throw std::runtime_error(std::string("Failed to read group ") +
                             group_name +
                             " entry: " +
                             strerror(errno));
  }

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO),
         "Changing to group %d/%s", group_entry->gr_gid, group_name.c_str());

  // Change the group ownership of the Unix domain socket.
  if (chown(sock_path.c_str(), -1, group_entry->gr_gid) == -1) {
    throw std::runtime_error(std::string("Failed to change socket group: ") +
                             strerror(errno));
  }

  // Change to the group ourselves.
  if (setgid(group_entry->gr_gid) == -1) {
    /* We don't consider a failure here unrecoverable but we do log the fact we
     * could change groups.
     */
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING),
           "Failed to change daemon group: %s", strerror(errno));
  }
}

/****************
 * change_user *
 ****************/

static void change_user(const std::string &user_name) {
  /* Change the user ownership of the domain socket and the user which we are
   * executing under.
   */
  struct passwd *user_entry = getpwnam(user_name.c_str());

  // Lookup the system user entry.
  if (user_entry == nullptr) {
    throw std::runtime_error(std::string("Failed to read user ") +
                             user_name +
                             " entry: " +
                             strerror(errno));
  }

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO),
         "Changing to user %d/%s", user_entry->pw_uid, user_name.c_str());

  // Change the ownership of the Unix domain socket.
  if (chown(sock_path.c_str(), user_entry->pw_uid, -1) == -1) {
    /* We don't consider a failure here unrecoverable but we do log the fact we
     * could change groups.
     */
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING),
           "Failed to change socket user: %s", strerror(errno));
  }

  /* We set the effective user id here so we can later restore our originial
   * user id to clean up the socket later.
   */
  if (seteuid(user_entry->pw_uid) == -1) {
    /* We don't consider a failure here unrecoverable but we do log the fact we
     * could change groups.
     */
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING),
           "Failed to change daemon user: %s", strerror(errno));
  }
}

/***************
 * sig_handler *
 ***************/

static void sig_handler(int signum) {
  /* Shutdown nicely to any standard signals sent to us.
   */
#ifdef DEBUG
  std::clog << "Signal " << signum << " received." << std::endl;
#endif

  switch (signum) {
  case SIGHUP:
  case SIGTERM:
  case SIGINT:
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO),
           "Interrupt signal, shutting down");
#ifdef DEBUG
    std::clog << "Interrupt signal, shutting down" << std::endl;
#endif
    running = false;
    break;
  }
}

/***********
 * version *
 ***********/

static void version() {
  std::cout << "Local Chat Dispatcher v" VERSION << "\n"
            << "Copyright © 2018-2019 Ron R Wills <ron@digitalcombine.ca>.\n"
            << "License BSD: 3-Clause BSD License <https://opensource.org/licenses/BSD-3-Clause>.\n\n"
            << "This is free software: you are free to change and redistribute it.\n"
            << "There is NO WARRANTY, to the extent permitted by law."
            << std::endl;
}

/********
 * help *
 ********/

static void help() {
  std::cout << "Local Chat Dispatcher v" VERSION << "\n"
            << "  lchatd [-d] [-s path] [-u user] [-g group] [-w path]\n"
            << "  lchatd -V\n"
            << "  lchatd -h|-?\n\n"
            << "Copyright © 2018-2019 Ron R Wills <ron@digitalcombine.ca>.\n"
            << "License BSD: 3-Clause BSD License <https://opensource.org/licenses/BSD-3-Clause>.\n\n"
            << "This is free software: you are free to change and redistribute it.\n"
            << "There is NO WARRANTY, to the extent permitted by law."
            << std::endl;
}

/******************************************************************************
 * Entry Point
 */

int main(int argc, char *argv[]) {
  bool fork_daemon = false;

  // Get the command line options.
  int opt;
  while ((opt = getopt(argc, argv, "dg:s:w:u:Vh?")) != -1) {
    switch (opt) {
    case 'd':
      fork_daemon = true;
      break;
    case 'g':
      chat_group = optarg;
      break;
    case '?':
    case 'h':
      help();
      return EXIT_SUCCESS;
    case 's':
      sock_path = optarg;
      break;
    case 'u':
      chat_user = optarg;
      break;
    case 'V':
      version();
      return EXIT_SUCCESS;
    case 'w':
      cwd_path = optarg;
      break;
    default:
      std::cerr << "Unknown option -" << (char)optopt << std::endl;
      help();
      return EXIT_FAILURE;
    }
  }

  // Open the logging facility.
  openlog("lchatd", LOG_CONS, LOG_PID);

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO), "Starting");
#ifdef DEBUG
  std::clog << "Starting" << std::endl;
#endif

  uid_t saved_uid = getuid();

  try {
    // Possible fork as an independant daemon.
    if (fork_daemon) daemon();
    else umask(0117);

    open_unix_socket();

    // Change the group of the socket and of us.
    if (not chat_group.empty())
      change_group(chat_group);

    // Change the user of the socket and of us.
    if (not chat_user.empty()) {
      change_user(chat_user);
    } else if (getuid() == 0) {
      // If we're root reduce ourself to nobody.
      change_user("nobody");
    }

  } catch (std::exception &err) {
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), "%s", err.what());
#ifdef DEBUG
    std::cerr << err.what() << std::endl;
#endif
    return EXIT_FAILURE;
  }

  // Setup some signal handlers
  signal(SIGINT,  sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGHUP,  sig_handler);
  // Needed for clients that suddenly disconnect.
  signal(SIGPIPE, SIG_IGN);

  // Log the fact we're up and runnging.
  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO),
         "Local chat listening on socket %s", sock_path.c_str());
#ifdef DEBUG
  std::clog << "Local chat listening on socket " << sock_path << std::endl;
#endif

  // The main loop.
  while (running) {
    chat_server.process_requests();
  }

  // Cleanup.
  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO), "Cleaning up socket");
  closelog();
  chat_server.close();
  if (setuid(saved_uid) == -1) {
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE),
           "Unable to restore UID: %s", strerror(errno));
  }
  if (unlink(sock_path.c_str()) == -1) {
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_WARNING),
           "Failed to clean up socket %s: %s",
           sock_path.c_str(),
           strerror(errno));
  }

  return EXIT_SUCCESS;
}
