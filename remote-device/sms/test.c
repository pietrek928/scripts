#include<stdio.h> //printf
#include<stdlib.h>
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr
#include<stdint.h>
#include<fcntl.h> // for open
#include<unistd.h>

#define MAX_PACKET 64
#define ZONES_MAX_SIZE 32
#define MAX_SMS 192

#define ALARM_IP "192.168.123.225"
#define ALARM_PORT 10967

#define SMS_NR_CNT 2

//#define SMS_NR "+48691559164"
#define SMS_NR1 "+48504975739"
#define SMS_NR2 "+48601249626"
#define SMS_TM_CNT 50

#define log( a... ) { fprintf( stderr, "info: " a ); fputc('\n',stderr); }
#define err( a... ) { fprintf( stderr, "error: " a ); fputc('\n',stderr); return -1; }

#define sms_printf( a... ) { \
    if ( sms_len<MAX_SMS ) { \
        int havbkdf = snprintf( sms_buf+sms_len, MAX_SMS-sms_len, a ); \
        if (havbkdf>0) sms_len+=havbkdf; \
        if ( sms_len<MAX_SMS ) \
            sms_buf[sms_len++] = '\n'; \
    } else sms_len = MAX_SMS; \
}

char resp_buf[128];

void clear_in(int sms_fd) {
    usleep(150000);
    int r = 0;
    do {
        r = read(sms_fd, resp_buf, sizeof(resp_buf)-2);
	if (r > 1) {
	    resp_buf[r] = 0;
	    log("ignored(len: %d): %s\n", r, resp_buf);
	}
    } while (r>1);
}

int wait_resp(int sms_fd) {
   usleep(150000);
   int wt = 100;
   int r = 0;
   do {
       r = read(sms_fd, resp_buf, sizeof(resp_buf)-2);
       if (r > 1) {
            resp_buf[r] = 0;
            log("response(len: %d): %s\n", r, resp_buf);
       }
   } while (r<=1 && --wt);
   if (!wt) return -1;
   return 0;
}

int transmit(int sms_fd, char *buf, int len) {
   int snd = 0;
   int flimit = 100;
   int r;
   while (snd < len && flimit) {
       r = write(sms_fd, buf+snd, len-snd);
       if (r > 0) snd += r; else flimit--;
   }
   if (snd < len) {
      log("sending error: %d\n", r);
      return -1;
   }
   return 0;
}

