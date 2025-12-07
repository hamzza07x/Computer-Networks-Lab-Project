// server.cpp - Central Server (Islamabad Campus)
// CN Project Fall 2025 - NU Information Exchange System
// TCP:54000 (Authentication & Messages), UDP:54001 (Heartbeats & Broadcast)
// Network and system headers
#include <arpa/inet.h>      // IP address conversion
#include <netinet/in.h>     // Internet address structs
#include <sys/socket.h>     // Socket operations
#include <sys/types.h>      // Data types for sockets
#include <sys/select.h>     // I/O multiplexing
#include <unistd.h>         // POSIX API functions
// C++ standard library headers
#include <algorithm>        // STL algorithms (remove, find)
#include <atomic>           // Thread-safe atomic variables
#include <cerrno>           // Error number definitions
#include <cstring>          // C string manipulation
#include <ctime>            // Time functions
#include <iostream>         // Input/output streams
#include <map>              // Key-value map container
#include <mutex>            // Mutual exclusion locks
#include <sstream>          // String stream operations
#include <string>           // String class
#include <thread>           // Thread management
#include <vector>           // Dynamic array container

#define TCP_PORT 54000
#define UDP_PORT 54001
#define BUFFER_SIZE 8192

using namespace std;

struct ClientInfo {
    int tcp_sock = -1;
    string campus;
    string department;
    sockaddr_in udp_addr{};
    bool has_udp = false;
    string last_seen;
};

map<string, ClientInfo> connected_clients;
map<int, string> socket_to_campus;
mutex clients_mutex;
atomic<bool> server_running{true};

// Hard-coded credentials
map<string, string> campus_credentials = {
    {"Lahore", "NU-LHR-123"},
    {"Karachi", "NU-KHI-123"},
    {"Peshawar", "NU-PEW-123"},
    {"CFD", "NU-CFD-123"},
    {"Multan", "NU-MLT-123"}
};

string get_current_time() {
    time_t now = time(nullptr);
    char time_buf[64];
    ctime_r(&now, time_buf);
    string time_str(time_buf);
    if(!time_str.empty() && time_str.back() == '\n')
        time_str.pop_back();
    return time_str;
}

void server_log(const string &message) {
    cout << "[" << get_current_time() << "] " << message << endl;
}

bool send_tcp_message(int socket, const string &message) {
    string formatted_msg = message + "\n";
    return send(socket, formatted_msg.c_str(), formatted_msg.size(), 0) > 0;
}

void route_campus_message(const string &source_campus, const string &source_dept,
                         const string &target_campus, const string &message_text) {
    lock_guard<mutex> lock(clients_mutex);
    
    auto target_client = connected_clients.find(target_campus);
    if(target_client == connected_clients.end()) {
        server_log("Routing failed: Campus '" + target_campus + "' is not connected.");
        return;
    }
    
    int target_socket = target_client->second.tcp_sock;
    string formatted_message = "FROM:" + source_campus + ":" + source_dept + ":" + message_text;
    
    if(send_tcp_message(target_socket, formatted_message)) {
        server_log("Message routed from " + source_campus + " to " + target_campus);
    } else {
        server_log("Failed to send message to " + target_campus);
    }
}

