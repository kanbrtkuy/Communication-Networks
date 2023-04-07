#include <iostream>
#include <unordered_map>
#include <set>
#include <utility>
#include <vector>
#include <sstream>
#include <string>
#include <fstream>


using namespace std;
const short UNREACH = -999;


class Message {
public:
    int src_id;
    int dst_id;
    string content;

    Message(int src_id, int dst_id, string content) : src_id(src_id), dst_id(dst_id), content(std::move(content)) {}
};

class GlobalState {

public:
    // {source_id : {dest_id : {next_id, cost} }}
    unordered_map<int, unordered_map<int, pair<int, int>>> forward_table;

    // graph the network; two direction
    unordered_map<int, unordered_map<int, int>> graph;

    // set of nodes in net
    set<int> nodes_set;

    vector<Message> msgs;

    // output file
    ofstream outputStream = ofstream("output.txt");

};

auto g = GlobalState();

void init_message(ifstream &message_file);

bool check_exist(int from_node_id, int to_node_id);

void init_graph(ifstream &file_stream);

void init_forward_table();

pair<int, int> cal_point_to_dis_vec(int src_id, int dst_id, int min_id, int min_cost);

void cal_dis_vec();

void send_messages();

void update_single();

void update_all(ifstream &changes_file);


int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    // read topo file
    ifstream topo_file(argv[1]);
    init_graph(topo_file);

    // read messages
    ifstream message_file(argv[2]);
    init_message(message_file);

    // read changes
    ifstream changes_file(argv[3]);
    update_all(changes_file);
}

// Ref: https://stackoverflow.com/questions/6286371/how-do-you-get-the-remaining-string-after-a-string-extracted-from-a-sstream-vari
void init_message(ifstream &message_file) {
    string line, content;
    string src_id, dst_id;
    while (getline(message_file, line)) {
        if (!line.empty()) {
            stringstream line_ss(line);
            getline(line_ss, src_id, ' ');
            getline(line_ss, dst_id, ' ');
            getline(line_ss, content);
            g.msgs.emplace_back(stoi(src_id), stoi(dst_id), content);
        }
    }
}

bool check_exist(int from_node_id, int to_node_id) {
    if (!g.graph.count(from_node_id))
        return false;
    return g.graph[from_node_id].count(to_node_id);
}


void init_graph(ifstream &file_stream) {
    string line;
    int src_id, dst_id, cost;

    // initialize graph
    while (getline(file_stream, line)) {
        stringstream line_ss(line);
        line_ss >> src_id >> dst_id >> cost;
        g.graph[src_id][dst_id] = cost;
        g.graph[dst_id][src_id] = cost;
        g.nodes_set.insert(src_id);
        g.nodes_set.insert(dst_id);
    }

    for (auto i: g.nodes_set) {
        for (auto j: g.nodes_set) {
            if (i == j) {
                g.graph[i][j] = 0;
            } else {
                if (!check_exist(i, j)) {
                    g.graph[i][j] = UNREACH;
                }
            }
        }
    }
}

void init_forward_table() {
    for (int src_id: g.nodes_set) {
        for (int dst_id: g.nodes_set) {
            if (g.graph[src_id][dst_id] == UNREACH) {
                g.forward_table[src_id][dst_id] = {UNREACH, UNREACH};
            } else {
                g.forward_table[src_id][dst_id] = {dst_id, g.graph[src_id][dst_id]};
            }
        }
    }
}

// calculate the shortest path
pair<int, int> cal_point_to_dis_vec(int src_id, int dst_id, int min_id, int min_cost) {
    for (int inter_node: g.nodes_set) {
        // if inter_node is reachable from src_id and dst_id
        if (g.graph[src_id][inter_node] != UNREACH && g.forward_table[inter_node][dst_id].second != UNREACH) {
            auto new_cost = g.graph[src_id][inter_node] + g.forward_table[inter_node][dst_id].second;
            // if min_cost is not initialized or the cost is smaller than min_cost
            if (!(new_cost > 0 && min_cost > 0 && new_cost < min_cost) && !(new_cost > 0 && min_cost < 0)) continue;
            min_id = inter_node;
            min_cost = new_cost;
        }
    }
    return {min_id, min_cost};
}


void cal_dis_vec() {

    for (auto i = 0; i < g.nodes_set.size(); i++) {
        for (int src_id: g.nodes_set) {
            for (int dst_id: g.nodes_set) {
                auto [min_id, min_cost] = g.forward_table[src_id][dst_id];
                g.forward_table[src_id][dst_id] = cal_point_to_dis_vec(src_id, dst_id, min_id, min_cost);
            }
        }
    }
    // output forward table
    for (int src_id: g.nodes_set) {
        for (int dst_id: g.nodes_set) {
            auto [f, s] = g.forward_table[src_id][dst_id];
            g.outputStream << dst_id
                           << " "
                           << f
                           << " "
                           << s
                           << endl;
        }
    }
}

void send_messages() {
    int src_id, dst_id, tmp_id;
    for (auto &msg: g.msgs) {
        src_id = msg.src_id;
        dst_id = msg.dst_id;
        tmp_id = src_id;
        g.outputStream << "from " << src_id << " to " << dst_id << " cost ";
        int cost = g.forward_table[src_id][dst_id].second;
        if (cost < 0) { // destination unreachable
            g.outputStream << "infinite hops unreachable ";
        } else if (cost == 0) { // destination is source
            g.outputStream << cost << " hops ";
        } else { // destination reachable
            g.outputStream << cost << " hops ";
            while (tmp_id != dst_id) {
                g.outputStream << tmp_id << " ";
                // reach destination
                tmp_id = g.forward_table[tmp_id][dst_id].first;
            }
        }
        g.outputStream << "message " << msg.content << endl;
    }
    g.outputStream << endl;
}


void update_single() {
    init_forward_table();
    cal_dis_vec();
    send_messages();
}

void update_all(ifstream &changes_file) {
    update_single();

    // update graph
    string line;
    int src_id, dst_id, cost;
    while (getline(changes_file, line)) {
        stringstream line_ss(line);
        line_ss >> src_id >> dst_id >> cost;
        g.graph[src_id][dst_id] = cost;
        g.graph[dst_id][src_id] = cost;
        update_single();
    }
}