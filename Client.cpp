#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include<windows.h>
#include<ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include<unistd.h>

#include "json.hpp"

using namespace std;

using json = nlohmann::json;

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024
// Structure to store packet data
struct Packet {
    std::string symbol;
    char buySellIndicator;
    int quantity;
    int price;
    int sequence;
};

// Function to send a request payload to the server
bool sendRequest(int sock, char callType, char resendSeq) {
    char payload[2];
    payload[0] = callType;
    payload[1] = resendSeq;

    if (send(sock, payload, sizeof(payload), 0) < 0) {
        std::cerr << "Failed to send request\n";
        return false;
    }

    return true;
}

bool receivePackets(int sock, std::vector<Packet>& packets) {
    char buffer[17];
    while (true) {
        int bytesRead = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            break;
        }

        if (bytesRead != sizeof(buffer)) {
            std::cerr << "Incomplete packet received\n";
            return false;
        }

        Packet packet;
        packet.symbol = std::string(buffer, buffer + 4);
        packet.buySellIndicator = buffer[4];
        packet.quantity = ntohl(*reinterpret_cast<int*>(buffer + 5));
        packet.price = ntohl(*reinterpret_cast<int*>(buffer + 9));
        packet.sequence = ntohl(*reinterpret_cast<int*>(buffer + 13));
        packets.push_back(packet);
    }

    return true;
}

// Function to generate the JSON output file
bool generateJSON(const std::string& filename, const std::vector<Packet>& packets) {
    json output;
    for (const auto& packet : packets) {
        json obj;
        obj["symbol"] = packet.symbol;
        obj["buySellIndicator"] = std::string(1, packet.buySellIndicator);
        obj["quantity"] = packet.quantity;
        obj["price"] = packet.price;
        obj["sequence"] = packet.sequence;
        output.push_back(obj);
    }
    for (const auto& packet : packets) {
        
        cout<<packet.symbol<<" "<<std::string(1, packet.buySellIndicator)<<" "<<packet.quantity<<" "<<packet.price<<" "<<packet.sequence<<endl;
        
    }

    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to open file for writing\n";
        return false;
    }

    file << output.dump(4) << std::endl;
    file.close();

    return true;
}

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return 1;
    }

    // Create a socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Server details
    const char* serverIP = "127.0.0.1";
    int serverPort = 3000;

    // Set up the server address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(serverPort);
    


    // Connect to the server
    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Receive and print data from the server
    
    // Send "Stream All Packets" request
    if (!sendRequest(clientSocket, 1, 0)) {
        close(clientSocket);
        return 1;
    }

    // Receive response packets from the server
    std::vector<Packet> packets;
    if (!receivePackets(clientSocket, packets)) {
        close(clientSocket);
        return 1;
    }

    // Handle missing sequences (assuming the last packet is never missed)
    int expectedSequence = 0;
    for (const auto& packet : packets) {
        if (packet.sequence != expectedSequence) {
            // Send "Resend Packet" request for the missing sequence
            if (!sendRequest(clientSocket, 2, expectedSequence)) {
                close(clientSocket);
                return 1;
            }

            // Receive and add the requested packet
            if (!receivePackets(clientSocket, packets)) {
                close(clientSocket);
                return 1;
            }
        }

        expectedSequence++;
    }

    // Generate the JSON output file
    std::string outputFilename = "output.json";  // Replace with your desired output file name
    if (!generateJSON(outputFilename,packets)) {
        close(clientSocket);
        return 1;
    }

    
    // Close the socket
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
