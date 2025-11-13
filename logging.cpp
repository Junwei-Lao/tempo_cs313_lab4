#include "common.h"
#include "channel.h"
#include <fstream>

using namespace std;

int main(int argc, char* argv[]) {
    // Default log file if not specified
    string log_file = "system.log";
    
    // Parse command line arguments
    for(int i = 1; i < argc; i++) {
        string arg = argv[i];
        if(arg == "-f" && i + 1 < argc) {
            log_file = argv[++i];
        }
    }
    
    RequestChannel channel("logging", RequestChannel::SERVER_SIDE);
    ofstream logfile(log_file, ios::app);

    while (true) {
        Request r = channel.receive_request(0);

        if (r.type == QUIT) {
            Response resp(true, 0, "", "Server shutting down");
            channel.send_response(resp);
            exit(0);
        }

        logfile << "[" << r.user_id << "]: ";
        
        switch(r.type) {
            case LOGIN:
                logfile << "logged in";
                break;
            case LOGOUT:
                logfile << "logged out";
                break;
            case DEPOSIT:
                logfile << "deposited " << r.amount;
                break;
            case WITHDRAW:
                logfile << "withdrew " << r.amount;
                break;
            case BALANCE:
                logfile << "viewed balance: " << r.amount;
                break;
            case EARN_INTEREST: // new option
                logfile << "accrued interest in all accounts";
                break;
            case UPLOAD_FILE:
                logfile << "uploaded file: " << r.filename;
                break;
            case DOWNLOAD_FILE:
                logfile << "downloaded file: " << r.filename;
                break;
            default:
                logfile << "unknown action (type=" << r.type << ")";
        }
        logfile << endl;

        Response resp;
        resp.success = true;
        resp.message = "Logged successfully";
        
        // Use the channel's send_response method instead of direct write
        channel.send_response(resp);
    }

    logfile.close();
    return 0;
}