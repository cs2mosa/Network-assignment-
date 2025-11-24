#include<bits/stdc++.h>
using namespace std;

const int MAX_SEQ = 7; // The Sequence Number space is 0..3

// Represents the Data Packet from the Network Layer
struct Packet {
    int id;          
    string content;
};

// Represents the Frame at the Physical Layer
struct Frame {
    int seq;            // Sequence Number (0..3)
    int ack;            // Ack Number
    Packet info;        // Payload
    bool is_corrupted;  // flag
};

queue<Frame> channel_sender_to_receiver;   
queue<int> channel_receiver_to_sender;    

int next_frame_to_send = 0; // Upper edge of sender window
int ack_expected = 0;       // Lower edge of sender window
int nbuffered = 0;          // Number of frames currently in flight (usually <= Max_Seq)
Packet sender_buffer[MAX_SEQ + 1]; // Buffer to store packets for retransmission

int frame_expected = 0;    

int global_packet_id_counter = 1;
int total_packets_target = 0;
int total_packets_delivered = 0;
set<int> packets_to_corrupt; 

bool between(int a, int b, int c) {
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a));
}

void print_log(string log_role, string message) { 
    cout << left << setw(12) << ("[" + log_role + "]   ") << message << endl;
}

// handles sending data to the receiver through physical layer
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
    } else {
        string type = is_retransmission ? "Resending" : "Sending";
        print_log("SENDER", type + " Packet #" + to_string(p.id) + " (Seq " + to_string(frame_nr) + ")");
    }

    channel_sender_to_receiver.push(s);
}

// handles the time out 
void event_timeout() {
    print_log("TIMER", "!!! TIMEOUT DETECTED for Seq " + to_string(ack_expected) + " !!!");
    print_log("SENDER", "Go-Back-N Triggered: Retransmitting window...");
    int next = ack_expected;
    for (int i = 1; i <= nbuffered; i++) {
        Packet p = sender_buffer[next];
        send_data(next, frame_expected, p, true); // true = is_retransmission
        next = (next + 1) % (MAX_SEQ + 1);
    }
}
// handles when an ACK arrives from the receiver.
void event_ack_arrival(int ack_val) {
    while (between(ack_expected, ack_val, next_frame_to_send)) {
        print_log("SENDER", "Received valid ACK for Seq " + to_string(ack_expected) + ". Window slides.");
        nbuffered--;
        ack_expected = (ack_expected + 1) % (MAX_SEQ + 1);
    }
}

// handles when a frame arrives from the wire.
void event_frame_arrival() {
    if (channel_sender_to_receiver.empty()) return;
    Frame r = channel_sender_to_receiver.front();
    channel_sender_to_receiver.pop();
    if (r.is_corrupted) {
        print_log("RECEIVER", "Frame " + to_string(r.seq) + " damaged. DISCARDING (Silence).");
        return; 
    }
    if (r.seq == frame_expected) {
        print_log("RECEIVER", "Frame " + to_string(r.seq) + " Accepted. (Packet #" + to_string(r.info.id) + ")");
        total_packets_delivered++;
        frame_expected = (frame_expected + 1) % (MAX_SEQ + 1);
        print_log("RECEIVER", "Sending ACK " + to_string(r.seq));
        channel_receiver_to_sender.push(r.seq);
        
    } else {
        print_log("RECEIVER", "Frame " + to_string(r.seq) + " Unexpected (Expected " + to_string(frame_expected) + "). DISCARDING.");
    }
}

int main() {
    cout << "=================================================\n";
    cout << "   GO-BACK-N PROTOCOL SIMULATION (Protocol 5)    \n";
    cout << "=================================================\n";
    cout << "Enter total number of frames to send: ";
    cin >> total_packets_target;
    
    int num_bad;
    cout << "Enter number of frames to corrupt: ";
    cin >> num_bad;
    
    if (num_bad > 0) {
        for(int i=0; i<num_bad; i++) {
            cout << "Enter a Packet ID to corrupt between 1 and " << total_packets_target << " :";
            int id; 
            cin >> id;
            packets_to_corrupt.insert(id);
        }
    }

    cout << "\n[SIM] Simulation Started. Window Size: " << MAX_SEQ << "\n\n";
    int idle_ticks = 0; 
    const int TIMEOUT_THRESHOLD = 4; 

    while (total_packets_delivered < total_packets_target) {
        bool activity = false;
        if (nbuffered < MAX_SEQ && global_packet_id_counter <= total_packets_target) {
            Packet p;
            p.id = global_packet_id_counter;
            p.content = "Data";
            sender_buffer[next_frame_to_send] = p;
            nbuffered++;
            send_data(next_frame_to_send, frame_expected, p, false); // false = new frame
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
        if (activity) {
            idle_ticks = 0; // Reset timer if data moved
        } else {
            if (nbuffered > 0) {
                idle_ticks++;
                if (idle_ticks >= TIMEOUT_THRESHOLD) {
                    event_timeout();
                    idle_ticks = 0; // Reset after triggering
                }
            }
        }
        //simulating the time it takes for frames to travel through the physical layer
        this_thread::sleep_for(chrono::milliseconds(200));
    }
    cout << "\n=================================================\n";
    cout << "   TRANSMISSION COMPLETE. ALL PACKETS DELIVERED.  \n";
    cout << "=================================================\n";
    return 0;
}