void handle_campus_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if(bytes_received <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_received] = '\0';
    string auth_data(buffer);
    
    // Parse authentication
    string campus_name, password, department;
    
    istringstream auth_stream(auth_data);
    string token;
    while(getline(auth_stream, token, ',')) {
        size_t colon_pos = token.find(':');
        if(colon_pos == string::npos) continue;
        
        string key = token.substr(0, colon_pos);
        string value = token.substr(colon_pos + 1);
        
        if(key == "Campus") campus_name = value;
        else if(key == "Pass") password = value;
        else if(key == "Dept") department = value;
    }
    
    // Validate
    if(campus_name.empty() || password.empty()) {
        send_tcp_message(client_socket, "AUTH_FAIL:Missing credentials");
        close(client_socket);
        return;
    }
    
    auto valid_cred = campus_credentials.find(campus_name);
    if(valid_cred == campus_credentials.end() || valid_cred->second != password) {
        send_tcp_message(client_socket, "AUTH_FAIL:Invalid credentials");
        close(client_socket);
        return;
    }
    
    // Register client
    {
        lock_guard<mutex> lock(clients_mutex);
        ClientInfo client_info;
        client_info.tcp_sock = client_socket;
        client_info.campus = campus_name;
        client_info.department = department.empty() ? "General" : department;
        client_info.last_seen = get_current_time();
        connected_clients[campus_name] = client_info;
        socket_to_campus[client_socket] = campus_name;
    }
    
    server_log("Campus authenticated: " + campus_name + " (Dept: " + 
               (department.empty() ? "General" : department) + ")");
    send_tcp_message(client_socket, "AUTH_OK:" + campus_name);
    
    // Handle client messages
    while(server_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if(activity < 0 && errno != EINTR) {
            break;
        }
        
        if(!server_running) {
            break;
        }
        
        if(activity > 0 && FD_ISSET(client_socket, &read_fds)) {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if(bytes_received <= 0) break;
            
            buffer[bytes_received] = '\0';
            string message(buffer);
            
            // Remove carriage returns and newlines
            message.erase(remove(message.begin(), message.end(), '\r'), message.end());
            message.erase(remove(message.begin(), message.end(), '\n'), message.end());
            
            // Check if it's a SEND message
            if(message.find("SEND:") == 0) {
                string payload = message.substr(5);
                size_t first_colon = payload.find(':');
                size_t second_colon = payload.find(':', first_colon + 1);
                
                if(first_colon != string::npos && second_colon != string::npos) {
                    string target_campus = payload.substr(0, first_colon);
                    string target_dept = payload.substr(first_colon + 1, second_colon - first_colon - 1);
                    string message_text = payload.substr(second_colon + 1);
                    
                    string source_campus, source_dept;
                    {
                        lock_guard<mutex> lock(clients_mutex);
                        auto source_it = socket_to_campus.find(client_socket);
                        if(source_it != socket_to_campus.end()) {
                            source_campus = source_it->second;
                            auto client_it = connected_clients.find(source_campus);
                            if(client_it != connected_clients.end()) {
                                source_dept = client_it->second.department;
                            }
                        }
                    }
                    
                    if(!target_campus.empty()) {
                        route_campus_message(source_campus, source_dept, target_campus, message_text);
                    }
                }
            }
        }
    }
    
    // Cleanup on disconnect
    {
        lock_guard<mutex> lock(clients_mutex);
        auto sock_it = socket_to_campus.find(client_socket);
        if(sock_it != socket_to_campus.end()) {
            string campus = sock_it->second;
            connected_clients.erase(campus);
            socket_to_campus.erase(sock_it);
            server_log("Campus disconnected: " + campus);
        }
    }
    close(client_socket);
}

void tcp_server_loop(int server_socket) {
    while(server_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if(activity < 0 && errno != EINTR) {
            break;
        }
        
        if(!server_running) {
            break;
        }
        
        if(activity > 0 && FD_ISSET(server_socket, &read_fds)) {
            sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);
            
            if(client_socket < 0) {
                if(errno == EINTR) continue;
                perror("accept error");
                break;
            }
            
            thread client_thread(handle_campus_client, client_socket);
            client_thread.detach();
        }
    }
}

