/*
 * Copyright (c) 2017, Uber Technologies, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef UBER_JAEGER_UTILS_NET_H
#define UBER_JAEGER_UTILS_NET_H

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace uber {
namespace jaeger {
namespace utils {
namespace net {

class IPAddress {
  public:
    static IPAddress v4(const std::string& ip, int port)
    {
        ::sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        const auto returnCode =
            inet_pton(addr.sin_family, ip.c_str(), &addr.sin_addr);
        if (returnCode < 0) {
            if (returnCode == 0) {
                std::ostringstream oss;
                oss << "Invalid IP address"
                       ", ip="
                    << ip << ", port=" << port;
                throw std::invalid_argument(oss.str());
            }
            std::ostringstream oss;
            oss << "Failed to parse IP address (inet_pton)"
                   ", ip="
                << ip << ", port=" << port;
            throw std::system_error(errno, std::generic_category(), oss.str());
        }
        return IPAddress(addr);
    }

    IPAddress()
        : _addr()
        , _addrLen(sizeof(::sockaddr_in))
    {
        std::memset(&_addr, 0, sizeof(_addr));
    }

    IPAddress(const ::sockaddr_storage& addr, ::socklen_t addrLen)
        : _addr(addr)
        , _addrLen(addrLen)
    {
    }

    IPAddress(const ::sockaddr& addr, ::socklen_t addrLen)
        : IPAddress(reinterpret_cast<const ::sockaddr_storage&>(addr), addrLen)
    {
    }

    explicit IPAddress(const ::sockaddr_in& addr)
        : IPAddress(reinterpret_cast<const ::sockaddr&>(addr), sizeof(addr))
    {
    }

    explicit IPAddress(const ::sockaddr_in6& addr)
        : IPAddress(reinterpret_cast<const ::sockaddr&>(addr), sizeof(addr))
    {
    }

    const ::sockaddr_storage& addr() const { return _addr; }

    ::socklen_t addrLen() const { return _addrLen; }

    void print(std::ostream& out) const
    {
        out << "{ family=" << family();
        const auto addrStr = host();
        if (!addrStr.empty()) {
            out << ", addr=" << addrStr;
        }
        out << ", port=" << port() << " }";
    }

    std::string authority() const
    {
        const auto portNum = port();
        if (portNum != 0) {
            return host() + ':' + std::to_string(portNum);
        }
        return host();
    }

    std::string host() const
    {
        std::array<char, INET6_ADDRSTRLEN> buffer;
        const auto af = family();
        const auto* addrStr = ::inet_ntop(
            af,
            af == AF_INET
                ? static_cast<const void*>(
                      &reinterpret_cast<const ::sockaddr_in&>(_addr).sin_addr)
                : static_cast<const void*>(
                      &reinterpret_cast<const ::sockaddr_in6&>(_addr)
                           .sin6_addr),
            &buffer[0],
            buffer.size());
        return addrStr ? addrStr : "";
    }

    int port() const
    {
        if (family() == AF_INET) {
            return ntohs(
                reinterpret_cast<const ::sockaddr_in&>(_addr).sin_port);
        }
        return ntohs(
            reinterpret_cast<const ::sockaddr_in6&>(_addr).sin6_port);
    }

    int family() const
    {
        if (_addrLen == sizeof(::sockaddr_in)) {
            return AF_INET;
        }
        assert(_addrLen == sizeof(::sockaddr_in6));
        return AF_INET6;
    }

  private:
    ::sockaddr_storage _addr;
    ::socklen_t _addrLen;
};

struct URI {
    static URI parse(const std::string& uriStr);

    URI()
        : _scheme()
        , _host()
        , _port(0)
        , _path()
        , _query()
    {
    }

    std::string authority() const
    {
        if (_port != 0) {
            return _host + ':' + std::to_string(_port);
        }
        return _host;
    }

    std::string target() const
    {
        if (!_query.empty()) {
            return _path + '?' + _query;
        }
        return _path;
    }

    void print(std::ostream& out) const
    {
        out << "{ scheme=\"" << _scheme << '"'
            << ", host=\"" << _host << '"'
            << ", port=" << _port
            << ", path=\"" << _path << '"'
            << ", query=\"" << _query << '"'
            << " }";
    }

    std::string _scheme;
    std::string _host;
    int _port;
    std::string _path;
    std::string _query;
};

struct AddrInfoDeleter : public std::function<void(::addrinfo*)> {
    void operator()(::addrinfo* addrInfo) const { ::freeaddrinfo(addrInfo); }
};

std::unique_ptr<::addrinfo, AddrInfoDeleter>
resolveAddress(const URI& uri, int socketType);

inline std::unique_ptr<::addrinfo, AddrInfoDeleter>
resolveAddress(const std::string& uriStr, int socketType)
{
    return resolveAddress(URI::parse(uriStr), socketType);
}

class Socket {
  public:
    Socket()
        : _handle(-1)
        , _family(-1)
        , _type(-1)
    {
    }

    Socket(const Socket&) = delete;

    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& socket)
        : _handle(socket._handle)
        , _family(socket._family)
        , _type(socket._type)
    {
    }

    Socket& operator=(Socket&& rhs)
    {
        _handle = rhs._handle;
        _family = rhs._family;
        _type = rhs._type;
        return *this;
    }

    ~Socket() { close(); }

    void open(int family, int type)
    {
        const auto handle = ::socket(family, type, 0);
        if (handle < 0) {
            std::ostringstream oss;
            oss << "Failed to open socket"
                   ", family="
                << family << ", type=" << type;
            throw std::system_error(errno, std::generic_category(), oss.str());
        }
        _handle = handle;
        _family = family;
        _type = type;
    }

    void bind(const IPAddress& addr)
    {
        const auto returnCode =
            ::bind(_handle,
                   reinterpret_cast<const ::sockaddr*>(&addr.addr()),
                   addr.addrLen());
        if (returnCode != 0) {
            std::ostringstream oss;
            oss << "Failed to bind socket to address"
                   ", addr=";
            addr.print(oss);
            throw std::system_error(errno, std::generic_category(), oss.str());
        }
    }

    void bind(const std::string& ip, int port)
    {
        const auto addr = IPAddress::v4(ip, port);
        bind(addr);
    }

    void connect(const IPAddress& serverAddr)
    {
        const auto returnCode =
            ::connect(_handle,
                      reinterpret_cast<const ::sockaddr*>(&serverAddr.addr()),
                      serverAddr.addrLen());
        if (returnCode != 0) {
            std::ostringstream oss;
            oss << "Cannot connect socket to remote address ";
            serverAddr.print(oss);
            throw std::runtime_error(oss.str());
        }
    }

    IPAddress connect(const std::string& serverURIStr)
    {
        return connect(URI::parse(serverURIStr));
    }

    IPAddress connect(const URI& serverURI)
    {
        auto result = resolveAddress(serverURI, _type);
        for (const auto* itr = result.get(); itr; itr = itr->ai_next) {
            const auto returnCode =
                ::connect(_handle, itr->ai_addr, itr->ai_addrlen);
            if (returnCode == 0) {
                return IPAddress(*itr->ai_addr, itr->ai_addrlen);
            }
        }
        std::ostringstream oss;
        oss << "Cannot connect socket to remote address ";
        serverURI.print(oss);
        throw std::runtime_error(oss.str());
    }

    static constexpr auto kDefaultBacklog = 128;

    void listen(int backlog = kDefaultBacklog)
    {
        const auto returnCode = ::listen(_handle, backlog);
        if (returnCode != 0) {
            throw std::system_error(
                errno, std::generic_category(), "Failed to listen on socket");
        }
    }

    Socket accept()
    {
        ::sockaddr_storage addrStorage;
        ::socklen_t addrLen = sizeof(addrStorage);
        const auto clientHandle = ::accept(
            _handle, reinterpret_cast<::sockaddr*>(&addrStorage), &addrLen);
        if (clientHandle < 0) {
            throw std::system_error(
                errno, std::generic_category(), "Failed to accept on socket");
        }

        Socket clientSocket;
        clientSocket._handle = clientHandle;
        clientSocket._family =
            (addrLen == sizeof(::sockaddr_in)) ? AF_INET : AF_INET6;
        clientSocket._type = SOCK_STREAM;
        return clientSocket;
    }

    void close()
    {
        if (_handle >= 0) {
            ::close(_handle);
            _handle = -1;
        }
    }

    int handle() { return _handle; }

  private:
    int _handle;
    int _family;
    int _type;
};

static constexpr auto kUDPPacketMaxLength = 65000;

namespace http {

class ParseError : public std::invalid_argument {
    using invalid_argument::invalid_argument;
};

class Header {
  public:
    Header() = default;

    Header(const std::string& key,
           const std::string& value)
        : _key(key)
        , _value(value)
    {
    }

  private:
    std::string _key;
    std::string _value;
};

enum class Method {
    OPTIONS,
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    TRACE,
    CONNECT,
    EXTENSION
};

Method parseMethod(const std::string& methodName);

class Request {
  public:
    static Request parse(std::istream& in);

    Request()
        : _method()
        , _target()
        , _version()
        , _headers()
    {
    }

    Method method() const { return _method; }

    const std::string& target() const { return _target; }

    const std::string& version() const { return _version; }

    const std::vector<Header>& headers() const { return _headers; }

  private:
    Method _method;
    std::string _target;
    std::string _version;
    std::vector<Header> _headers;
};

class Response {
  public:
    static Response parse(std::istream& in);

    Response()
        : _version()
        , _statusCode(0)
        , _reason()
        , _headers()
        , _body()
    {
    }

    int statusCode() const { return _statusCode; }

    const std::string& reason() const { return _reason; }

    const std::vector<Header>& headers() const { return _headers; }

    const std::string& body() const { return _body; }

  private:
    std::string _version;
    int _statusCode;
    std::string _reason;
    std::vector<Header> _headers;
    std::string _body;
};

Response get(const URI& uri);

}  // namespace http
}  // namespace net
}  // namespace utils
}  // namespace jaeger
}  // namespace uber

inline std::ostream& operator<<(std::ostream& out,
                                const uber::jaeger::utils::net::IPAddress& addr)
{
    addr.print(out);
    return out;
}

inline std::ostream& operator<<(std::ostream& out,
                                const uber::jaeger::utils::net::URI& uri)
{
    uri.print(out);
    return out;
}

#endif  // UBER_JAEGER_UTILS_NET_H
