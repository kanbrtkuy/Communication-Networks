#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

int num_nodes;
int pckt_size;
vector<int> ranges;
int max_attempt;
int simu_time;
int channel_occu_time = 0;

void parseLine(const string &line) {
    string input_type;
    istringstream ss(line);
    ss >> input_type;
    string s;

    if (input_type == "R") {
        while (ss >> s) {
            ranges.push_back(stoi(s));
        }
    }

    ss >> s;
    if (input_type == "N")
        num_nodes = stoi(s);
    else if (input_type == "L")
        pckt_size = stoi(s);
    else if (input_type == "M")
        max_attempt = stoi(s);
    else if (input_type == "T")
        simu_time = stoi(s);
}


void readInput(const string& filename) {
    ifstream in;
    string line;
    in.open(filename);

    if (in.is_open()) {
        for (int i = 0; i < 5; i++) {
            getline(in, line);
            parseLine(line);
        }
        in.close();
    }
}


int get_back_off(int node_id, int ticks, int R) {
    return (node_id + ticks) % R;
}

void run_simulation() {
    bool channelOccupied = false;
    vector<int> back_offs;
    vector<int> collisions(num_nodes, 0);
    int remaining_time = 0;
    int node_id;

    // initialize back_offs
    for (int i = 0; i < num_nodes; i++) {
        back_offs.push_back(get_back_off(i, 0, ranges[0]));
    }

    for (int t = 0; t < simu_time; t++) {
        if (channelOccupied) {
            if (remaining_time > 0) {
                remaining_time--;
                channel_occu_time++;
                continue;
            }
            channelOccupied = false;
            collisions[node_id] = 0;
            back_offs[node_id] = get_back_off(node_id, t, ranges[0]);
        }

        vector<int> candidates;
        for (int idx = 0; idx < num_nodes; idx++) {
            if (back_offs[idx] == 0)
                candidates.push_back(idx);
        }
        //  decrease back_offs
        if (candidates.empty()) {
            for (int &e: back_offs) {
                e--;
            }
            continue;
        }

        // start transmission
        if (candidates.size() == 1) {
            channelOccupied = true;
            remaining_time = pckt_size - 1;
            channel_occu_time++;
            node_id = candidates[0];
            continue;
        }

        // collision
        if (candidates.size() > 1) {
            for (int idx: candidates) {
                collisions[idx]++;
                if (collisions[idx] == max_attempt)
                    collisions[idx] = 0;
                back_offs[idx] = get_back_off(idx, t + 1, ranges[collisions[idx]]);
            }
        }
    }
}

void output_stats(const string& filename) {
    FILE *fpout;

    fpout = fopen(filename.c_str(), "a");

    //original
    fprintf(fpout, "%.2lf\n", 1 * (1.0 * channel_occu_time) / (1.0 * simu_time));
    fprintf(fpout, "\n");
    fclose(fpout);
}

int main(int argc, char **argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fclose(fpOut);

    readInput(argv[1]);

    channel_occu_time = 0;

    run_simulation();
    output_stats("output.txt");

    return 0;
}