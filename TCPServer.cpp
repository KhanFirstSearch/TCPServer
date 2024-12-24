#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <signal.h>
using namespace std;

// Sends a list of files in the directory to the client
void direc_list(int client_socket, const string &dir_path) {
    DIR *dir = opendir(dir_path.c_str());
    if (!dir) {
        string error_message = "Cannot read directory!!";
        write(client_socket, error_message.c_str(), error_message.size());
        return;
    }

    string result;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        result += entry->d_name;
        result += "\n";
    }
    closedir(dir);

    write(client_socket, result.c_str(), result.size());
}

void client_socket_connection(int client_socket, const string &root_directory) {
    char buffer[1024];
    int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        perror("Failed to read request");
        return;
    }

    buffer[bytes_read] = '\0'; // Null Terminating Char
    string converter(buffer); // Converts buffer into string
    stringstream ss(converter);
    string type, path;

    ss >> type >> path;
    if (type == "INFO") {
        time_t curr_time = time(0); // Get the current time
        string formated_curr_time = ctime(&curr_time); // Convert to string format
        write(client_socket, formated_curr_time.c_str(), formated_curr_time.size());
        return;
    }

    // Check if the type AND path is valid.
    if (type != "GET" || !(!path.empty() && path[0] == '/' && path.find("..") == string::npos)) {
        string errmsg = "Invalid";
        write(client_socket, errmsg.c_str(), errmsg.size());
        return;
    }

    // Create the full path based on the root directory
    string abs_path = root_directory + path;
    struct stat path_stat;

    // Check if the path exists AND is accessible
    if (stat(abs_path.c_str(), &path_stat) != 0) {
        string errmsg = "Path Is Invalid...";
        write(client_socket, errmsg.c_str(), errmsg.size());
        return;
    }

    // Deals with normal files
    if (S_ISREG(path_stat.st_mode)) {
        ifstream file(abs_path);
        char buffer[1024];
        while (file.read(buffer, sizeof(buffer))) {
            write(client_socket, buffer, file.gcount());
        }
        if (file.gcount() > 0) {
            write(client_socket, buffer, file.gcount());
        }
    }
    // Deals with directories
    else if (S_ISDIR(path_stat.st_mode)) {
        string index_file = abs_path + "/index.html";
        if (stat(index_file.c_str(), &path_stat) == 0 && S_ISREG(path_stat.st_mode)) {
            ifstream file(index_file);
            char buffer[1024];
            while (file.read(buffer, sizeof(buffer))) {
                write(client_socket, buffer, file.gcount());
            }
            if (file.gcount() > 0) {
                write(client_socket, buffer, file.gcount());
            }
        } else {
            direc_list(client_socket, abs_path);
        }
    } 
    // If its not a directory or a normal file, send an error
    else {
        string errmsg = "You did not use a normal file or a directory!";
        write(client_socket, errmsg.c_str(), errmsg.size());
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <port> <root_directory>\n";
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]); // User Input for the Port
    string root_directory = argv[2]; // User Input fo the direc

    // Validate the root directory
    struct stat root_stat;
    if (stat(root_directory.c_str(), &root_stat) != 0 || !S_ISDIR(root_stat.st_mode)) {
        perror("root directory");
        exit(EXIT_FAILURE);
    }

    // Create the server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configure the server address structure
    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Try to bind the socket
    if (::bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections (with a limit of 10 connections)
    if (listen(server_socket, 10) < 0) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port " << port << endl;

    // Main server loop to accept and handle connections
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Create a child process to handle the client request
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(client_socket);
        } else if (pid == 0) { // Child Process
            close(server_socket);
            client_socket_connection(client_socket, root_directory);
            close(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket); // Parent process accepts new connections
        }
    }

    // Close the server socket
    close(server_socket);
    exit(EXIT_FAILURE);
    return 0;
}