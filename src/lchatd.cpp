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
#endif

// Global settings.
static std::string sock_path = "/var/lib/lchat/sock";
static std::string cwd_path = "/var/lib/lchat";
static std::string sock_group;
static bool running = true;

class ChatClient : public sockets::connection {
public:
  ChatClient(int sockfd) : sockets::connection(sockfd) {}
  virtual ~ChatClient() throw() {}

  std::string name() const { return _name; }

protected:
  std::string _name;

  virtual void connect(int sockfd);
  virtual void recv();

private:
  bool is_private(const std::string &mesg);
};

sockets::server<ChatClient> chat_server;

/******************************************************************************
 * class ChatClient
 */

/***********************
 * ChatClient::connect *
 ***********************/

void ChatClient::connect(int sockfd) {
#if defined(__FreeBSD__)
  struct xucred ucred;
  int len = sizeof(struct xucred);
  int oval;

  if (setsockopt(sockfd, 0, LOCAL_CREDS, &oval, sizeof(oval)) == -1) {
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
           "Unable to determine connected peer: %s", strerror(errno));
    throw sockets::exception(
      std::string("Unable to determine connected peer: ") +
      strerror(errno));
  }
#else
  struct ucred ucred;
  int len = sizeof(struct ucred);
#endif

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO), "New client connection");

  // Get the clients UID.
#if defined(__FreeBSD__)
  if (getsockopt(sockfd, 0, LOCAL_PEERCRED,
                 &ucred, (socklen_t *)&len) == -1) {
#else
  if (getsockopt(sockfd, SOL_SOCKET, SO_PEERCRED,
                 &ucred, (socklen_t *)&len) == -1) {
#endif
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
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
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR),
           "Unable to determine connected user: %s", strerror(errno));
    throw sockets::exception(
      std::string("Unable to determine connected user: ") +
      strerror(errno));
  }

  // Log the connection.
  _name = pw_entry->pw_name;
  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE),
         "%s has joined the chat", _name.c_str());

  // Send out notices about the new connection.
  for (auto &it: chat_server) {
    (sockets::iostream &)(*it.second) << _name << " has joined the chat."
                                      << std::endl;
  }
  ios << "? Type '/help' to get a list of chat commands." << std::endl;
}

/********************
 * ChatClient::recv *
 ********************/

void ChatClient::recv() {
  std::string in;

  // Get the command from the client.
  getline(ios, in);

  if (!in.empty()) {
    if (in[0] == '/') {

      // Server side commands.
      if (in == "/quit" or in == "/exit") {
        for (auto &it: chat_server) {
          (sockets::iostream &)(*it.second) << _name
                                            << " has left the chat."
                                            << std::endl;
        }
        this->close();

      } else if (in == "/who") {
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

      } else if (in == "/help") {
        ios << "? All server commands start with the '/' character.\n"
            << "? /help               - Displays this help dialog.\n"
            << "? /who                - Displays a list of all the users in "
            << "the chat.\n"
            << "? /quit or /exit      - Leaves the chat.\n"
            << "? /version or /about  - Version information about this "
            << "server\n? \n"
            << "? Starting a message with 'username: ' sends a private "
            << "message\n"
            << "? root: I wish I had your power!\n"
            << std::endl;

      } else if (in == "/version" or in == "/about") {
        ios << "Local Chat Server v" << VERSION << "\n"
            << "Copyright (c) 2018 Ron R Wills <ron.rwsoft@gmail.com>\n"
            << "License GPLv3+; GNU GPL version 3 or later "
            << "<http://gnu.org/licenses/gpl.html>\n"
            << "This is free software, you are free to change and "
            << "redistribute it.\n"
            << "There is NO WARRANTY, to the extent permitted by law."
            << std::endl;

      } else {
        ios << "? Unknown chat command '" << in << "'\n"
            << "? Type '/help' to get a list of chat commands." << std::endl;
      }

    } else {
      // A message for everyone to see.
      if (not is_private(in)) {
        for (auto &it: chat_server) {
          (sockets::iostream &)(*it.second) << _name << ": " << in
                                            << std::endl;
        }
      }
    }
  }
}

