#include<bits/stdc++.h>
using namespace std;

const int WINDOW_SIZE = 3;         // The Sender and Receiver Window Size (N)
const int MAX_SEQ = 2 * WINDOW_SIZE - 1; // Sequence Space is 0..5 (2N - 1)

// Represents the Data Packet from the Network Layer
struct Packet {
    int id;            
    string content;
};

// Represents the Frame at the Physical Layer
struct Frame {
    int seq;            // Protocol Sequence Number (0..MAX_SEQ)
    int ack;            // Acknowledgement Number (Not strictly used in this SR sim, but included)
    Packet info;        // Payload
    bool is_corrupted;  // Simulation flag
};

queue<Frame> channel_sender_to_receiver;
queue<int> channel_receiver_to_sender;      

int next_frame_to_send = 0; // Upper edge of sender window + 1
int ack_expected = 0;       // Lower edge of sender window
int nbuffered = 0;          // Number of frames currently in flight (<= WINDOW_SIZE)
Packet sender_buffer[MAX_SEQ + 1]; // Buffer to store packets for retransmission


bool timer_running[MAX_SEQ + 1] = { false };
int timer_value[MAX_SEQ + 1] = { 0 }; // Timer counter for each frame

int frame_expected = 0;     // Lower edge of receiver window
int next_expected_ack = 0;  // Upper edge of receiver window + 1

Frame receiver_buffer[MAX_SEQ + 1]; // Buffer to store out-of-order frames
bool buffer_filled[MAX_SEQ + 1] = { false }; // Tracks which buffer slots are filled

int global_packet_id_counter = 1;
int total_packets_target = 0;
int total_packets_delivered = 0;
set<int> packets_to_corrupt; // User defined IDs to fail
const int TIMEOUT_THRESHOLD = 4; // Cycles of inactivity before timeout

bool between(int a, int b, int c) {
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a));
}

void print_log(string log_role, string message) { //log printing formatter
    cout << left << setw(12) << ("[" + log_role + "]   ") << message << endl;
}

// handles Transmition Function 
void send_data(int frame_nr, int frame_exp, Packet p, bool is_retransmission) {
    Frame s;
    s.seq = frame_nr;
    s.ack = (frame_exp + MAX_SEQ) % (MAX_SEQ + 1); 
    s.info = p;
    s.is_corrupted = false;
    if (packets_to_corrupt.count(p.id)) {
        s.is_corrupted = true;
        packets_to_corrupt.erase(p.id);
        string type = is_retransmission ? "Resending" : "Sending";
        print_log("WIRE", ">>> CORRUPTING " + type + " Packet #" + to_string(p.id) + " (Seq " + to_string(frame_nr) + ") <<<");
    }
    else {
        string type = is_retransmission ? "Resending" : "Sending";
        print_log("SENDER", type + " Packet #" + to_string(p.id) + " (Seq " + to_string(frame_nr) + ")");
    }
    timer_running[frame_nr] = true;
    timer_value[frame_nr] = 0; 

    channel_sender_to_receiver.push(s);
}
void check_timers() {
    for (int i = 0; i <= MAX_SEQ; i++) {
        if (timer_running[i]) {
            timer_value[i]++;
            if (timer_value[i] >= TIMEOUT_THRESHOLD) {
                print_log("TIMER", "!!! TIMEOUT DETECTED for Seq " + to_string(i) + " !!!");
                print_log("SENDER", "Selective Retransmission: Retransmitting frame " + to_string(i) + "...");
                send_data(i, frame_expected, sender_buffer[i], true); 
            }
        }
    }
}

