// client.cpp This is Universal Campus Client for ALL campuses
// Usage: ./client <CampusName> <Department> <Password>
// Example: ./client Lahore Admissions NU-LHR-123

// Network communication headers
#include <arpa/inet.h>      // IP address conversion functions
#include <netinet/in.h>     // Internet address structures
#include <sys/socket.h>     // Socket creation and operations
#include <sys/select.h>     // Multiplexing I/O operations
#include <unistd.h>         // POSIX API (close, read, write)
// Standard C++ headers  
#include <iostream>         // Console input/output
#include <string>           // String manipulation
#include <thread>           // Multi-threading support
#include <atomic>           // Thread-safe atomic operations
#include <cstring>          // C string functions
#include <signal.h>         // Signal handling (Ctrl+C)
#include <chrono>           // Time utilities

using namespace std;

const int TCP_PORT = 54000;
const int UDP_PORT = 54001;
const int BUFFER_SIZE = 8192;

atomic<bool> client_running{true};
int tcp_socket = -1, udp_socket = -1;
string campus_name, department, password, server_ip = "127.0.0.1";

void signal_handler(int sig) {
    cout << "\nSignal received. Exiting campus client...\n";
    client_running = false;
    if(tcp_socket != -1) shutdown(tcp_socket, SHUT_RDWR);
}

void display_menu() {
    cout << "\n========================================\n";
    cout << campus_name << " Campus - " << department << " Department\n";
    cout << "========================================\n";
    cout << "1. Send message to another campus\n";
    cout << "2. Show connection status\n";
    cout << "3. Exit client\n";
    cout << "========================================\n";
    cout << "Choice: ";
}

void receive_tcp_messages() {
    char buffer[BUFFER_SIZE];
    
    while(client_running) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(tcp_socket, &read_set);
        
        timeval timeout = {1, 0};
        int ready = select(tcp_socket + 1, &read_set, NULL, NULL, &timeout);
        
        if(ready > 0 && FD_ISSET(tcp_socket, &read_set)) {
            ssize_t bytes = recv(tcp_socket, buffer, BUFFER_SIZE - 1, 0);
            if(bytes > 0) {
                buffer[bytes] = '\0';
                cout << "\n========================================\n";
                cout << "NEW MESSAGE RECEIVED\n";
                cout << "========================================\n";
                
                string msg(buffer);
                if(msg.find("FROM:") == 0) {
                    size_t colon1 = msg.find(':', 5);
                    size_t colon2 = msg.find(':', colon1 + 1);
                    size_t colon3 = msg.find(':', colon2 + 1);
                    
                    if(colon1 != string::npos && colon2 != string::npos && colon3 != string::npos) {
                        string from_campus = msg.substr(5, colon1 - 5);
                        string from_dept = msg.substr(colon1 + 1, colon2 - colon1 - 1);
                        string message = msg.substr(colon3 + 1);
                        
                        cout << "From: " << from_campus << " (" << from_dept << ")\n";
                        cout << "Message: " << message << "\n";
                    } else {
                        cout << msg << "\n";
                    }
                } else {
                    cout << msg << "\n";
                }
                
                cout << "========================================\n";
                cout << "Choice: ";
                cout.flush();
            }
            else if(bytes == 0) {
                cout << "\nServer connection lost\n";
                client_running = false;
                break;
            }
        }
    }
}

void receive_udp_broadcasts() {
    char buffer[BUFFER_SIZE];
    sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    while(client_running) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(udp_socket, &read_set);
        
        timeval timeout = {1, 0};
        int ready = select(udp_socket + 1, &read_set, NULL, NULL, &timeout);
        
        if(ready > 0 && FD_ISSET(udp_socket, &read_set)) {
            ssize_t bytes = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                    (sockaddr*)&src_addr, &addr_len);
            if(bytes > 0) {
                buffer[bytes] = '\0';
                cout << "\n========================================\n";
                cout << "SERVER BROADCAST\n";
                cout << "========================================\n";
                cout << buffer << "\n";
                cout << "========================================\n";
                cout << "Choice: ";
                cout.flush();
            }
        }
    }
}

