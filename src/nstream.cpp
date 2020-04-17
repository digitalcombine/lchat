/*                                                                  -*- c++ -*-
 * Copyright (c) 2018 Ron R Wills <ron.rwsoft@gmail.com>
 *
 * This file is part of the Local Chat Suite.
 *
 * Local Chat Suite is free software: you can redistribute it and/or modify
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

#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

/******************************************************************************
 * class sockets::exception
 */

/*********************************
 * sockets::exception::exception *
 *********************************/

sockets::exception::exception () noexcept {
}

sockets::exception::exception (const exception &other) noexcept
  : message(other.message) {
}

sockets::exception::exception (const char *message) noexcept
  : message(message) {
}

sockets::exception::exception (const std::string &message) noexcept
  : message(message) {
}

/**********************************
 * sockets::exception::~exception *
 **********************************/

sockets::exception::~exception() noexcept {
}

/**********************************
 * sockets::exception::operator = *
 **********************************/

sockets::exception&
sockets::exception::operator= (const exception &other) noexcept {
  if (this != &other) {
    message = other.message;
  }
  return *this;
}

/******************************************************************************
 * class sockets::ionotready
 */

/***********************************
 * sockets::ionotready::ionotready *
 ***********************************/

sockets::ionotready::ionotready () noexcept {
}

/************************************
 * sockets::ionotready::~ionotready *
 ************************************/

sockets::ionotready::~ionotready() noexcept {
}

/******************************************************************************
 * class sockets::socketbuf
 */

/*********************************
 * sockets::socketbuf::socketbuf *
 *********************************/

sockets::socketbuf::socketbuf(size_t buffer)
  : sockfd(-1), _obuf(buffer), _ibuf(buffer), _attempts(1), _delay(100000) {

  // Setup the stream buffers.
  char *end = &_ibuf.front() + _ibuf.size();
  setg(end, end, end);

  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);
}

sockets::socketbuf::socketbuf(int sockfd, size_t buffer)
  : sockfd(-1), _obuf(buffer), _ibuf(buffer), _attempts(1), _delay(100000) {
  // The socket has already been created for us.
  this->sockfd = sockfd;

  // Setup the stream buffers.
  char *end = &_ibuf.front() + _ibuf.size();
  setg(end, end, end);

  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);
}

/**********************************
 * sockets::socketbuf::~socketbuf *
 **********************************/

sockets::socketbuf::~socketbuf() noexcept {
  close();
}

/****************************
 * sockets::socketbuf::open *
 ****************************/

sockets::socketbuf *sockets::socketbuf::open(const std::string &hostname,
                                             const std::string &service) {

  struct addrinfo hints;
  struct addrinfo *info, *iter;

  /*  Resolve the hostname and get a list of the possible ways of connecting
   * to the given service.
   */
#ifdef DEBUG
  std::clog << "NET: Getting socket info for " << hostname << ":" << service
            << std::endl;
#endif
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // Stream socket
  hints.ai_flags = AI_CANONNAME;
  hints.ai_protocol = 0;           // Any protocol

  int res = getaddrinfo(hostname.c_str(), service.c_str(), &hints, &info);
  if (res != 0)
    throw sockets::exception(gai_strerror(res));

  // Iterate throught the result and attempt to connect to the server.
  for (iter = info; iter != NULL; iter = iter->ai_next) {
    if ((sockfd = ::socket(iter->ai_family, iter->ai_socktype,
                           iter->ai_protocol)) == -1) {
      continue;
    }

    if (connect(sockfd, iter->ai_addr, iter->ai_addrlen) == -1) {
      ::close(sockfd);
      continue;
    }

    break;
  }

  freeaddrinfo(info);

  // We were unable to connect to the server.
  if (iter == NULL)
    throw sockets::exception((std::string("Unable able to connect to ") +
                             hostname + ":" + service).c_str());

  return this;
}