// handles Ack Arrival Event 
void event_ack_arrival(int ack_val) {
    int upper_edge = (next_frame_to_send + MAX_SEQ + 1) % (MAX_SEQ + 1);
    if (between(ack_expected, ack_val, upper_edge)) {
        print_log("SENDER", "Received valid SACK for Seq " + to_string(ack_val) + ".");
        if (timer_running[ack_val]) {
            timer_running[ack_val] = false;
        }
        if (ack_val == ack_expected) {
            while (nbuffered > 0 && !timer_running[ack_expected]) {
                print_log("SENDER", "Window slides past Seq " + to_string(ack_expected) + ".");
                nbuffered--;
                ack_expected = (ack_expected + 1) % (MAX_SEQ + 1);
            }
        }
    }
}

// handles Frame Arrival Event
void event_frame_arrival() {
    if (channel_sender_to_receiver.empty()) return;
    Frame r = channel_sender_to_receiver.front();
    channel_sender_to_receiver.pop();
    if (r.is_corrupted) {
        print_log("RECEIVER", "Frame " + to_string(r.seq) + " damaged. DISCARDING (Silence).");
        return;
    }
    next_expected_ack = (frame_expected + WINDOW_SIZE) % (MAX_SEQ + 1);
    if (between(frame_expected, r.seq, next_expected_ack)) {
        print_log("RECEIVER", "Sending SACK " + to_string(r.seq));
        channel_receiver_to_sender.push(r.seq);
        if (!buffer_filled[r.seq]) {
            receiver_buffer[r.seq] = r;
            buffer_filled[r.seq] = true;
        }
        while (buffer_filled[frame_expected]) {
            Frame delivered_frame = receiver_buffer[frame_expected];
            print_log("RECEIVER", "Frame " + to_string(frame_expected) + " Delivered. (Packet #" + to_string(delivered_frame.info.id) + ")");
            total_packets_delivered++;
            buffer_filled[frame_expected] = false;
            frame_expected = (frame_expected + 1) % (MAX_SEQ + 1);
        }

    }
    else {
        print_log("RECEIVER", "Frame " + to_string(r.seq) + " Outside Window (Exp " + to_string(frame_expected) + "). DISCARDING.");
    }
}
int main() {
    cout << "=================================================\n";
    cout << "  SELECTIVE REPEAT PROTOCOL SIMULATION (SR)      \n";
    cout << "=================================================\n";
    cout << "Enter total number of frames to send: ";
    cin >> total_packets_target;

    int num_bad;
    cout << "Enter number of frames to corrupt: ";
    cin >> num_bad;

    if (num_bad > 0) {
        for (int i = 0; i < num_bad; i++) {
            cout << "Enter a Packet ID to corrupt between 1 and " << total_packets_target << " :";
            int id;
            cin >> id;
            packets_to_corrupt.insert(id);
        }
    }

    cout << "\n[SIM] Simulation Started. Window Size: " << WINDOW_SIZE << ", Seq Space: 0-" << MAX_SEQ << "\n\n";

    while (total_packets_delivered < total_packets_target) {
        bool activity = false;
        if (nbuffered < WINDOW_SIZE && global_packet_id_counter <= total_packets_target) {
            Packet p;
            p.id = global_packet_id_counter;
            p.content = "Data";
            sender_buffer[next_frame_to_send] = p;
            nbuffered++;
            send_data(next_frame_to_send, frame_expected, p, false);
            next_frame_to_send = (next_frame_to_send + 1) % (MAX_SEQ + 1);
            global_packet_id_counter++;
            activity = true;
        }
        if (!channel_sender_to_receiver.empty()) {
            event_frame_arrival();
            activity = true;
        }
        while (!channel_receiver_to_sender.empty()) {
            int ack = channel_receiver_to_sender.front();
            channel_receiver_to_sender.pop();
            event_ack_arrival(ack);
            activity = true;
        }
        check_timers();
        this_thread::sleep_for(chrono::milliseconds(200));
    }

    cout << "\n=================================================\n";
    cout << "   TRANSMISSION COMPLETE. ALL PACKETS DELIVERED.  \n";
    cout << "=================================================\n";
    return 0;
}
