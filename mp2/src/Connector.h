#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <vector>

#include "Packet.h"
#include "tools.h"

#define RTT 20 * 1000

class Connector {
public:

};

class ReceiverConnector : public Connector {
public:
    ReceiverConnector(unsigned short int myUDPport);

    ~ReceiverConnector();

    char buf[sizeof(Packet)];

    ssize_t recv_pkt(Packet *pkt);

    void send_final_ack(int nextACK);

    void send_ack(int nextACK, int ack_type = Packet::ACK);

    ssize_t send_pkt(Packet *pkt);

protected:
    struct sockaddr_in _addr;
    socklen_t addrlen = sizeof(_addr);
    struct sockaddr_in si_me, si_other;
    int sfd;

    ssize_t recv_buf();

    ssize_t send_buf();

    void get_socket();

    void bind_socket(unsigned short int myUDPport);
};


class SenderConnector : public Connector {
public:
    SenderConnector();

    ~SenderConnector();

    struct addrinfo *p;
    int sfd;
    char pkt_buffer[sizeof(Packet)];
//    int MAX_SEND_PKTS =  1000000000;
// protected:

    void get_socket(char *hostname, unsigned short int hostUDPport);

    void set_sock_timeout() const;

    ssize_t recv_pkt(Packet *pkt);

    ssize_t send_pkt(Packet *pkt);

    ssize_t recvfrom_null();

    // send packages with error message (on pkg_num)
    ssize_t send_pkt_error(Packet *pkt);
};

class CongestionController {
public:
    double CW = 1.0;
    int SST = 64, dupAckCnt = 0;
    int congestion_ctrl_state = SLOW_START;
    enum socket_state {
        SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY, FIN_WAIT
    };
    int BUFFER_SIZE;
    CongestionController(int bf);
//    void congestionControl(bool newACK, bool timeout);
    void timeout();
    void newAck();
    void normal();
    void change_state(enum socket_state s);
    bool isDupAck();
    void handleDupAck();
};