void send_heartbeat(sockaddr_in server_udp_addr) {
    while(client_running) {
        string heartbeat = "HEARTBEAT:" + campus_name;
        sendto(udp_socket, heartbeat.c_str(), heartbeat.size(), 0,
              (sockaddr*)&server_udp_addr, sizeof(server_udp_addr));
        
        // Sleep for 10 seconds with interrupt checks
        for(int i = 0; i < 60 && client_running; i++) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
}

void cleanup() {
    client_running = false;
    
    if(tcp_socket != -1) {
        shutdown(tcp_socket, SHUT_RDWR);
        close(tcp_socket);
        tcp_socket = -1;
    }
    if(udp_socket != -1) {
        close(udp_socket);
        udp_socket = -1;
    }
}

int main(int argc, char* argv[]) {
    // Handle Ctrl+C
    signal(SIGINT, signal_handler);
    
    // Get credentials from command line
    if(argc >= 4) {
        campus_name = argv[1];
        department = argv[2];
        password = argv[3];
        if(argc >= 5) server_ip = argv[4];
    } else {
        cout << "Usage: " << argv[0] << " <Campus> <Department> <Password> [ServerIP]\n";
        cout << "Example: " << argv[0] << " Karachi Academics NU-KHI-123\n";
        cout << "\nAvailable Campuses:\n";
        cout << "  Lahore   Admissions   NU-LHR-123\n";
        cout << "  Karachi  Academics    NU-KHI-123\n";
        cout << "  Peshawar IT           NU-PEW-123\n";
        cout << "  CFD      Sports       NU-CFD-123\n";
        cout << "  Multan   Admissions   NU-MLT-123\n";
        return 1;
    }
    
    cout << "========================================\n";
    cout << "NU Information Exchange System - Client\n";
    cout << "Campus: " << campus_name << "\n";
    cout << "Department: " << department << "\n";
    cout << "Server: " << server_ip << "\n";
    cout << "========================================\n";
    
    // TCP Connection
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_socket < 0) {
        perror("TCP socket creation failed");
        return 1;
    }
    
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
    
    cout << "Connecting to central server...\n";
    if(connect(tcp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        cleanup();
        return 1;
    }
    
    // Authentication
    string auth_data = "Campus:" + campus_name + ",Pass:" + password + ",Dept:" + department + "\n";
    if(send(tcp_socket, auth_data.c_str(), auth_data.size(), 0) < 0) {
        perror("Authentication failed");
        cleanup();
        return 1;
    }
    
    char response[BUFFER_SIZE];
    ssize_t bytes = recv(tcp_socket, response, BUFFER_SIZE - 1, 0);
    if(bytes > 0) {
        response[bytes] = '\0';
        cout << "Server: " << response << "\n";
        
        if(string(response).find("AUTH_FAIL") != string::npos) {
            cout << "Authentication rejected\n";
            cleanup();
            return 1;
        }
    }
    
    // UDP Setup
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(udp_socket < 0) {
        perror("UDP socket creation failed");
        cleanup();
        return 1;
    }
    
    sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = 0;
    
    if(bind(udp_socket, (sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("UDP bind failed");
        cleanup();
        return 1;
    }
    
    sockaddr_in udp_server_addr = server_addr;
    udp_server_addr.sin_port = htons(UDP_PORT);
    
    // Start background threads
    thread tcp_receiver(receive_tcp_messages);
    thread udp_receiver(receive_udp_broadcasts);
    thread heartbeat_thread(send_heartbeat, udp_server_addr);
    
    cout << "Connected to central server successfully!\n";
    cout << "Heartbeat service started (60 second intervals)\n";
    
    // Main interaction loop
    while(client_running) {
        display_menu();
        
        string choice;
        if(!getline(cin, choice)) {
            client_running = false;
            break;
        }
        
        if(choice == "1") {
            cout << "\n--- Send Message ---\n";
            cout << "Target Campus (Lahore/Karachi/Peshawar/CFD/Multan): ";
            string target_campus;
            getline(cin, target_campus);
            
            if(target_campus.empty()) {
                cout << "Campus name cannot be empty\n";
                continue;
            }
            
            cout << "Target Department: ";
            string target_dept;
            getline(cin, target_dept);
            
            if(target_dept.empty()) {
                cout << "Department cannot be empty\n";
                continue;
            }
            
            cout << "Message: ";
            string message;
            getline(cin, message);
            
            if(message.empty()) {
                cout << "Message cannot be empty\n";
                continue;
            }
            
            string packet = "SEND:" + target_campus + ":" + target_dept + ":" + message + "\n";
            if(send(tcp_socket, packet.c_str(), packet.size(), 0) > 0) {
                cout << "Message sent to " << target_campus << " (" << target_dept << ")\n";
            } else {
                cout << "Failed to send message\n";
            }
        }
        else if(choice == "2") {
            cout << "\n--- Connection Status ---\n";
            cout << "Campus: " << campus_name << "\n";
            cout << "Department: " << department << "\n";
            cout << "TCP Connection: Active\n";
            cout << "UDP Heartbeat: Active\n";
            cout << "Server: " << server_ip << "\n";
            cout << "Status: Connected\n";
        }
        else if(choice == "3") {
            cout << "Disconnecting from server...\n";
            client_running = false;
            break;
        }
        else if(choice == "help") {
            cout << "\n--- Help Menu ---\n";
            cout << "1. Send Message - Send to another campus\n";
            cout << "2. Status - Show connection information\n";
            cout << "3. Exit - Close this client\n";
            cout << "\nMessage Format:\n";
            cout << "   Target Campus: Lahore, Karachi, etc\n";
            cout << "   Target Dept: Admissions, Academics, etc\n";
            cout << "   Message: Any text message\n";
        }
        else {
            cout << "Invalid choice. Type 'help' for options.\n";
        }
    }
    
    // Clean shutdown
    cleanup();
    
    if(tcp_receiver.joinable()) tcp_receiver.join();
    if(udp_receiver.joinable()) udp_receiver.join();
    if(heartbeat_thread.joinable()) heartbeat_thread.join();
    
    cout << campus_name << " campus client stopped.\n";
    return 0;
}