sockets::socketbuf *sockets::socketbuf::open(const std::string &filename) {
  struct sockaddr_un addr;

  // Create the unix socket.
  sockfd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    throw sockets::exception(
      std::string("Unable to create unix domain socket ") +
      filename + ": " + strerror(errno));
  }

  // Connect to the socket.
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, filename.c_str(), sizeof(addr.sun_path) - 1);
  if (connect(sockfd, (struct sockaddr *)&addr,
              sizeof(struct sockaddr_un)) < 0) {
    throw sockets::exception(
      std::string("Unable to connect to unix domain socket ") +
      filename + ": " + strerror(errno));
  }

  return this;
}

/*****************************
 * sockets::socketbuf::close *
 *****************************/

void sockets::socketbuf::close() {
  if (sockfd >= 0) {
    sync();
    ::close(sockfd);
  }
  sockfd = -1;

  // Reset the stream buffers.
  char *end = &_ibuf.front() + _ibuf.size();
  setg(end, end, end);

  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);
}

/********************************
 * sockets::socketbuf::overflow *
 ********************************/

sockets::socketbuf::int_type sockets::socketbuf::overflow(int_type ch) {
  if (ch != traits_type::eof()) {
    *pptr() = ch;
    pbump(1);
  }

  if (pptr() > epptr())
    if (not oflush()) return traits_type::eof();

  return ch;
}

/****************************
 * sockets::socketbuf::sync *
 ****************************/

int sockets::socketbuf::sync() {
  return (oflush() ? 0 : -1);
}

/********************************
 * sockets::socketbuf::underflow *
 ********************************/

sockets::socketbuf::int_type sockets::socketbuf::underflow() {
  if (gptr() >= egptr()) {
    // The buffer has been exhausted, read more in from the socket.

    int res = 0;
    for (unsigned int attempts = 0; res <= 0; ++attempts) {
      res = read(sockfd, &_ibuf.front(), _ibuf.size());

      if (res == 0) {
        // End of file, usually it was disconnected.
        return traits_type::eof();

      } else if (res < 0) {
        if (errno == EAGAIN or errno == EWOULDBLOCK) {
          /*  If the socket is non-blocking then we can try again to read
           * the socket or throw an ionotready exception.
           */
          if (attempts == (_attempts - 1)) throw sockets::ionotready();
          usleep(_delay);
        } else {
          // We got an io error.
          throw sockets::exception((std::string("Socket read error: ") +
                                    strerror(errno)));
        }
      }
    }

    // Update the buffer.
    setg(&_ibuf.front(), &_ibuf.front(), &_ibuf.front() + res);
  }

  return traits_type::to_int_type(*gptr());
}

/******************************
 * sockets::socketbuf::oflush *
 ******************************/

bool sockets::socketbuf::oflush() {
  int wlen = pptr() - pbase();
  char *buf = pbase();

  // Write the entire buffer to the socket.
  do {
    int wrote = write(sockfd, buf, wlen);
    if (wrote == -1) return false;
    buf += wrote;
    wlen -= wrote;
  } while (wlen);

  // Reset the buffers.
  char *base = &_obuf.front();
  setp(base, base + _obuf.size() - 1);

  return true;
}

/******************************************************************************
 * class sockets::iostream
 */

/*******************************
 * sockets::iostream::iostream *
 *******************************/

sockets::iostream::iostream(size_t buffer)
  : std::iostream(&sockbuf), sockbuf(buffer) {
}

sockets::iostream::iostream(const std::string &hostname,
                            const std::string &service,
                            size_t buffer)
  : std::iostream(&sockbuf), sockbuf(buffer) {
  sockbuf.open(hostname, service);
}

sockets::iostream::iostream(const std::string &filename, size_t buffer)
  : std::iostream(&sockbuf), sockbuf(buffer) {
  sockbuf.open(filename);
}

sockets::iostream::iostream(int sockfd, size_t buffer)
  : std::iostream(&sockbuf), sockbuf(sockfd, buffer) {
}

/****************************
 * sockets::iostream::close *
 ****************************/

void sockets::iostream::close() {
  sockbuf.close();
  setstate(eofbit);
}

/******************************************************************************
 * Stream modifiers
 */

/*********************
 * sockets::nonblock *
 *********************/

