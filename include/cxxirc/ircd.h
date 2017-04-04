/**\file
 *
 * \copyright
 * This file is part of the cxxirc project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/cxxirc
 * \see Project Source Code: https://github.com/ef-gy/cxxirc
 * \see Licence Terms: https://github.com/ef-gy/cxxirc/blob/master/COPYING
 */

#if !defined(CXXIRC_IRCD_H)
#define CXXIRC_IRCD_H

#include <cxxirc/irc.h>

namespace cxxirc {
namespace ircd {

template <class sock>
static std::size_t setup(cxxhttp::net::endpoint<sock> lookup,
                         cxxhttp::io::service &service = cxxhttp::io::service::common()) {
  return lookup.with([&service, &lookup](
                         typename sock::endpoint &endpoint) -> bool {
    net::irc::server<sock> *s = new net::irc::server<sock>(endpoint, service);

    s->name = lookup.name();

    return true;
  });
}

static efgy::cli::option socket("-{0,2}irc:unix:(.+)", [](std::smatch &m) -> bool {
  return setup(cxxhttp::net::endpoint<asio::local::stream_protocol>(m[1])) > 0;
}, "Listen for IRC connections on the given unix socket[1].");

static efgy::cli::option tcp("-{0,2}irc:(.+):([0-9]+)", [](std::smatch &m) -> bool {
  return setup(cxxhttp::net::endpoint<asio::ip::tcp>(m[1], m[2])) > 0;
}, "Listen for IRC connections on the given host[1] and port[2].");
}
}

#endif
