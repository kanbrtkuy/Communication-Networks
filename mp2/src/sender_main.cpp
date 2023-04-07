
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>


#include <iostream>
#include <queue>
#include <functional>
#include <memory>

#include "Connector.h"

using namespace std;

#define BUFFER_SIZE 400
#define MAX_SEQ_NUMBER 100000

// safe file pointer
unique_ptr<FILE, function<void(FILE *)>> uni_fp;

// store unknown socket value
struct sockaddr_storage their_addr;
socklen_t addr_len = sizeof their_addr;

// note: should be large
unsigned long long int seq_number;
unsigned long long int remainBytesToLoad;

queue<Packet> buffer;
queue<Packet> ack_queue;
SenderConnector connector;
CongestionController cc{BUFFER_SIZE};

void open_file(char *filename, unsigned long long int bytesToTransfer);

int load_buffer(unsigned long long int pkt_number);

void send_packets();


void reliablyTransfer(char *hostname, unsigned short int hostUDPport,
                      char *filename, unsigned long long int bytesToTransfer) {

    connector.get_socket(hostname, hostUDPport);
    connector.set_sock_timeout();
    open_file(filename, bytesToTransfer);

    load_buffer(BUFFER_SIZE);
    send_packets();
    while (!buffer.empty() || !ack_queue.empty()) {
        if (connector.recvfrom_null() == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK)
                diep("Can not Receive the Main ACK", 2);

            cout << "Time out. Resend Packet #" << ack_queue.front().seq_num << endl;
            connector.send_pkt_error(&ack_queue.front());
            cc.timeout();
            continue;
        }
        Packet pkt{};
        memcpy(&pkt, connector.pkt_buffer, sizeof(Packet));
        if (pkt.msg_type == Packet::ACK) {
            cout << "receive ack" << pkt.ack_num << endl;
            if (pkt.ack_num == ack_queue.front().seq_num) {
                cc.normal();
                if (cc.isDupAck()) {
                    cc.handleDupAck();
                    connector.send_pkt_error(&ack_queue.front());
                }
            } else {
                if (pkt.ack_num > ack_queue.front().seq_num) {
                    while (true) {
                        if (ack_queue.empty() || ack_queue.front().seq_num >= pkt.ack_num) break;
                        cc.newAck();
                        ack_queue.pop();
                    }
                    send_packets();
                }
            }
        }
    }

    // send FIN
    Packet pkt{};
    while (true) {
        pkt.msg_type = Packet::FIN;
        pkt.data_size = 0;
        if (connector.send_pkt(&pkt) == -1) {
            diep("Fail to Send FIN to Receiver", 2);
        }
        Packet ack{};
        if (recvfrom(connector.sfd, connector.pkt_buffer, sizeof(Packet), 0,
                     (struct sockaddr *) &their_addr, &addr_len) == -1) {
            diep("Fail to receive from Receiver", 2);
        }
        memcpy(&ack, connector.pkt_buffer, sizeof(Packet));
        if (ack.msg_type == Packet::FIN_ACK) {
            cout << "Sender Receive the FIN_ACK" << endl;
            return;
        }
    }
}

int main(int argc, char **argv) {
    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
    return 0;
}


void send_packets() {

    if (buffer.empty()) {
        cout << "Buffer Empty. Stop Sending Packets" << endl;
        return;
    }

    auto k = cc.CW - ack_queue.size();

    auto nums_pkts_to_send = min((unsigned long long int) k, (unsigned long long int) buffer.size());
    if (k < 1) {
        connector.send_pkt_error(&ack_queue.front());
        cout << "Sent pkt #" << ack_queue.front().seq_num << endl;
        return;
    }


    for (int i = 0; i < nums_pkts_to_send; ++i) {
        connector.send_pkt_error(&buffer.front());
        cout << "Sent Packet #" << buffer.front().seq_num << endl;
        ack_queue.push(buffer.front());
        buffer.pop();
    }
    load_buffer(nums_pkts_to_send);
}

// load data from file and then construct pks and add to buffer for sending
int load_buffer(unsigned long long int pkt_number) {
    if (pkt_number == 0) return 0;
    int cnt = 0;
    for (int i = 0; i < pkt_number; ++i) {
        if (remainBytesToLoad == 0) break;
        unsigned long long int byte_of_pkt = min(remainBytesToLoad, (unsigned long long int) DATA_SIZE);

        char data_buffer[DATA_SIZE];
        size_t file_size = fread(data_buffer, sizeof(char), byte_of_pkt, uni_fp.get());
        if (file_size > 0) {
            Packet pkt{};
            pkt.data_size = (int) file_size;
            pkt.msg_type = Packet::DATA;
            pkt.seq_num = (int) seq_number;
            memcpy(pkt.data, &data_buffer, sizeof(char) * byte_of_pkt);
            buffer.push(pkt);
            if (++seq_number > MAX_SEQ_NUMBER)
                seq_number = 0;
        }
        remainBytesToLoad -= file_size;
        cnt = i;
    }
    return cnt;
}


void open_file(char *filename, unsigned long long int bytesToTransfer) {

    FILE *_fp = fopen(filename, "rb");
    if (_fp == nullptr) {
        cout << "Could Not Open File" << endl;
        exit(1);
    }
    uni_fp = unique_ptr<FILE, function<void(FILE *)>>(
            _fp, [](FILE *fp) {
                fclose(fp);
            });

    remainBytesToLoad = bytesToTransfer;
    unsigned long long int num_pkt_total;
    num_pkt_total = (unsigned long long int) (remainBytesToLoad / DATA_SIZE + 1);
    cout << num_pkt_total << endl;
}