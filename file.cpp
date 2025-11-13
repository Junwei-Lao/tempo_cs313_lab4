#include "common.h"
#include "channel.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

int main(int argc, char* argv[]) {
    vector<string> allowed_extensions;
    
    // Get allowed extensions from command line arguments
    for (int i = 1; i < argc; i++) {
        allowed_extensions.push_back(argv[i]);
    }
    
    RequestChannel channel("file", RequestChannel::SERVER_SIDE);

    if (system("mkdir -p storage") != 0) {
        cout << "Error creating storage directory" << endl;
        return 1;
    }

    while (true) {
        Request r = channel.receive_request(0);

        if (r.type == QUIT) {
            Response resp(true, 0, "", "Server shutting down");
            channel.send_response(resp);
            exit(0);
        }

        Response resp;
        resp.success = true;
        
        if (r.type == UPLOAD_FILE) {
            // Check file extension if extensions were provided
            if (!allowed_extensions.empty()) {
                size_t dot_pos = r.filename.find_last_of(".");
                if (dot_pos == string::npos) {
                    resp.success = false;
                    resp.message = "File has no extension";
                    channel.send_response(resp);
                    continue;
                }

                string ext = r.filename.substr(dot_pos);
                bool allowed = false;
                for (const string& allowed_ext : allowed_extensions) {
                    if (ext == allowed_ext) {
                        allowed = true;
                        break;
                    }
                }
                
                if (!allowed) {
                    resp.success = false;
                    resp.message = "File extension not allowed";
                    channel.send_response(resp);
                    continue;
                }
            }
            
            string filepath = "storage/" + r.filename;
            ofstream outfile(filepath);
            
            if (!outfile) {
                resp.success = false;
                resp.message = "Failed to create file";
            } else {
                outfile << r.data;
                outfile.close();
                resp.message = "File uploaded successfully";
            }
        }
        else if (r.type == DOWNLOAD_FILE) {
            string filepath = "storage/" + r.filename;
            ifstream infile(filepath);
            
            if (!infile) {
                resp.success = false;
                resp.message = "File not found";
            } else {
                stringstream buffer;
                buffer << infile.rdbuf();
                resp.data = buffer.str();
                resp.message = "File downloaded successfully";
                infile.close();
            }
        }
        else {
            resp.success = false;
            resp.message = "Unknown RequestType";
        }

        channel.send_response(resp);
    }
    
    return 0;
}