/**************************
 * ChatClient::is_private *
 **************************/

bool ChatClient::is_private(const std::string &mesg) {
  size_t pos = mesg.find(':');
  bool result = false;

  // Do we have a private message?
  if (pos != mesg.npos) {

    // Send the private message to all the users connections.
    for (auto it = chat_server.begin(); it != chat_server.end(); ++it) {
      if (mesg.compare(0, pos,
            (dynamic_cast<ChatClient *>(it->second))->name()) == 0) {
        (sockets::iostream &)(*it->second) << "! "
                                           << _name << ": "
                                           << mesg.substr(pos + 2,
                                                mesg.length() - pos - 1)
                                           << std::endl;
        result = true;
      }
    }

    // If a message was sent, send in to all our connections as well.
    if (result) {
      for (auto &it: chat_server) {
        if ((dynamic_cast<ChatClient *>(it.second))->name() == _name) {
          (sockets::iostream &)(*it.second) << "! ^"
                                            << mesg.substr(0, pos) << ": "
                                            << mesg.substr(pos + 2,
                                                  mesg.length() - pos - 1)
                                            << std::endl;
        }
      }
    }
  }

  return result;
}

/**********
 * daemon *
 **********/

static void daemon() {
  pid_t pid, sid;

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE),
         "Forking server creating daemon");

  // Fork to create our new process.
  pid = fork();
  if (pid < 0) {
    throw std::runtime_error(std::string("Failed to for daemon: ") +
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

/***********************
 * change_socket_group *
 ***********************/

static void change_socket_group(const std::string &group_name) {
  struct group *group_entry = getgrnam(group_name.c_str());

  // Lookup the system group entry.
  if (group_entry == NULL) {
    throw std::runtime_error(std::string("Failed to read group ") +
                             group_name +
                             " entry: " +
                             strerror(errno));
  }

  if (chown(sock_path.c_str(), -1, group_entry->gr_gid) == -1) {
    throw std::runtime_error(std::string("Failed to change socket group: ") +
                             strerror(errno));
  }
}

/***************
 * sig_handler *
 ***************/

static void sig_handler(int signum) {

#ifdef DEBUG
  std::clog << "Signal " << signum << " received." << std::endl;
#endif

  switch (signum) {
  case SIGTERM:
  case SIGINT:
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE),
           "Interrupt signal, shutting down");
    running = false;
    break;
  }
}

/******************************************************************************
 * Entry Point
 */

int main(int argc, char *argv[]) {
  bool fork_daemon = false;

  // Get the command line options.
  int opt;
  while ((opt = getopt(argc, argv, "dg:s:w:")) != -1) {
    switch (opt) {
    case 'd':
      fork_daemon = true;
      break;
    case 'g':
      sock_group = optarg;
      break;
    case 's':
      sock_path = optarg;
      break;
    case 'w':
      cwd_path = optarg;
      break;
    default:
      std::cerr << "Unknown option -" << (char)optopt << std::endl;
      return EXIT_FAILURE;
    }
  }

  openlog("lchatd", LOG_CONS, LOG_PID);

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_INFO), "Starting");

  try {
    if (fork_daemon) daemon();
    else umask(0117);

    chat_server.open(sock_path);

    if (not sock_group.empty())
      change_socket_group(sock_group);

  } catch (std::exception &err) {
    remove(sock_path.c_str());
    syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_ERR), "%s", err.what());
    return EXIT_FAILURE;
  }

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  syslog(LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE),
         "Local chat listening on socket %s", sock_path.c_str());

  // The main loop.
  while (running) {
    chat_server.process_requests();
  }

  closelog();

  chat_server.close();

  remove(sock_path.c_str());

  return EXIT_SUCCESS;
}
