#include "channel.h"
#include "signals.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sstream>

using namespace std;

RequestChannel::RequestChannel(const string name, const Side side) : 
    process_name(name), my_side(side), read_fd(-1), write_fd(-1) {
    
    read_pipe = "fifo_" + name + "_" + (side == SERVER_SIDE ? "1" : "2");
    write_pipe = "fifo_" + name + "_" + (side == SERVER_SIDE ? "2" : "1");

    // Create FIFOs
    if (mkfifo(read_pipe.c_str(), 0666) < 0 && errno != EEXIST) {
        perror(("Error creating read pipe " + read_pipe).c_str());
    }
    if (mkfifo(write_pipe.c_str(), 0666) < 0 && errno != EEXIST) {
        perror(("Error creating write pipe " + write_pipe).c_str());
    }

    // Give both sides a chance to set up
    usleep(100000); // Wait 0.1 seconds

    // Open pipes in the correct order
    if (side == SERVER_SIDE) {
        read_fd = open(read_pipe.c_str(), O_RDONLY);
        write_fd = open(write_pipe.c_str(), O_WRONLY);
    } else {
        write_fd = open(write_pipe.c_str(), O_WRONLY);
        read_fd = open(read_pipe.c_str(), O_RDONLY);
    }
}

RequestChannel::~RequestChannel() {
    close(read_fd);
    close(write_fd);
    unlink(read_pipe.c_str());
    unlink(write_pipe.c_str());
}

Response RequestChannel::send_request(const Request& req, int timeout_seconds) {
    // Use alarm and timeout for sending request
    Response resp;
    SignalHandling::timeout_occurred = false;
    
    if (!SignalHandling::wait_with_timeout(timeout_seconds)) {
        return Response(false, 0, "", "Request timed out");
    }
    
    // Use a more robust format with delimiters
    stringstream ss;
    ss << static_cast<int>(req.type) << "|"
       << req.user_id << "|"
       << req.amount << "|"
       << req.filename << "|"
       << req.data << "\n";
    string msg = ss.str();
    
    // Write the full message
    int bytes_written = write(write_fd, msg.c_str(), msg.size());
    if (bytes_written < 0) {
        perror("Write failed");
        SignalHandling::cancel_timeout();
        return Response(false, 0, "", "Write failed");
    }

    // Read response
    char buf[1024];
    int bytes_read = read(read_fd, buf, sizeof(buf)-1);
    if (bytes_read < 0) {
        perror("Read failed");
        SignalHandling::cancel_timeout();
        return Response(false, 0, "", "Read failed");
    }
    buf[bytes_read] = '\0';

    SignalHandling::cancel_timeout();
    
    if (SignalHandling::timeout_occurred) {
        return Response(false, 0, "", "Operation timed out");
    }

    string response_str(buf);
    size_t pos = 0;
    string token;
    string delimiter = "|";

    // Parse response using same delimiter format
    if ((pos = response_str.find(delimiter)) != string::npos) {
        resp.success = (response_str.substr(0, pos) == "1");
        response_str.erase(0, pos + delimiter.length());
    }
    if ((pos = response_str.find(delimiter)) != string::npos) {
        resp.balance = stod(response_str.substr(0, pos));
        response_str.erase(0, pos + delimiter.length());
    }
    if ((pos = response_str.find(delimiter)) != string::npos) {
        resp.data = response_str.substr(0, pos);
        resp.message = response_str.substr(pos + delimiter.length());
    }
    
    return resp;
}

Request RequestChannel::receive_request(int timeout_seconds) {
    char buf[1024];
    
    // For servers, don't terminate on timeout
    bool is_server = (my_side == SERVER_SIDE);
    
    SignalHandling::timeout_occurred = false;
    if (!SignalHandling::wait_with_timeout(timeout_seconds)) {
        if (is_server) {
            // For servers, just try again instead of returning QUIT
            return receive_request(timeout_seconds);
        }
        return Request(QUIT); // Return QUIT on timeout for clients
    }
    
    int bytes_read = read(read_fd, buf, sizeof(buf)-1);
    
    SignalHandling::cancel_timeout();
    
    if (bytes_read <= 0 || SignalHandling::timeout_occurred) {
        // Timeout or error occurred - return QUIT to trigger cleanup
        return Request(QUIT);
    }
    
    buf[bytes_read] = '\0';
    return Request::parseRequest(buf);
}

void RequestChannel::send_response(const Response& resp) {
    stringstream ss;
    ss << (resp.success ? "1" : "0") << "|"
       << resp.balance << "|"
       << resp.data << "|"
       << resp.message << "\n";
    string msg = ss.str();
    
    int bytes_written = write(write_fd, msg.c_str(), msg.size());
    if (bytes_written < 0) {
        perror("Write failed in send_response");
    }
}

string RequestChannel::get_process_name() const {
    return process_name;
}