#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include "common.h"
#include <string>

class RequestChannel {
public:
    enum Side {SERVER_SIDE, CLIENT_SIDE};
    
    RequestChannel(const std::string process_name, const Side side);
    ~RequestChannel();
    
    // Modified to support timeout
    Response send_request(const Request& req, int timeout_seconds = 30);
    Request receive_request(int timeout_seconds = 30);
    void send_response(const Response& resp);
    std::string get_process_name() const;

private:
    std::string process_name;
    Side my_side;
    std::string read_pipe;
    std::string write_pipe;
    int read_fd;
    int write_fd;
};

#endif
