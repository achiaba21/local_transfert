#include "ltr/network/broadcast_socket.hpp"

#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <sys/socket.h>
#endif

namespace ltr::network {

bool BroadcastUdpSocket::enableBroadcast() {
    const int enable = 1;
#ifdef _WIN32
    const int r = ::setsockopt(
        getHandle(), SOL_SOCKET, SO_BROADCAST,
        reinterpret_cast<const char*>(&enable),
        static_cast<int>(sizeof(enable)));
#else
    const int r = ::setsockopt(
        getHandle(), SOL_SOCKET, SO_BROADCAST,
        &enable, sizeof(enable));
#endif
    return r == 0;
}

} // namespace ltr::network
