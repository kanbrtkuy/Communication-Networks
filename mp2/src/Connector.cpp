#include "Connector.h"

using namespace std;

// ReceiverConnector

ssize_t ReceiverConnector::recv_pkt(Packet *pkt) {
    ssize_t out = recv_buf();
    memcpy(pkt, buf, sizeof(Packet));
    return out;
}

void ReceiverConnector::send_final_ack(int nextACK) {
    send_ack(nextACK, Packet::FIN_ACK);
}

void ReceiverConnector::send_ack(int nextACK, int ack_type) {
    Packet ack;
    ack.msg_type = ack_type;
    ack.ack_num = nextACK;
    ack.data_size = 0;  // data size is 0 since we are sending ack
    send_pkt(&ack);
    cout << "sent ack" << ack.ack_num << endl;
}

ssize_t ReceiverConnector::send_pkt(Packet *pkt) {
    memcpy(buf, pkt, sizeof(Packet));
    return send_buf();
}

ssize_t ReceiverConnector::recv_buf() {
    int recvbytes = recvfrom(sfd, buf, sizeof(Packet), 0,
                             (struct sockaddr *) &_addr, &addrlen);
    if (recvbytes <= 0) {
        diep("Connection closed\n", 2);
        return 0;
    } else {
        return recvbytes;
    }
}

ssize_t ReceiverConnector::send_buf() {
    return sendto(sfd, buf, sizeof(Packet), 0, (struct sockaddr *) &_addr,
                  addrlen);
}


ReceiverConnector::ReceiverConnector(unsigned short int myUDPport) {
    get_socket();
    bind_socket(myUDPport);
}

ReceiverConnector::~ReceiverConnector() { close(sfd); }

void ReceiverConnector::get_socket() {
    if ((sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) diep("socket", 1);
    cout << "Socket: " << sfd << endl;
}

void ReceiverConnector::bind_socket(unsigned short int myUDPport) {
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(sfd, (struct sockaddr *) &si_me, sizeof(si_me)) == -1) diep("bind", 1);
}


// SenderConnector
SenderConnector::SenderConnector() {
//    get_socket(hostname, hostUDPport);
}

SenderConnector::~SenderConnector() { close(sfd); }

void SenderConnector::get_socket(char *hostname, unsigned short int hostUDPport) {
    int rv, sockfd;
    char portStr[10];
    struct addrinfo hints, *recvinfo;
    sprintf(portStr, "%d", hostUDPport);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    memset(&recvinfo, 0, sizeof recvinfo);
    if ((rv = getaddrinfo(hostname, portStr, &hints, &recvinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %sfd\n", gai_strerror(rv));
        sfd = 1;
        return;
    }

    // loop through all the results and bind to the first we can
    for (p = recvinfo; p != nullptr; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: error opening socket");
            continue;
        }
        break;
    }
    if (p == nullptr) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    sfd = sockfd;
}

void SenderConnector::set_sock_timeout() const {
    struct timeval timeout{0, 2 * RTT};
    if (setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <
        0)
        fprintf(stderr, "Error: Setting Socket Timeout\n");
}

ssize_t SenderConnector::recv_pkt(Packet *pkt) {
    return 0;
}

ssize_t SenderConnector::send_pkt(Packet *pkt) {
    memcpy(pkt_buffer, pkt, sizeof(Packet));
    return sendto(sfd, pkt_buffer, sizeof(Packet), 0, p->ai_addr,
                  p->ai_addrlen);
}

ssize_t SenderConnector::recvfrom_null() {
    return recvfrom(sfd, pkt_buffer, sizeof(Packet), 0, nullptr,
                    nullptr);
}

ssize_t SenderConnector::send_pkt_error(Packet *pkt) {
    auto out = send_pkt(pkt);
    if (out == -1) {
        printf("Fail to send %d pkt", pkt->seq_num);
        diep("Error: data sending", 2);
    }
    return out;
}

void CongestionController::timeout() {
    switch (congestion_ctrl_state) {
        case SLOW_START:
            SST = (int) (CW / 2.0);
            CW = 1;
            dupAckCnt = 0;
            return;
        case CONGESTION_AVOIDANCE:
            SST = (int) (CW / 2.0);
            CW = 1;
            dupAckCnt = 0;
            change_state(SLOW_START);
            return;
        case FAST_RECOVERY:
            SST = (int) (CW / 2.0);
            CW = 1;
            dupAckCnt = 0;
            change_state(SLOW_START);
            return;
        default:
            break;
    }
}

void CongestionController::newAck() {
    switch (congestion_ctrl_state) {
        case SLOW_START:
            dupAckCnt = 0;
            if (++CW >= BUFFER_SIZE)
                CW = BUFFER_SIZE - 1;
            if (CW >= SST) {
                change_state(CONGESTION_AVOIDANCE);
            }
            break;
        case CONGESTION_AVOIDANCE:
            CW = CW + 1.0 / CW;
            if (CW >= BUFFER_SIZE)
                CW = BUFFER_SIZE - 1;
            dupAckCnt = 0;
            break;
        case FAST_RECOVERY:
            CW = SST;
            dupAckCnt = 0;
            change_state(CONGESTION_AVOIDANCE);
            break;
        default:
            break;
    }
}

void CongestionController::normal() {
    switch (congestion_ctrl_state) {
        case SLOW_START:
            dupAckCnt++;
            if (CW >= SST) {
                change_state(CONGESTION_AVOIDANCE);
            }
            break;
        case CONGESTION_AVOIDANCE:
            dupAckCnt++;
            break;
        case FAST_RECOVERY:
            if (++CW >= BUFFER_SIZE)
                CW = BUFFER_SIZE - 1;
            break;
        default:
            break;
    }
}

void CongestionController::change_state(enum socket_state s) {
    cout << congestion_ctrl_state << " --> " << s << endl;
    congestion_ctrl_state = s;
}


CongestionController::CongestionController(int bf) {
    BUFFER_SIZE = bf;
}

bool CongestionController::isDupAck() {
    return dupAckCnt >= 2;
}

void CongestionController::handleDupAck() {
    SST = CW / 2.0;
    CW = SST + 3;
    dupAckCnt = 0;
    change_state(CongestionController::FAST_RECOVERY);
}