void udp_server_loop(int udp_socket) {
    char buffer[BUFFER_SIZE];
    
    while(server_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(udp_socket, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(udp_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if(activity < 0 && errno != EINTR) {
            break;
        }
        
        if(!server_running) {
            break;
        }
        
        if(activity > 0 && FD_ISSET(udp_socket, &read_fds)) {
            sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            ssize_t bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                             (sockaddr*)&client_addr, &addr_len);
            
            if(bytes_received <= 0) continue;
            
            buffer[bytes_received] = '\0';
            string message(buffer);
            
            // Handle heartbeat messages
            if(message.find("HEARTBEAT:") == 0) {
                string campus_name = message.substr(10);
                
                lock_guard<mutex> lock(clients_mutex);
                auto client_it = connected_clients.find(campus_name);
                if(client_it != connected_clients.end()) {
                    client_it->second.udp_addr = client_addr;
                    client_it->second.has_udp = true;
                    client_it->second.last_seen = get_current_time();
                    server_log("Heartbeat from " + campus_name);
                }
            }
        }
    }
}

void admin_console(int udp_socket) {
    string command;
    
    cout << "\n======================================================\n";
    cout << "NU Information Exchange System - Admin Console\n";
    cout << "======================================================\n";
    cout << "Commands: list | broadcast:<message> | quit | help\n";
    cout << "======================================================\n\n";
	cout << "admin>";
    
    while(server_running && getline(cin, command)) {
        if(command == "list") {
            lock_guard<mutex> lock(clients_mutex);
            cout << "\n--- Connected Campuses ---\n";
            if(connected_clients.empty()) {
                cout << "No campuses connected.\n";
            } else {
                for(const auto& client : connected_clients) {
                    cout << "Campus: " << client.first 
                         << " | Department: " << client.second.department
                         << "\nLast seen: " << client.second.last_seen;
                    if(client.second.has_udp) cout << " [UDP Active]";
                    cout << "\n----------------------------------------\n";
                }
            }
            cout << "Total connected: " << connected_clients.size() << "\n";
        }
        else if(command.find("broadcast:") == 0) {
            string broadcast_msg = command.substr(10);
            if(broadcast_msg.empty()) {
                cout << "Error: Broadcast message cannot be empty.\n";
                continue;
            }
            
            lock_guard<mutex> lock(clients_mutex);
            int sent_count = 0;
            for(const auto& client : connected_clients) {
                if(client.second.has_udp) {
                    sendto(udp_socket, broadcast_msg.c_str(), broadcast_msg.size(), 0,
                          (sockaddr*)&client.second.udp_addr, sizeof(client.second.udp_addr));
                    sent_count++;
                }
            }
            server_log("Broadcast sent to " + to_string(sent_count) + " campuses: " + broadcast_msg);
            cout << "Broadcast sent to " << sent_count << " campuses.\n";
        }
        else if(command == "quit") {
            cout << "\nShutting down server...\n";
            
            // Close all client sockets
            {
                lock_guard<mutex> lock(clients_mutex);
                for(auto& client : connected_clients) {
                    if(client.second.tcp_sock != -1) {
                        shutdown(client.second.tcp_sock, SHUT_RDWR);
                        close(client.second.tcp_sock);
                    }
                }
                connected_clients.clear();
                socket_to_campus.clear();
            }
            
            server_running = false;
            server_log("Server shutdown initiated.");
            break;
        }
        else if(command == "help") {
            cout << "\nAvailable commands:\n";
            cout << "  list                    - Show all connected campuses\n";
            cout << "  broadcast:<message>     - Send message to all campuses\n";
            cout << "  quit                    - Stop the server\n";
            cout << "  help                    - Show this help message\n";
        }
        else if(!command.empty()) {
            cout << "Unknown command. Type 'help' for available commands.\n";
        }
        
        if(server_running) cout << "\nadmin> ";
    }
}

void cleanup_sockets(int tcp_socket, int udp_socket) {
    if(tcp_socket != -1) {
        shutdown(tcp_socket, SHUT_RDWR);
        close(tcp_socket);
    }
    if(udp_socket != -1) {
        close(udp_socket);
    }
}

int main() {
    int tcp_socket = -1;
    int udp_socket = -1;
    
    try {
        // Create TCP server socket
        tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if(tcp_socket < 0) {
            perror("TCP socket creation failed");
            return 1;
        }
        
        // Set socket options
        int opt = 1;
        setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Bind TCP socket
        sockaddr_in tcp_addr{};
        tcp_addr.sin_family = AF_INET;
        tcp_addr.sin_addr.s_addr = INADDR_ANY;
        tcp_addr.sin_port = htons(TCP_PORT);
        
        if(bind(tcp_socket, (sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
            perror("TCP bind failed");
            cleanup_sockets(tcp_socket, udp_socket);
            return 1;
        }
        
        // Listen for TCP connections
        if(listen(tcp_socket, 10) < 0) {
            perror("TCP listen failed");
            cleanup_sockets(tcp_socket, udp_socket);
            return 1;
        }
        
        server_log("TCP server listening on port " + to_string(TCP_PORT));
        
        // Create UDP server socket
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if(udp_socket < 0) {
            perror("UDP socket creation failed");
            cleanup_sockets(tcp_socket, udp_socket);
            return 1;
        }
        
        // Bind UDP socket
        sockaddr_in udp_addr{};
        udp_addr.sin_family = AF_INET;
        udp_addr.sin_addr.s_addr = INADDR_ANY;
        udp_addr.sin_port = htons(UDP_PORT);
        
        if(bind(udp_socket, (sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
            perror("UDP bind failed");
            cleanup_sockets(tcp_socket, udp_socket);
            return 1;
        }
        
        server_log("UDP server listening on port " + to_string(UDP_PORT));
        server_log("NU Information Exchange System started successfully!");
        cout << "\nServer is running. Type 'quit' to stop.\n";
        
        // Start server threads
        thread tcp_thread(tcp_server_loop, tcp_socket);
        thread udp_thread(udp_server_loop, udp_socket);
        thread admin_thread(admin_console, udp_socket);
        
        // Wait for threads to finish
        tcp_thread.join();
        udp_thread.join();
        admin_thread.join();
        
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
    
    // Final cleanup
    cleanup_sockets(tcp_socket, udp_socket);
    
    server_log("Server stopped.");
    return 0;
}