int sms_len = 0;
char sms_buf[ MAX_SMS+5 ];
int sms_fd = -1;
char sms_inits[] = "AT+CMGF=1\r";
//  601249626
#define COMMON_BEG "\rAT+CMGS=\""
char* sms_beg[ SMS_NR_CNT ] = {
    COMMON_BEG SMS_NR1 "\"\rmaluzyn:\n",
    COMMON_BEG SMS_NR2 "\"\rmaluzyn:\n",
};
int sms_beg_l[ SMS_NR_CNT ];
char sms_end[] = "\x1A\r";
int sms_init() {
    int fd = open( "/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY );
    if ( fd == -1 ) { log( "opening sms device failed." ) return -1; }
    clear_in(fd);
    if ( transmit( fd, sms_inits, sizeof(sms_inits)-1 ) || wait_resp(fd) ) {
        log("sms init failed.");
        close(fd);
	return -1;
    }
    sms_len = 0;
    return fd;
}

void sms_flush() {
_b:;
    if ( !sms_len ) return;
    int r = 0;
    if ( sms_fd != -1 ) {
        int i;
        for ( i=0; i<SMS_NR_CNT; i++ ) {
	    log("sended: %s", sms_buf);
	    clear_in(sms_fd);
            r |= transmit( sms_fd, sms_beg[i], sms_beg_l[i] );
            r |= transmit( sms_fd, sms_buf, sms_len );
            r |= transmit( sms_fd, sms_end, sizeof(sms_end)-1 );
            r |= wait_resp(sms_fd);
        }
        if ( r <= 0 ) close( sms_fd );
    } else r = -1;
    if ( r < 0 ) {
        log("sms transmission failed.");
        sms_fd = sms_init();
        if ( sms_fd == -1 ) { log("connecting to modem failed."); exit( 1 ); }
        goto _b;
    }
    sms_len = 0;
}

int calc_chk( char *data, int l ) {
    int r = 0x147A;
    while ( l-- ) {
        r =( (r<<1) | ((r>>15)&1) );
        r = ~r;
        r += ((r>>8)&0xff) + *(data++);
    }
    return ((r&0xff)<<8) | ((r>>8)&0xff);
}

char send_buf[1000];
int send_data( int sock, char *data, int l ) {
    int s = calc_chk( data, l );
    char *b = send_buf; int ll = l+6;
    *(b++) = *(b++) = 0xfe;
    while ( l-- ) *(b++) = *(data++);
    *((uint16_t*)b) = s; b += 2;
    *(b++) = 0xfe; *(b++) = 0x0d;
    return send( sock, send_buf, ll, 0 );
}

#define Q_MOVE      0x00
#define Q_ALTAMP    0x01
#define Q_AL        0x02
#define Q_AL_MEM
#define Q_ALTAMP_MEM

uint32_t zones_alarm[ ZONES_MAX_SIZE ];

void bitlist_chk( char *name, void *buf, int l, uint32_t *cur_bits, int bits_size ) {
    uint32_t *s = (uint32_t*)buf;
    int i; l>>=2;
    if ( l > bits_size ) l = bits_size;
    for ( i=0; i<l; i++ ) {
        int a = s[i]^cur_bits[i];
        if ( a ) {
            int o;
            for ( o=0; o<32; o++ ) {
                if ( a&1 ) sms_printf( "%s: s%d = %s", name, i*32+o+1, (s[i]&(1<<o)?"alarm":"ok") );
                a>>=1;
            }
            cur_bits[i] = s[i];
        }
    }
}

void proc_packet( uint8_t *buf, int l ) {
    if ( l<3 ) return;
    uint16_t chk = calc_chk( buf, l-2 );
    if ( chk != *(uint16_t*)(buf+l-2) ) return;
    l -= 2;
    /*int i; 
    log( "packet length: %d", l );
    for ( i=0; i<l; i++ ) log( "char: %02X", buf[i]&0xff );//*/
    char cc = buf[0]; buf++; l--;
    switch( cc ) {
        case Q_AL:{
            bitlist_chk( "A", buf, l, zones_alarm, ZONES_MAX_SIZE );
            //sms_flush();
            //log("zones violation data");
            }break;
        default:
                log( "unsupported packet code: %02X", cc );
    }
}

uint8_t recv_buf[1024];
uint8_t packet_buf[1024];
int packet_l = -2;
uint8_t packet_lc = 0;
int extract_packet( uint8_t *data, int l ) {
    int a = 0;
    while ( packet_l<0 && a<l )
        if ( data[a++]==0xFE ) packet_l++;
            else packet_l=-2;
    while ( a < l && packet_l<MAX_PACKET ) {
        uint8_t c = data[a++];
        if ( packet_lc==0xFE ) {
            if ( c == 0x0D ) {
                proc_packet( packet_buf, packet_l );
                packet_l = -2;
                packet_lc = 0;
                return a;
            } else
            if ( c == 0xFE ) return a-2;
                else packet_buf[packet_l++] = c;
        } else if ( c != 0xFE ) packet_buf[packet_l++] = c;
        packet_lc = c;
    }
    return 0;
}

int send_q( int sock, uint8_t q ) {
    return send_data( sock, &q, 1 );
}

int socket_init() {
    struct sockaddr_in server;
    int sock;
     
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) err("Could not create socket");
    
    struct timeval tv;
    tv.tv_sec = 30;  /* 30 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors
    if ( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval)) ) err( "setsockopt failed." );
    
    log("Socket created");
     
    server.sin_addr.s_addr = inet_addr(ALARM_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons( ALARM_PORT );
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
                err("connect failed. Error");
     
    log("Connected\n");

    return sock;
}

int main(int argc, char **argv)
{
    int i;
    for ( i=0; i<SMS_NR_CNT; i++ ) sms_beg_l[i] = strlen( sms_beg[i] );
    sms_fd = sms_init();
    usleep( 500000 );

    /*sms_printf( "!!!!! BOOOOOM !!!!!" );
    sms_flush();

    return 0;*/
    for ( i=0; i<ZONES_MAX_SIZE; i++ ) zones_alarm[i]=0;

    int sock = socket_init();
    if ( sock == -1 ) err("creating socket failed.");

    //char aaa[] = { 0x00 };
    //send(sock , buf, strlen(message) , 0);

    //char recv_buf[ 1000 ];
    sms_printf( "program started." );
    sms_flush();

    int tm_cnt = SMS_TM_CNT;
    while ( 1 ) {
          //log( "send: %d", 
                  send_q( sock, Q_AL );
          //);
        int l = recv( sock, packet_buf, sizeof(packet_buf), 0 );
          //log( "recv: %d", l );
        if ( l<=0 ) {
            //err( "connection error" );
            close( sock );
            log( "recreating socket..." );
            sock = socket_init();
            if ( sock == -1 ) err("creating socket failed.");
        }
        //log( "recv: %d", l );
        if ( l>0 )
            extract_packet( packet_buf, l ); 
        if ( !(tm_cnt--) ) {
            sms_flush();
            tm_cnt = SMS_TM_CNT;
        }
        usleep( 100000 );
    }

    return 0;
}

