#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

void xorp(uint8_t* d, const uint8_t* a, const uint8_t* b) {
 for(size_t i=0; i<160; ++i) d[i] = a[i] ^ b[i];
}

struct grp {
 bool d[3] = {0,0,0};
 bool pa = 0;
  bool pb = 0;
 uint8_t dp[3][160];
 uint8_t pap[160];
uint8_t pbp[160];
};

struct slot { bool r=0; uint8_t p[160]; };

int main() {
 char* tstr = std::getenv("T0");
  char* dstr = std::getenv("DELAY_MS");
 if(!tstr || !dstr) return 1;

 uint64_t t0 = 0;
  std::string ts = tstr;
 size_t dot = ts.find('.');
  if(dot == std::string::npos) dot = ts.find(',');
 
 if(dot != std::string::npos) {
  std::string frac = ts.substr(dot + 1);
  while(frac.length() < 6) frac += '0';
  t0 = std::stoull(ts.substr(0, dot)) * 1000000ULL + std::stoull(frac.substr(0,6));
  } else {
  uint64_t raw = std::stoull(ts);
   t0 = (raw > 100000000000ULL) ? raw : raw * 1000000ULL;
 }
 
 uint64_t del = std::stoull(dstr) * 1000;

  int in = socket(AF_INET, SOCK_DGRAM, 0);
 int y = 1;
 setsockopt(in, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y));
sockaddr_in bin{};
 bin.sin_family = AF_INET;
  bin.sin_addr.s_addr = INADDR_ANY;
 bin.sin_port = htons(47002);
 bind(in, (sockaddr*)&bin, sizeof(bin));

  int out = socket(AF_INET, SOCK_DGRAM, 0);
 sockaddr_in bout{};
 bout.sin_family = AF_INET;
  bout.sin_addr.s_addr = inet_addr("127.0.0.1");
bout.sin_port = htons(47020);

  std::vector<slot> jb(1024);
 uint32_t nxt = 0;
 std::map<uint32_t, grp> grps;

 auto rec = [&](uint32_t b) {
  if(grps.find(b) == grps.end()) return;
  grp& g = grps[b];
  bool ch = 1;
   while(ch) {
   ch = 0;
   if(g.pa && !g.d[0] && g.d[1]) {
   xorp(g.dp[0], g.dp[1], g.pap); g.d[0] = ch = 1;
   if(b >= nxt) { jb[b % 1024].r = 1; std::memcpy(jb[b % 1024].p, g.dp[0], 160); }
    } else if(g.pa && g.d[0] && !g.d[1]) {
   xorp(g.dp[1], g.dp[0], g.pap); g.d[1] = ch = 1;
    if(b+1 >= nxt) { jb[(b+1) % 1024].r = 1; std::memcpy(jb[(b+1) % 1024].p, g.dp[1], 160); }
    }
    
    if(g.pb && !g.d[1] && g.d[2]) {
     xorp(g.dp[1], g.dp[2], g.pbp); g.d[1] = ch = 1;
   if(b+1 >= nxt) { jb[(b+1) % 1024].r = 1; std::memcpy(jb[(b+1) % 1024].p, g.dp[1], 160); }
    } else if(g.pb && g.d[1] && !g.d[2]) {
    xorp(g.dp[2], g.dp[1], g.pbp); g.d[2] = ch = 1;
    if(b+2 >= nxt) { jb[(b+2) % 1024].r = 1; std::memcpy(jb[(b+2) % 1024].p, g.dp[2], 160); }
   }
   }
  };

 uint8_t pkt[256];
  sockaddr_in saddr{};

 while(1) {
  uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  uint64_t disp = t0 + del + nxt * 20000ULL - 2000ULL; 
   
  int wait = (disp > now) ? ((disp - now) / 1000) : 0;
  pollfd fds[1] = {{in, POLLIN, 0}};
   poll(fds, 1, wait);

  if(fds[0].revents & POLLIN) {
    socklen_t slen = sizeof(saddr);
   if(recvfrom(in, pkt, sizeof(pkt), 0, (sockaddr*)&saddr, &slen) == 165) {
     uint32_t sq;
     std::memcpy(&sq, pkt, 4);
     sq = ntohl(sq);
     uint8_t typ = pkt[4];
     
     if(typ == 0 && sq >= nxt && sq < nxt + 1024) {
      jb[sq % 1024].r = 1;
      std::memcpy(jb[sq % 1024].p, pkt + 5, 160);
      grps[sq - (sq % 3)].d[sq % 3] = 1;
     std::memcpy(grps[sq - (sq % 3)].dp[sq % 3], pkt + 5, 160);
       rec(sq - (sq % 3));
      } else if(typ == 1 && sq + 3 > nxt) {
     grps[sq].pa = 1;
      std::memcpy(grps[sq].pap, pkt + 5, 160);
      rec(sq);
    } else if(typ == 2 && sq + 3 > nxt) {
     grps[sq].pb = 1;
     std::memcpy(grps[sq].pbp, pkt + 5, 160);
       rec(sq);
     }
    }
  }

  now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
   while(now >= disp) {
   uint8_t obuf[164];
    uint32_t net = htonl(nxt);
    std::memcpy(obuf, &net, 4);

   if(jb[nxt % 1024].r) std::memcpy(obuf + 4, jb[nxt % 1024].p, 160);
   else std::memset(obuf + 4, 0, 160);
   
    sendto(out, obuf, 164, 0, (sockaddr*)&bout, sizeof(bout));
    jb[nxt % 1024].r = 0;

    if((nxt % 3) == 2) grps.erase(nxt - 2);

    ++nxt;
   disp = t0 + del + nxt * 20000ULL - 2000ULL;
   }
  }
 return 0;
}