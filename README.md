
# NU Information Exchange System

A multi-campus communication system for FAST-NUCES implementing client-server architecture with hybrid TCP/UDP protocols.

## 📋 Features

- **Central Server**: Islamabad campus as hub
- **Campus Clients**: Lahore, Karachi, Peshawar, CFD, Multan
- **Authentication**: Secure campus credential validation
- **Message Routing**: Campus-to-campus communication
- **Broadcast System**: Server announcements to all campuses
- **Heartbeat Monitoring**: 60-second status updates
- **Admin Console**: Real-time system monitoring

text

## 🚀 Quick Start

### Prerequisites
- C++11 or higher
- POSIX-compliant system (Linux/Mac/WSL)

### Compilation
```bash
# Server
g++  -pthread server.cpp -o server

# Client
g++  -pthread client.cpp -o client
Execution
Start Server (Terminal 1):

bash
./server
Run Campus Clients (Separate Terminals):

bash
./client Lahore Admissions NU-LHR-123
./client Karachi Academics NU-KHI-123
./client Peshawar IT NU-PEW-123
./client CFD Sports NU-CFD-123
./client Multan Admissions NU-MLT-123
🎮 Usage
Campus Client Menu
text
1. Send message to another campus
2. Show connection status
3. Exit client
Server Admin Commands
text
list                    - Show connected campuses
broadcast:<message>     - Send message to all campuses
quit                    - Stop server
help                    - Show available commands
🔧 Technical Details
Protocol Design
TCP (54000): Reliable message delivery and authentication

UDP (54001): Efficient heartbeats and broadcasts

Concurrency Model
Multi-threading: Each client handled by separate thread

Mutex Protection: Thread-safe data structures

Atomic Operations: Safe shutdown signaling

Message Formats
text
Authentication: Campus:<Name>,Pass:<Pass>,Dept:<Dept>
Message Send: SEND:<TargetCampus>:<TargetDept>:<Message>
Heartbeat: HEARTBEAT:<CampusName>
📊 Testing
Test Cases
✅ Multi-campus connection establishment

✅ Inter-campus message routing

✅ Server broadcast functionality

✅ Graceful shutdown procedures

✅ Error handling for invalid inputs

Credentials
Campus	Department	Password
Lahore	Admissions	NU-LHR-123
Karachi	Academics	NU-KHI-123
Peshawar	IT	NU-PEW-123
CFD	Sports	NU-CFD-123
Multan	Admissions	NU-MLT-123
🛠️ Development
Project Structure
text
.
├── server.cpp          # Central server implementation
├── client.cpp          # Universal campus client
├── README.md           # This documentation
└── Technical_Report.pdf # Detailed project report
Key Functions
handle_campus_client(): Manage individual campus connections

route_campus_message(): Route messages between campuses

send_heartbeat(): Periodic UDP status updates

admin_console(): Server management interface

📈 Performance
Connection establishment: < 2 seconds

Message delivery latency: < 1 second

Supports 5+ simultaneous campus connections

Efficient memory usage with multiple threads

🤝 Contributing
This project was developed as part of Computer Networks course at FAST-NUCES.

📄 License
Academic Project - FAST-NUCES Fall 2025

👥 Group Members
Haider Abbas - 23F-0632

Jahanzaib Ahmed Khan - 23F-0549

Muhammad Hamza - 23P-0577

Project developed for Computer Networks course, FAST-NUCES CFD
