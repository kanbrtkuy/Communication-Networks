#include <cstdio>
#include <cstdlib>

#include <cstring>
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>

#include "Connector.h"

#define TOT_COUNT 2000
using namespace std;

class PacketBuffer {
public:
    char buffer[DATA_SIZE];
};

vector<PacketBuffer> data_buffer(TOT_COUNT, PacketBuffer());


template<typename T>
class atomic_queue {
public:
    void push(const T &value) {
        lock_guard<mutex> lock(m_mutex);
        m_queque.push(value);
    }

    T pop() {
        lock_guard<mutex> lock(m_mutex);
        T v = m_queque.front();
        m_queque.pop();
        return v;
    }

    bool is_empty() {
        lock_guard<mutex> lock(m_mutex);
        return m_queque.empty();
    }

private:
    queue<T> m_queque;
    mutable mutex m_mutex;
};

FILE *fp;
atomic_queue<pair<int, int>> index_to_write;
atomic<bool> writing_data;
atomic<bool> stop;

void write_data() {
    while (!stop) {
        while (writing_data) {
            if (!index_to_write.is_empty()) {
                auto a = index_to_write.pop();
                int idx = a.first;
                int size = a.second;
                fwrite(&(data_buffer[idx].buffer), sizeof(char), size,
                       fp);
            } else {
                break;
            }
        }
    }
}


void reliablyReceive(unsigned short int myUDPport, char *destinationFile) {

    fp = fopen(destinationFile, "wb");
    ReceiverConnector connector(myUDPport);


    writing_data = true;
    stop = false;
    thread writer(write_data);

    int nextACK = 0;
    vector<bool> already_ACK(TOT_COUNT, false);
    vector<int> data_size(TOT_COUNT, DATA_SIZE);


    int cur_idx = 0;
    while (true) {
        Packet pkt{};
        connector.recv_pkt(&pkt);

        cout << "Receive Packet #" << pkt.seq_num << ", Type #" << pkt.msg_type << endl;
        if (pkt.is_data()) {
            if (pkt.seq_num == nextACK) {
                memcpy(&(data_buffer[cur_idx].buffer), &pkt.data, pkt.data_size);
                index_to_write.push({cur_idx, pkt.data_size});
                writing_data = true;
                cout << "Write Packet #" << pkt.seq_num << endl;
                nextACK++;
                if (++cur_idx >= TOT_COUNT)
                    cur_idx = 0;
                while (already_ACK[cur_idx]) {
                    index_to_write.push({cur_idx, data_size[cur_idx]});
                    writing_data = true;
                    cout << "Write Index #" << cur_idx << endl;
                    already_ACK[cur_idx] = false;
                    if (++cur_idx >= TOT_COUNT)
                        cur_idx = 0;
                    nextACK++;
                }
            } else if (pkt.seq_num > nextACK) {
                int ahead_idx = (cur_idx + pkt.seq_num - nextACK) % TOT_COUNT;
                memcpy(&(data_buffer[ahead_idx].buffer), &pkt.data, pkt.data_size);

                already_ACK[ahead_idx] = true;
                data_size[ahead_idx] = pkt.data_size;
            }
            connector.send_ack(nextACK, Packet::ACK);
        } else if (pkt.is_fin()) {
            connector.send_final_ack(nextACK);
            break;
        }
    }
    stop = true;
    cout << destinationFile << "is received." << endl << endl;
    writer.join();
    fclose(fp);
}

int main(int argc, char **argv) {
    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %sfd UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}
