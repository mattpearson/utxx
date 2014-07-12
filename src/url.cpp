//----------------------------------------------------------------------------
/// \file  url.cpp
//----------------------------------------------------------------------------
/// \brief Implementation of url parsing functions.
//----------------------------------------------------------------------------
// Copyright (c) 2011 Serge Aleynikov <saleyn@gmail.com>
// Created: 2011-09-10
//----------------------------------------------------------------------------
/*
***** BEGIN LICENSE BLOCK *****

This file is part of the utxx open-source project.

Copyright (C) 2011 Serge Aleynikov <saleyn@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

***** END LICENSE BLOCK *****
*/

#include <utxx/url.hpp>
#include <boost/optional.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/assign.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace utxx {

namespace detail {

    const char* connection_type_to_str(connection_type a_type)
    {
        switch (a_type) {
            case TCP:       return "tcp";
            case UDP:       return "udp";
            case UDS:       return "uds";
            case FILENAME:  return "file";
            default:        return "undefined";
        }
    }

} // namespace detail

std::string addr_info::to_string() const
{
    std::stringstream s;
    s << m_proto << "://";
    if (!addr.empty()) s << addr;
    if (!port.empty()) s << ':' << port;
    if (!path.empty()) s << path;
    return s.str();
}

bool is_ipv4_addr(const std::string& a_addr)
{
    using namespace boost::xpressive;
    static const boost::xpressive::mark_tag delim(1), d1(2), d2(3), d3(4), d4(5);
    static const boost::xpressive::sregex s_re_ip =
            (d1 = repeat<1,3>(_d))
        >>  (delim = (boost::xpressive::set='.'))
        >>  (d2 = repeat<1,3>(_d)) >> delim
        >>  (d3 = repeat<1,3>(_d)) >> delim
        >>  (d4 = repeat<1,3>(_d));

    boost::xpressive::smatch l_what;

    bool res = boost::xpressive::regex_search(a_addr, l_what, s_re_ip);
    return res 
        && boost::lexical_cast<int>(l_what[d1]) < 256
        && boost::lexical_cast<int>(l_what[d2]) < 256
        && boost::lexical_cast<int>(l_what[d3]) < 256
        && boost::lexical_cast<int>(l_what[d4]) < 256;
}

//----------------------------------------------------------------------------
// URL Parsing
//----------------------------------------------------------------------------
bool addr_info::parse(const std::string& a_url) {
    static std::map<std::string, std::string> proto_to_port =
            boost::assign::map_list_of
                ("http", "80") ("https", "443") ("file", "") ("uds", "");

    using namespace boost::xpressive;
    // "(http|perc)://(.*?)(:(\\d+))?(/.*)" | "(file|uds)://(.*)"
    static const boost::xpressive::sregex l_re_url =
          (    (s1 = icase("http") | icase("https") | icase("udp") | icase("tcp"))
            >> "://"
            >> optional(s2 = +set[alpha | '-' | '.' | '_' | digit])
            >> optional(':' >> (s3 = +_d))
            >> optional(s4 = '/' >> *_) )
        | (    (s1 = icase("file") | icase("uds"))
            >> "://"
            >> optional(s4 = +_) );

    boost::xpressive::smatch l_what;

    bool res = boost::xpressive::regex_search(a_url, l_what, l_re_url);
    if (res) {
        m_proto = l_what[1];
        boost::to_lower(m_proto);
        addr  = l_what[2];
        port  = l_what[3].matched ? l_what[3] : proto_to_port[m_proto];
        path  = l_what[4];
    }

    if      (m_proto == "tcp"  ||
             m_proto == "http" ||
             m_proto == "https")                    proto = TCP;
    else if (m_proto == "udp")                      proto = UDP;
    else if (m_proto == "uds")                      proto = UDS;
    else if (m_proto == "file")                     proto = FILENAME;
    else                                            proto = UNDEFINED;

    m_is_ipv4 = is_ipv4_addr(addr);
    return res;
}



} // namespace utxx
