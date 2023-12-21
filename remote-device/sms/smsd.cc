#include <arpa/inet.h> // inet_addr
#include <cstdint>
#include <fcntl.h> // for open
#include <iostream>
#include <netinet/in.h> // sockaddr_in
#include <stdexcept>
#include <unistd.h>

#define log(a...)                                                              \
  {                                                                            \
    fprintf(stderr, "info: " a);                                               \
    fputc('\n', stderr);                                                       \
  }
#define err(a...)                                                              \
  {                                                                            \
    fprintf(stderr, "error: " a);                                              \
    fputc('\n', stderr);                                                       \
    return -1;                                                                 \
  }

static const char SMS_NR[] = "+48691559164";
// #define SMS_NR1 "+48504975739"
// #define SMS_NR2 "+48601249626"

#define ENDL "\r\n"

static const char sms_init[] = "AT+CMGF=1\r";
static const char sms_beg[] = "\rAT+CMGS=\"";
static const char sms_start[] = "\"\rmaluzyn:\n";
static const char sms_end[] = "\x1A\r";

class SMSService {
  int fd = -1;

  void transmit(const char *buf, int len, int fail_limit = 10) {
    int sent = 0;
    do {
      auto s = write(fd, buf + sent, len - sent);
      if (s > 0)
        sent += s;
      else {
        fail_limit--;
        usleep(150000);
      }
    } while (sent < len && fail_limit);
    if (sent < len) {
      throw std::runtime_error("sending failed");
    }
  }

  int receive(char *buf, int buf_size, int wait_limit = 100) {
    do {
      auto r = read(fd, buf, buf_size - 1);
      if (r > 0) {
        buf[r] = 0;
        return r;
      }
    } while (--wait_limit);

    buf[0] = 0;
    return 0;
  }

public:
  SMSService() {
    fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
      throw std::runtime_error("Opening AT commands device failed");
    }

    transmit(sms_init, sizeof(sms_init) - 1);

    usleep(150000);

    char buf[1024];
    auto l = receive(buf, sizeof(buf));
    std::cout << l << std::endl;
    std::cout << buf << std::endl;

    transmit(sms_beg, sizeof(sms_beg) - 1);
    transmit(SMS_NR, sizeof(SMS_NR) - 1);
    transmit(sms_start, sizeof(sms_start) - 1);
    transmit(sms_end, sizeof(sms_end) - 1);

    usleep(150000);
    l = receive(buf, sizeof(buf));
    std::cout << l << std::endl;
    std::cout << buf << std::endl;
  }
};

class AlarmService {
  constexpr static int Q_MOVE = 0x00;
  constexpr static int Q_ALTAMP = 0x01;
  constexpr static int Q_AL = 0x02;
  // #define Q_AL_MEM
  // #define Q_ALTAMP_MEM

  constexpr static int MAX_PACKET = 64;

  int calc_chk(uint8_t *data, int l) {
    int r = 0x147A;
    while (l--) {
      r = ((r << 1) | ((r >> 15) & 1));
      r = ~r;
      r += ((r >> 8) & 0xff) + *(data++);
    }
    return ((r & 0xff) << 8) | ((r >> 8) & 0xff);
  }

  constexpr static int ZONES_MAX_SIZE = 32;
  uint32_t zones_alarm[ZONES_MAX_SIZE];

  void bitlist_chk(char *name, void *buf, int l, uint32_t *cur_bits,
                   int bits_size) {
    uint32_t *s = (uint32_t *)buf;
    int i;
    l >>= 2;
    if (l > bits_size)
      l = bits_size;
    for (i = 0; i < l; i++) {
      int a = s[i] ^ cur_bits[i];
      if (a) {
        int o;
        for (o = 0; o < 32; o++) {
          if (a & 1)
            sms_printf("%s: s%d = %s", name, i * 32 + o + 1,
                       (s[i] & (1 << o) ? "alarm" : "ok"));
          a >>= 1;
        }
        cur_bits[i] = s[i];
      }
    }
  }

  void proc_packet(uint8_t *buf, int l) {
    if (l < 3)
      return;
    uint16_t chk = calc_chk(buf, l - 2);
    if (chk != *(uint16_t *)(buf + l - 2))
      return;
    l -= 2;
    /*int i;
    log( "packet length: %d", l );
    for ( i=0; i<l; i++ ) log( "char: %02X", buf[i]&0xff );//*/
    char cc = buf[0];
    buf++;
    l--;
    switch (cc) {
    case Q_AL: {
      bitlist_chk("A", buf, l, zones_alarm, ZONES_MAX_SIZE);
      // sms_flush();
      // log("zones violation data");
    } break;
    default:
      log("unsupported packet code: %02X", cc);
    }
  }

  uint8_t recv_buf[1024];
  uint8_t packet_buf[1024];
  int packet_l = -2;
  uint8_t packet_lc = 0;
  int extract_packet(uint8_t *data, int l) {
    int a = 0;
    while (packet_l < 0 && a < l)
      if (data[a++] == 0xFE)
        packet_l++;
      else
        packet_l = -2;
    while (a < l && packet_l < MAX_PACKET) {
      uint8_t c = data[a++];
      if (packet_lc == 0xFE) {
        if (c == 0x0D) {
          proc_packet(packet_buf, packet_l);
          packet_l = -2;
          packet_lc = 0;
          return a;
        } else if (c == 0xFE)
          return a - 2;
        else
          packet_buf[packet_l++] = c;
      } else if (c != 0xFE)
        packet_buf[packet_l++] = c;
      packet_lc = c;
    }
    return 0;
  }

  int socket_init(const char *ip, uint16_t port) {
    struct sockaddr_in server;
    int sock;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
      err("Could not create socket");

    struct timeval tv;
    tv.tv_sec = 30; /* 30 Secs Timeout */
    tv.tv_usec = 0; // Not init'ing this can cause strange errors
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
                   sizeof(struct timeval)))
      err("setsockopt failed.");

    log("Socket created");

    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    // Connect to remote server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
      err("connect failed. Error");

    log("Connected\n");

    return sock;
  }
};

int main() {
  auto s = SMSService();
  return 0;
}