std::istream &sockets::nonblock(std::istream &ios) {
  iostream *iosptr = dynamic_cast<iostream *>(&ios);
  if (iosptr != NULL and iosptr->is_open()) {
    int flags = fcntl(iosptr->sockbuf.socket(), F_GETFL, 0);
    if (fcntl(iosptr->sockbuf.socket(), F_SETFL, flags | O_NONBLOCK) < 0)
      throw sockets::exception(std::string("Setting nonblocking failed: ") +
                               strerror(errno));

    // Allow exceptions to be passed through the stream.
    ios.exceptions(iosptr->exceptions() | std::iostream::badbit);
  }
  return ios;
}

/******************
 * sockets::block *
 ******************/

std::istream &sockets::block(std::istream &ios) {
  iostream *iosptr = dynamic_cast<iostream *>(&ios);
  if (iosptr != NULL and iosptr->is_open()) {
    int flags = fcntl(iosptr->sockbuf.socket(), F_GETFL, 0);
    if (fcntl(iosptr->sockbuf.socket(), F_SETFL, flags & ~O_NONBLOCK) < 0)
      throw sockets::exception((std::string("Setting blocking failed: ") +
                                strerror(errno)).c_str());

    // The stream should catch and deal with exceptions.
    iosptr->exceptions(iosptr->exceptions() & ~std::iostream::badbit);
  }
  return ios;
}

/**********************
 * sockets::keepalive *
 **********************/

std::ostream &sockets::keepalive(std::ostream &ios) {
  iostream *iosptr = dynamic_cast<iostream *>(&ios);
  if (iosptr != NULL) {
    int optval = 1;
    if(setsockopt(iosptr->sockbuf.socket(), SOL_SOCKET, SO_KEEPALIVE,
                  &optval, sizeof(optval)) < 0) {
      throw sockets::exception(std::string("Setting keep alive failed: ") +
                               strerror(errno));
    }
  }
  return ios;
}

/************************
 * sockets::nokeepalive *
 ************************/

std::ostream &sockets::nokeepalive(std::ostream &ios) {
  iostream *iosptr = dynamic_cast<iostream *>(&ios);
  if (iosptr != NULL) {
    int optval = 0;
    if(setsockopt(iosptr->sockbuf.socket(), SOL_SOCKET, SO_KEEPALIVE,
                  &optval, sizeof(optval)) < 0) {
      throw sockets::exception(std::string("Clearing keep alive failed: ") +
                               strerror(errno));
    }
  }
  return ios;
}

std::ostream &sockets::recvtimeout::operator()(std::ostream &ios) const {
  iostream *iosptr = dynamic_cast<iostream *>(&ios);
  if (iosptr != NULL) {
    if(setsockopt(iosptr->sockbuf.socket(), SOL_SOCKET, SO_RCVTIMEO,
                  &arg, sizeof(arg)) < 0) {
      throw sockets::exception(
        std::string("Setting receive timeout failed: ") +
        strerror(errno));
    }
  }
  return ios;
}

sockets::iostream &
sockets::recvattempts::operator()(sockets::iostream &ios) const {
  ios.sockbuf.attempts(arg);
  return ios;
}

/******************************************************************************
 * class sockets::connection
 */

sockets::connection::connection(int sockfd) : ios(sockfd) {
}

sockets::connection::~connection() noexcept {
}

void sockets::connection::connect(int sockfd) {
  (void)sockfd;
}

void sockets::connection::close() {
  ios.close();
}

/******************************************************************************
 * class sockets::server_base
 */

/*************************************
 * sockets::server_base::server_base *
 *************************************/

sockets::server_base::server_base() : sockfd(-1) {
  FD_ZERO(&active_fd_set);

  timeout.tv_sec = 10;
  timeout.tv_usec = 0;
}

/**************************************
 * sockets::server_base::~server_base *
 **************************************/

sockets::server_base::~server_base() noexcept {
  FD_ZERO(&active_fd_set);
  ::close(sockfd);
}

/******************************
 * sockets::server_base::open *
 ******************************/

