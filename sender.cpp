#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <endian.h>

void xorp(uint8_t* d, const uint8_t* a, const uint8_t* b) {
 for(size_t i=0; i<160; ++i) d[i] = a[i] ^ b[i];
}

int main() {
  int in = socket(AF_INET, SOCK_DGRAM, 0);
 int y = 1;
 setsockopt(in, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y));
sockaddr_in bnd{};
  bnd.sin_family = AF_INET;
 bnd.sin_addr.s_addr = INADDR_ANY;
bnd.sin_port = htons(47010);
 bind(in, (sockaddr*)&bnd, sizeof(bnd));

 int out = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in dst{};
dst.sin_family = AF_INET;
  dst.sin_addr.s_addr = inet_addr("127.0.0.1");
 dst.sin_port = htons(47001);

 uint8_t mem[3][160];
  char bf[164];
 sockaddr_in saddr{};
 socklen_t slen = sizeof(saddr);

  while(1) {
  if(recvfrom(in, bf, sizeof(bf), 0, (sockaddr*)&saddr, &slen) == 164) {
   uint32_t sq;
  std::memcpy(&sq, bf, 4);
    sq = ntohl(sq);
    int off = sq % 3;
    
  std::memcpy(mem[off], bf + 4, 160);

   uint8_t pkt[165];
  uint32_t net = htonl(sq);
   std::memcpy(pkt, &net, 4);
  pkt[4] = 0; 
    std::memcpy(pkt + 5, bf + 4, 160);
   sendto(out, pkt, 165, 0, (sockaddr*)&dst, sizeof(dst));

   uint32_t bnet = htonl(sq - off);
   if(off == 1 || off == 2) {
    std::memcpy(pkt, &bnet, 4);
   pkt[4] = off; 
   xorp(pkt + 5, mem[off - 1], mem[off]);
   sendto(out, pkt, 165, 0, (sockaddr*)&dst, sizeof(dst));
    }
  }
 }
return 0;
}