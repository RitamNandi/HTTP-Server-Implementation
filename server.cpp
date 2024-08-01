#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <sys/types.h>
#include <thread>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

std::string fileDirectory;


void handleConnections(int connection) {

  // Need to use recv to read data from the socket
  char buffer[1024] = {0};
  auto numBytes = recv(connection, buffer, sizeof(buffer), 0); // recv returns the length of the message in bytes
  buffer[numBytes] = '\0';
  std::string request(buffer);
  std::string method, url, http_version, line;
  std::string userAgent;

// How a request might look like: 
// GET
// /user-agent
// HTTP/1.1
// \r\n

// // Headers
// Host: localhost:4221\r\n
// User-Agent: foobar/1.2.3\r\n  // This is the user agent I want to return
// Accept: */*\r\n
// \r\n

  std::istringstream request_stream(request);
  request_stream >> method >> url >> http_version; // splits up the 3 space separated tokens and feeds it into the 3 variables
  std::getline(request_stream, line); // Picking up everything in the request after the http version and feeding it into line

   while (std::getline(request_stream, line) && line != "\r") {
        std::size_t pos = line.find(": ");
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 2);
            value.erase(value.find_last_not_of("\r\n") + 1); // Trim

            if (key == "User-Agent") {
                userAgent = value;
                break; // no need to keep searching
            }
        }
    }

  std::string response;
  std::stringstream responseStream;

  if (url == "/") {
    responseStream << "HTTP/1.1 200 OK\r\n\r\n";
  } else if (url.substr(0, 6) == "/echo/") { // is this how I should manually implement an "echo" endpoint? It should technically work...
    // we want Content-Type and Content-Length headers
    // I'll hope that content-type is sticking to strings
    // Content-length we can grab the remnants of the string to get the actual message
    std::string echoStr = url.substr(6, url.length() - 6);

    responseStream << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " << std::to_string(echoStr.length()) << "\r\n\r\n" << echoStr;

  } else if (url == "/user-agent") { // user-agent endpoint (get request)

    // read the user agent and return in response body
    // curl -v --header "User-Agent: foobar/1.2.3" http://localhost:4221/user-agent
    // HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nfoobar/1.2.3

    responseStream << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " << std::to_string(userAgent.length()) << "\r\n\r\n" << userAgent;


  } else if (url.substr(0, 7) == "/files/") { // get request, not able to do post request for now, will look into later

    std::string requestedFileDir = url.substr(7, url.length() - 7);
    std::ifstream fileStream(fileDirectory + requestedFileDir);

    std::stringstream fileContents;

    if (fileStream.good()) {
      fileContents << fileStream.rdbuf();
    } else {
      responseStream << "HTTP/1.1 404 Not Found\r\n\r\n";
    }

    // send over the file contents and a response
    // example response: HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 14\r\n\r\nHello, World!
    responseStream << "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " << std::to_string(fileContents.str().length()) << "\r\n\r\n" << fileContents.str();

  } else {
    responseStream << "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  response = responseStream.str();
   
  send(connection, response.c_str(), response.size(), 0);
  
  close(connection);
  
}

int main(int argc, char **argv) { 
  
  std::cout << "Logs:\n";

  int server_fd = socket(AF_INET, SOCK_STREAM, 0); // socket: endpoint for sending and receiving data across a network

  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }

  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";

    // Argc and argv will have command line information, such as additional command passed in when running

  // std::string fileDirectory; // handling returning files based on file endpoint, need to collect the directory from the command line (maybe I need this as global var?)


  if (argc == 3 && strcmp(argv[1], "--directory") == 0) {
    fileDirectory = argv[2];
  }



  while (true) {
      int connection = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
      if (connection < 0) {
        std::cerr << "Failed to accept client connection...\n";
        continue;
      }

      std::cout << "Client connected\n";

      // use threading to handle concurrent client connections
      std::thread clientThread(handleConnections, connection);
      clientThread.detach(); // detach thread, execute independently of the thread that created it

  }
  


  close(server_fd);
  return 0;
}