void sockets::server_base::open(const char *hostname, const char *service) {
  struct addrinfo hints;
  struct addrinfo *info, *iter;

#ifdef DEBUG
  std::clog << "NET: Getting socket info for " << hostname << ":"
            << service << std::endl;
#endif
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
  hints.ai_flags = AI_CANONNAME|AI_PASSIVE;
  hints.ai_protocol = 0;           /* Any protocol */

  int res = getaddrinfo(hostname, service, &hints, &info);
  if (res != 0)
    throw sockets::exception(gai_strerror(res));

  for (iter = info; iter != NULL; iter = iter->ai_next) {
    if ((sockfd = socket(iter->ai_family, iter->ai_socktype,
                         iter->ai_protocol))
        == -1) {
      continue;
    }

    if (bind(sockfd, iter->ai_addr, iter->ai_addrlen) == -1) {
      ::close(sockfd);
      continue;
    }

    break;
  }

  freeaddrinfo(info);

  if (iter == NULL)
    throw sockets::exception((std::string("Unable able to bind to ") +
                          hostname + ":" + service).c_str());

  if (listen(sockfd, 1) == -1)
    throw sockets::exception((std::string("Unable able to listen to ") +
                          hostname + ":" + service).c_str());

  std::clog << "Server listening on " << hostname << ":"
            << service << std::endl;

  // Initialize the set of active sockets.
  FD_ZERO(&active_fd_set);
  FD_SET(sockfd, &active_fd_set);
}

void sockets::server_base::open(const std::string &filename) {
  struct sockaddr_un addr;

  sockfd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    throw sockets::exception(
      std::string("Unable to create unix domain socket ") +
      filename + ": " + strerror(errno));
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, (char *)filename.c_str(), sizeof(addr.sun_path) - 1);
  if (bind(sockfd, (const sockaddr *)&addr, sizeof(addr)) == -1) {
    ::close(sockfd);
    sockfd = -1;
    throw sockets::exception(std::string("Unable able to bind to ") +
                             filename);
  }

  if (listen(sockfd, 1) == -1)
    throw sockets::exception(std::string("Unable able to listen to ") +
                             filename);

  std::clog << "Server listening on " << filename << std::endl;

  // Initialize the set of active sockets.
  FD_ZERO(&active_fd_set);
  FD_SET(sockfd, &active_fd_set);
}

/*******************************
 * sockets::server_base::close *
 *******************************/

void sockets::server_base::close() {
  FD_ZERO(&active_fd_set);
  ::close(sockfd);
  sockfd = -1;
}

/******************************************
 * sockets::server_base::process_requests *
 ******************************************/

void sockets::server_base::process_requests() {
  fd_set read_fd_set = active_fd_set;
  if (::select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
    if (errno == EINTR)
      return;
    else
      throw sockets::exception(std::string("select: ") +
                               strerror(errno));
  }

  /* Service all the sockets with input pending. */
  for (int i = 0; i < FD_SETSIZE; ++i)
    if (FD_ISSET (i, &read_fd_set)) {

      if (i == sockfd) {
        // Connection request on original socket.
        int newfd;
        struct sockaddr_in clientname;

        // Attempt to accept the connection.
        socklen_t size = sizeof(clientname);
        newfd = accept(sockfd, (struct sockaddr *)&clientname, &size);
        if (newfd < 0) {
          std::clog << "Unable to accept connection: "
                    << strerror(errno) << std::endl;
          return;
        }

        // Add it to the clients lists.
        FD_SET(newfd, &active_fd_set);
        _clients[newfd] = new_connection(newfd);
        try {
          _clients[newfd]->connect(newfd);
        } catch (std::exception &err) {
          std::clog << "Exception: " << err.what() << std::endl;
        }

      } else {

        /* Data arriving on an already-connected socket. */
        _clients[i]->recv();
        if (not _clients[i]->ios or _clients[i]->ios.eof()) {
          std::clog << "Connection closed" << std::endl;

          auto tmp = _clients[i];
          _clients.erase(i);  // Remove the client from our list.
          delete tmp; // Destroy the client.
          FD_CLR(i, &active_fd_set);
        }
      }
    }
}

/*************************************
 * sockets::server_base::operator () *
 *************************************/

void sockets::server_base::operator()() {
  while (true) process_requests();
}
