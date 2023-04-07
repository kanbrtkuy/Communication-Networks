// #define DATA_SIZE 2000
#include "Packet.h"

bool Packet::is_data() const { return msg_type == DATA; }
bool Packet::is_fin() const { return msg_type == FIN; }
