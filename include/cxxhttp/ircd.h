/**\file
 *
 * \copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/cxxhttp
 * \see Project Source Code: https://github.com/ef-gy/cxxhttp
 * \see Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 */

#if !defined(CXXHTTP_IRCD_H)
#define CXXHTTP_IRCD_H

#include <cxxhttp/irc.h>

namespace cxxhttp {
namespace ircd {

template <class sock>
static std::size_t setup(net::endpoint<sock> lookup,
                         io::service &service = io::service::common()) {
  return lookup.with([&service, &lookup](
                         typename sock::endpoint &endpoint) -> bool {
    net::irc::server<sock> *s = new net::irc::server<sock>(endpoint, service);

    s->name = lookup.name();

    return true;
  });
}

static efgy::cli::option socket("-{0,2}irc:unix:(.+)", [](std::smatch &m) -> bool {
  return setup(net::endpoint<asio::local::stream_protocol>(m[1])) > 0;
}, "Listen for IRC connections on the given unix socket[1].");

static efgy::cli::option tcp("-{0,2}irc:(.+):([0-9]+)", [](std::smatch &m) -> bool {
  return setup(net::endpoint<asio::ip::tcp>(m[1], m[2])) > 0;
}, "Listen for IRC connections on the given host[1] and port[2].");
}
}

#endif
