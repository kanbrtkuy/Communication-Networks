
#include <cstdio>
#include <climits>

#include<iostream>
#include<unordered_map>
#include<set>
#include<vector>
#include<sstream>
#include<string>
#include<fstream>

using namespace std;

unordered_map<int, unordered_map<int, int>> topo;

unordered_map<int, unordered_map<int, pair<int, int>>> forward_table;

set<int> node_set;

ofstream fpOut("output.txt");

void InitPathWeightData(const string &input_file_name) {
    ifstream infile(input_file_name);
    int src_id, dst_id, cost;

    string line;
    while (getline(infile, line)) {
        istringstream iss(line);

        if (!(iss >> src_id >> dst_id >> cost)) { break; }

        topo[src_id][dst_id] = cost;
        topo[dst_id][src_id] = cost;
        node_set.insert(src_id);
        node_set.insert(dst_id);
    }

}

void output_message(int src, int dst, const string& message) {

    if (forward_table[src][dst].second == INT_MAX ||
        !node_set.count(src) ||
        !node_set.count(dst)) {
        fpOut << "from " << src << " to " << dst << " cost infinite hops unreachable" << endl;
        return;
    }

    vector<int> hops;
    int curr_hop = src;
    while (curr_hop != dst) {
        hops.push_back(curr_hop);
        curr_hop = forward_table[curr_hop][dst].first;
    }
    fpOut << "from " << src << " to " << dst << " cost " << forward_table[src][dst].second << " hops ";
    for (int hop: hops) {
        fpOut << hop << " ";
    }
    if (hops.empty()) {
        fpOut << " ";
    }
    fpOut << "message " << message << endl;


}

void InitMessageData(const string &input_file_name) {
    ifstream message_file(input_file_name);

    string line;
    int src_id, dst_id;
    while (getline(message_file, line)) {

        if (!line.empty()) {
            stringstream line_ss(line);
            string content;
            string message;
            line_ss >> src_id >> dst_id;

            getline(line_ss, message);
            if (src_id == -999 || dst_id == -999) {
                continue;
            }
            cout << "src_id: " << src_id << " dst_id: " << dst_id << " message: " << message << endl;
            output_message(src_id, dst_id, message.substr(1));
        }

    }
}

void output_table() {
    size_t num_nodes = node_set.size();
    for (int i = 1; i <= num_nodes; i++) {
        for (int j = 1; j <= num_nodes; j++) {
            if (!node_set.count(i) || !node_set.count(j)) {
                continue;
            }
            if (forward_table[i][j].second == INT_MAX) {
                continue;
            }
            if (i == j) {
                fpOut << j << " " << j << " " << 0 << endl;
                continue;
            }
            fpOut << j << " " << forward_table[i][j].first << " " << forward_table[i][j].second << endl;
        }
    }
}

void CalculateForwardTableDijkstra(int start_u) {
    set<int> N;
    set<int> edge;
    unordered_map<int, int> D;
    unordered_map<int, int> path;

    N.insert(start_u);
    for (auto v: node_set) {
        if (v == start_u) {
            D[start_u] = 0;
        } else {
            // v adjacent to u
            if (topo[start_u].count(v)) {
                D[v] = topo[start_u][v];
                edge.insert(v);
            } else {
                D[v] = -999;
            }
        }
        path[v] = start_u;
    }


    // Loop
    int count = 1;
    while (count < node_set.size() - 1) {
        // find min
        int min_num = INT_MAX;
        int next_node_w;
        for (auto i: node_set) {
            if (N.count(i)) {
                continue;
            }
            if (D[i] > 0 && D[i] < min_num) {
                next_node_w = i;
                min_num = D[i];

            } else if (D[i] == min_num) {
                next_node_w = min(i, next_node_w);
            }
        }

        N.insert(next_node_w);

        for (auto [v, w_v]: topo[next_node_w]) {
            if (v == start_u || N.count(v)) {
                continue;
            }

            int D_v = D[v];
            int D_w = D[next_node_w];

            if (D_w == -999) {
                continue;
            } else {
                if (D_v > (D_w + w_v) || D_v == -999) {
                    D[v] = D_w + w_v;
                    path[v] = next_node_w;
                }
                if (D_v == (D_w + w_v) && next_node_w < path[v]) {
                    path[v] = next_node_w;
                }
            }
        }
        count++;
    }

    unordered_map<int, int> second_col;
    for (int i: node_set) {
        int cur = path[i];
        int pre = path[cur];
        if (cur == start_u || !edge.count(cur)) {
            cur = i;
        }
        while (pre != start_u && cur != start_u) {
            cur = path[cur];
            pre = path[cur];
        }
        second_col[i] = cur;
    }

    unordered_map<int, pair<int, int>> one_table;

    for (auto &[f, s]: D) {
        one_table[f].first = second_col[f];
        one_table[f].second = s;
    }

    forward_table[start_u] = one_table;
}

void InitForwardTable(const set<int> &n_set) {
    forward_table.clear();
    for (auto node: n_set) {
        CalculateForwardTableDijkstra(node);
    }
    output_table();
}


void UpdatePathWeightData(const string &input_file_name_1, const string &message_file) {

    ifstream change_infile(input_file_name_1);
    int src_id, dst_id, cost;

    std::string line;
    while (getline(change_infile, line)) {
        istringstream iss(line);

        if (!(iss >> src_id >> dst_id >> cost)) { break; }

        // process pairs
        if (src_id == 0 || dst_id == 0 || cost == 0) {
            continue;
        }
        if (cost == -999) {
            topo[src_id].erase(dst_id);
            topo[dst_id].erase(src_id);
            if (!topo.count(src_id)) {
                node_set.erase(src_id);
            }
            if (!topo.count(dst_id)) {
                node_set.erase(dst_id);
            }
        } else {
            node_set.insert(src_id);
            node_set.insert(dst_id);
            topo[src_id][dst_id] = cost;
            topo[dst_id][src_id] = cost;
        }
        InitForwardTable(node_set);
        InitMessageData(message_file);

    }

}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    string topo_file(argv[1]);
    string message_file(argv[2]);
    string changes_file(argv[3]);

    InitPathWeightData(topo_file);
    InitForwardTable(node_set);
    //CalculateForwardTableDijkstra();

    InitMessageData(message_file);
    //SendMessage();

    UpdatePathWeightData(changes_file, message_file);
}
