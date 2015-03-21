/**\file
 * \brief asio.hpp IRC Server
 *
 * \copyright
 * Copyright (c) 2015, ef.gy Project Members
 * \copyright
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * \copyright
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * \copyright
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \see Project Documentation: http://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 */

#if !defined(EF_GY_HTTP_H)
#define EF_GY_HTTP_H

#include <set>
#include <map>
#include <iomanip>

#include <regex>
#include <system_error>
#include <algorithm>
#include <functional>

#include <ef.gy/server.h>

namespace efgy {
namespace net {
namespace irc {

enum numericMessage {
  RPL_WELCOME = 1
};

template <typename base, typename requestProcessor> class session;

namespace processor {
template <class sock> class base {
public:
  typedef session<sock, base<sock> > session;

  bool nick(session &session, const std::string &nick) {
    session.nick = nick;

    switch (session.status) {
    case session::expect_nick_user:
      session.status = session::expect_user;
      break;
    case session::expect_nick:
      session.status = session::nominal;
      return hello(session);
    default:
      break;
    }

    return true;
  }

  bool user(session &session, const std::string &user, const std::string &mode,
            const std::string &real) {
    session.user = user;

    switch (session.status) {
    case session::expect_nick_user:
      session.status = session::expect_nick;
      break;
    case session::expect_user:
      session.status = session::nominal;
      return hello(session);
    default:
      break;
    }

    return true;
  }

  bool nick(session &session, const std::vector<std::string> &param) {
    return nick(session, param[0]);
  }

  bool user(session &session, const std::vector<std::string> &param) {
    return user(session, param[0], param[1], param[3]);
  }

  bool ping(session &session, const std::vector<std::string> &param) {
    session.reply("PONG", param);

    return true;
  }

  bool other(session &session, const std::string &command,
             const std::vector<std::string> &param) {
    session.server.log << "[" << session.prefix()
                       << "] unknown command:" << command << "\n";

    return true;
  }

  bool hello(session &session) {
    session.reply(RPL_WELCOME);

    return motd(session);
  }

  bool motd(session &session) { return true; }

  bool remember(session &session) {
    sessions.insert(&session);

    return true;
  }

  bool forget(session &session) {
    sessions.erase(&session);

    return true;
  }

protected:
  std::set<session *> sessions;
};
}

template <typename base, typename requestProcessor = processor::base<base> >
using server = net::server<base, requestProcessor, session>;

template <typename base, typename requestProcessor> class session {
protected:
  typedef server<base, requestProcessor> serverType;

public:
  enum {
    expect_nick_user,
    expect_nick,
    expect_user,
    nominal,
    expect_pong,
    shutdown
  } status;

  typename base::socket socket;

  std::string user;
  std::string nick;

  std::string prefix(void) { return nick + "!" + user + "@host"; }

  std::set<std::string> subscriptions;

  std::string content;

  serverType &server;

  session(serverType &pServer)
      : server(pServer), socket(pServer.io), status(expect_nick_user), input() {
  }

  ~session(void) {
    status = shutdown;

    server.processor.forget(*this);

    try {
      socket.shutdown(base::socket::shutdown_both);
    }
    catch (std::system_error & e) {
      std::cerr << "exception while shutting down socket: " << e.what() << "\n";
    }

    try {
      socket.close();
    }
    catch (std::system_error & e) {
      std::cerr << "exception while closing socket: " << e.what() << "\n";
    }
  }

  void start() { read(); }

  bool reply(const std::string &command,
             const std::vector<std::string> &params) {
    bool have_trailing = false;

    std::stringstream reply;

    reply << ":" << server.name << " " << command;

    for (const std::string &param : params) {
      if (param != "") {
        if (have_trailing) {
          std::cerr
              << "WARNING: IRC: Ignored parameter after trailing parameter:"
              << param << "\n";
        }

        if (param.find(' ') != std::string::npos) {
          reply << " :" << param;
          have_trailing = true;
          continue;
        }

        reply << " " << param;
      } else {
        std::cerr << "WARNING: IRC: empty parameter in reply.\n";
      }
    }

    reply << "\r\n";

    std::cerr << "sent: " << reply.str();

    asio::async_write(socket, asio::buffer(reply.str()),
                      [&](std::error_code ec, std::size_t length) {
      handleWrite(ec);
    });

    return true;
  }

  bool reply(const enum numericMessage num, std::vector<std::string> params = {
  }) {
    std::stringstream code;

    if (params.size() == 0) {
      switch (num) {
      case RPL_WELCOME:
        params.push_back("Welcome to the Internet Relay Network " + prefix());
        break;
      default:
        break;
      }
    }

    code << std::setfill('0') << std::setw(3) << long(num);

    params.insert(params.begin(), nick);

    return reply(code.str(), params);
  }

protected:
  void handleRead(const std::error_code &error, size_t bytes_transferred) {
    if (status == shutdown) {
      return;
    }

    if (!error) {
      static const std::regex line(
          "(:([^ ]+) )?([A-Z]+)( ([^: \r][^ \r]*))?( ([^: \r][^ \r]*))?( ([^: "
          "\r][^ \r]*))?( ([^: \r][^ \r]*))?( ([^: \r][^ \r]*))?( ([^: \r][^ "
          "\r]*))?( ([^: \r][^ \r]*))?( ([^: \r][^ \r]*))?( ([^: \r][^ "
          "\r]*))?( ([^: \r][^ \r]*))?( ([^: \r][^ \r]*))?( ([^: \r][^ "
          "\r]*))?( ([^: \r][^ \r]*))?( ([^: \r][^ \r]*))?( :([^\r]+))? *\r?");
      std::istream is(&input);
      std::string s;
      std::smatch matches;

      std::getline(is, s);

      if (std::regex_match(s, matches, line)) {
        std::vector<std::string> param;
        for (unsigned int i : {
          5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33
        }) {
          if (matches[i] != "") {
            param.push_back(matches[i]);
          }
        }

        if (matches[3] == "NICK") {
          server.processor.nick(*this, param);
        } else if (matches[3] == "USER") {
          server.processor.user(*this, param);
        } else if (matches[3] == "PING") {
          server.processor.ping(*this, param);
        } else {
          server.processor.other(*this, matches[3], param);
        }
      } else {
        server.log << "[" << prefix() << "] malformed message:" << s << "\n";
      }

      read();
    } else {
      delete this;
    }
  }

  void handleWrite(const std::error_code &error) {
    if (status == shutdown) {
      return;
    }

    if (!error) {
      //read();
    } else {
      delete this;
    }
  }

  void read(void) {
    switch (status) {
    case shutdown:
      break;
    default:
      asio::async_read_until(
          socket, input, "\n",
          [&](const asio::error_code & error, std::size_t bytes_transferred) {
        handleRead(error, bytes_transferred);
      });
      break;
    }
  }

  asio::streambuf input;
};
}
}
}

#endif