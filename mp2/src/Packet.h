// Packet structure used for transfering
#define DATA_SIZE 1000

class Packet {
 public:
//   const static int DATA_SIZE = 2000;
  int data_size;
  int seq_num;
  int ack_num;
  int msg_type;
  char data[DATA_SIZE];

  bool is_data() const;
  bool is_fin() const;

  // DATA 0 SYN 1 ACK 2 FIN 3 FINACK 4
  enum state { DATA, SYN, ACK, FIN, FIN_ACK };
};