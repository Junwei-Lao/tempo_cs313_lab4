#include "common.h"
#include "channel.h"
#include "signals.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <vector>
#include <limits>

using namespace std;
using namespace SignalHandling;

void print_menu() {
    cout << "\n=== Banking System Menu ===\n"
         << "1. Login\n"
         << "2. Deposit\n"
         << "3. Withdraw\n"
         << "4. View Balance\n"
         << "5. Upload File\n"
         << "6. Download File\n"
         << "7. Logout\n"
         << "8. Server Status\n"
         << "9. Update Interest for All Accounts\n"   // New option
         << "0. Exit\n"
         << "Enter choice: ";
}

void empty_file(const string& filename) {
    ofstream clear_file(filename, ios::trunc);
    clear_file.close();
}

void clear_input() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// Retry mechanism for timed-out operations
template<typename Func>
void retry_operation(const string& operation_name, Func operation, int max_retries = 3) {
    int retries = 0;
    bool success = false;
    
    while (!success && retries < max_retries && !shutdown_requested) {
        if (retries > 0) {
            cout << "Retrying " << operation_name << " (attempt " << retries+1 << " of " << max_retries << ")..." << endl;
        }
        
        success = operation();
        
        if (!success && !shutdown_requested) {
            retries++;
            if (retries < max_retries) {
                char retry;
                cout << "Operation failed. Retry? (y/n): ";
                cin >> retry;
                clear_input();
                
                if (tolower(retry) != 'y') {
                    cout << "Operation canceled." << endl;
                    break;
                }
            } else {
                cout << "Maximum retry attempts reached." << endl;
            }
        }
    }
}

int main() {
    // Initialize signal handling
    SignalHandling::setup_handlers();
    SignalHandling::log_signal_event("Client started");

    // Start servers
    cout << "Starting servers..." << endl;

    // finance server
    int max_account;
    cout << "Enter the maximum account number: ";
    cin >> max_account;
    clear_input();

    // Start finance server
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }
    if (pid == 0) { // Child process
        char* args[] = {(char*)"./finance", (char*)"-m", (char*)to_string(max_account).c_str(), nullptr};
        execvp(args[0], args);
        perror("Execvp failed");
        exit(1);
    }
    
    // Register finance server with signal handler
    SignalHandling::register_server(pid, "finance");

    // logging server
    string log_file_name;
    cout << "Enter the name of your logging file: ";
    getline(cin, log_file_name);
    if (log_file_name.empty()) {
        log_file_name = "system.log";
        cout << "Using default: " << log_file_name << endl;
    }

    pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }
    if (pid == 0) { // Child process
        char* args[] = {(char*)"./logging", (char*)"-f", (char*)log_file_name.c_str(), nullptr};
        execvp(args[0], args);
        perror("Execvp failed");
        exit(1);
    }
    
    // Register logging server with signal handler
    SignalHandling::register_server(pid, "logging");

    // file server
    cout << "Enter number of allowed file extensions: ";
    int num_extensions;
    cin >> num_extensions;
    cin.ignore(); // Clear newline

    // Store extensions in vector
    vector<string> extensions;
    cout << "Enter allowed extensions (including the dot, e.g. .txt):" << endl;
    for(int i = 0; i < num_extensions; i++) {
        cout << i+1 << ": ";
        string ext;
        getline(cin, ext);
        if (ext.empty()) {
            ext = ".txt";
            cout << "Using default: " << ext << endl;
        }
        extensions.push_back(ext);
    }

    // Create argument array for file server
    char** file_args = new char*[num_extensions + 2]; // +2 for program name and NULL
    file_args[0] = (char*)"./file";
    
    // Fill with pointers to the extension strings
    for(int i = 0; i < num_extensions; i++) {
        file_args[i + 1] = (char*)extensions[i].c_str();
    }
    file_args[num_extensions + 1] = NULL;

    pid_t file_pid = fork();
    if (file_pid < 0) {
        perror("Fork failed");
        exit(1);
    }
    if (file_pid == 0) {
        execvp(file_args[0], file_args);
        perror("File server exec failed");
        exit(1);
    }
    
    // Register file server with signal handler
    SignalHandling::register_server(file_pid, "file");

    delete[] file_args;
    
    // Give servers time to start
    cout << "Waiting for servers to start..." << endl;
    sleep(1);
    
    // Create RequestChannels for each server
    RequestChannel finance("finance", RequestChannel::CLIENT_SIDE);
    RequestChannel file("file", RequestChannel::CLIENT_SIDE);
    RequestChannel logging("logging", RequestChannel::CLIENT_SIDE);

    int current_user = -1;  // -1 means no user logged in
    bool running = true;
    
    while (running && !shutdown_requested) {
        print_menu();
        
        int choice;
        if (!(cin >> choice)) {
            // Check for shutdown after failed input
            if (SignalHandling::shutdown_requested) {
                break;  // Exit the main loop
            }
            cout << "Invalid input. Please enter a number: ";
            clear_input();
            continue;
        }
        clear_input();

        // Check for shutdown before processing
        if (shutdown_requested) {
            break;
        }

        try {
            switch (choice) {
                case 0: {  // Exit
                    running = false;
                    break;
                }
                
                case 1: {  // Login
                    if (current_user != -1) {
                        cout << "Already logged in! Please logout first.\n";
                        break;
                    }
                    
                    cout << "Enter user ID: ";
                    cin >> current_user;
                    clear_input();
                    
                    // Login with timeout (10 seconds)
                    auto login_operation = [&]() {
                        Request login(LOGIN, current_user);
                        Response resp;

                        
                        bool success = execute_with_timeout([&]() {
                            resp = logging.send_request(login);
                            return true;
                        }, 10);
                        
                        if (!success) {
                            cout << "Login timed out after 10 seconds." << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Logged in as user " << current_user << endl;
                            return true;
                        } else {
                            cout << "Login failed: " << resp.message << endl;
                            current_user = -1;
                            return false;
                        }

                    };

                    // Block signals during transaction
                    block_signals();
                    
                    retry_operation("login", login_operation);
                    
                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 2: {  // Deposit
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    double amount;
                    cout << "Enter amount to deposit: ";
                    cin >> amount;
                    clear_input();
                    
                    // Deposit with timeout (30 seconds)
                    auto deposit_operation = [&]() {
                        Request txn(DEPOSIT, current_user, amount);
                        Response resp;
                        
                        bool success = execute_with_timeout([&]() {
                            resp = finance.send_request(txn);
                            return true;
                        }, 30);
                        
                        if (!success) {
                            cout << "Deposit timed out after 30 seconds." << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Deposit successful. New balance: " << resp.balance << endl;
                            
                            // Log the deposit
                            Request audit(DEPOSIT, current_user, amount);
                            Response log_resp = logging.send_request(audit);
                            if (!log_resp.success) {
                                cout << "Warning: Failed to log transaction" << endl;
                            }
                            return true;
                        } else {
                            cout << "Deposit failed: " << resp.message << endl;
                            return false;
                        }
                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("deposit", deposit_operation);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 3: {  // Withdraw
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    double amount;
                    cout << "Enter amount to withdraw: ";
                    cin >> amount;
                    clear_input();
                    
                    // Withdraw with timeout (30 seconds)
                    auto withdraw_operation = [&]() {
                        Request txn(WITHDRAW, current_user, amount);
                        Response resp;
                        
                        bool success = execute_with_timeout([&]() {
                            resp = finance.send_request(txn);
                            return true;
                        }, 30);
                        
                        if (!success) {
                            cout << "Withdrawal timed out after 30 seconds." << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Withdrawal successful. New balance: " << resp.balance << endl;
                            
                            // Log the withdrawal
                            Request audit(WITHDRAW, current_user, amount);
                            Response log_resp = logging.send_request(audit);
                            if (!log_resp.success) {
                                cout << "Warning: Failed to log transaction" << endl;
                            }
                            return true;
                        } else {
                            cout << "Withdrawal failed: " << resp.message << endl;
                            return false;
                        }

                    };


                    // Block signals during transaction
                    block_signals();

                    retry_operation("withdrawal", withdraw_operation);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 4: {  // View Balance
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    // View balance with timeout (15 seconds)
                    auto balance_operation = [&]() {
                        Request txn(BALANCE, current_user);
                        Response resp;

                        bool success = execute_with_timeout([&]() {
                            resp = finance.send_request(txn);
                            return true;
                        }, 15);
                        
                        if (!success) {
                            cout << "Balance request timed out after 15 seconds." << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Current balance: " << resp.balance << endl;
                            
                            // Log the balance view
                            Request audit(BALANCE, current_user, resp.balance);
                            Response log_resp = logging.send_request(audit);
                            if (!log_resp.success) {
                                cout << "Warning: Failed to log transaction" << endl;
                            }
                            return true;
                        } else {
                            cout << "Failed to get balance: " << resp.message << endl;
                            return false;
                        }

                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("balance check", balance_operation);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 5: {  // Upload File
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    string filename;
                    cout << "Enter filename to upload: ";
                    getline(cin, filename);
                    
                    ifstream infile(filename);
                    if (!infile) {
                        cout << "Error: Could not open file\n";
                        break;
                    }
                    
                    string content((istreambuf_iterator<char>(infile)), {});
                    infile.close();

                    // Upload file with timeout (60 seconds)
                    auto upload_operation = [&]() {
                        Request upload(UPLOAD_FILE, current_user, 0, filename, content);
                        Response resp;
                        
                        bool success = execute_with_timeout([&]() {
                            resp = file.send_request(upload);
                            return true;
                        }, 60);
                        
                        if (!success) {
                            cout << "File upload timed out after 60 seconds." << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "File upload successful\n";
                            
                            // Log the file upload
                            Request audit(UPLOAD_FILE, current_user, 0, filename);
                            Response log_resp = logging.send_request(audit);
                            if (!log_resp.success) {
                                cout << "Warning: Failed to log file upload" << endl;
                            }
                            return true;
                        } else {
                            cout << "File upload failed: " << resp.message << endl;
                            return false;
                        }

                    };

                     // Block signals during transaction
                    block_signals();

                    retry_operation("file upload", upload_operation);

                    // Unblock signals after transaction
                    unblock_signals();

                    break;
                }
                
                case 6: {  // Download File
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    string filename;
                    cout << "Enter filename to download: ";
                    getline(cin, filename);
                    
                    // Download file with timeout (60 seconds)
                    auto download_operation = [&]() {
                        Request download(DOWNLOAD_FILE, current_user, 0, filename);
                        Response resp;
                        
                        bool success = execute_with_timeout([&]() {
                            resp = file.send_request(download);
                            return true;
                        }, 60);
                        
                        if (!success) {
                            cout << "File download timed out after 60 seconds." << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            ofstream outfile(filename);
                            if (!outfile) {
                                cout << "Error: Could not create output file\n";
                                return false;
                            }
                            outfile << resp.data;
                            outfile.close();
                            cout << "File downloaded successfully\n";
                            
                            // Log the file download
                            Request audit(DOWNLOAD_FILE, current_user, 0, filename);
                            Response log_resp = logging.send_request(audit);
                            if (!log_resp.success) {
                                cout << "Warning: Failed to log file download" << endl;
                            }
                            return true;
                        } else {
                            cout << "File download failed: " << resp.message << endl;
                            return false;
                        }

                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("file download", download_operation);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 7: {  // Logout
                    if (current_user == -1) {
                        cout << "Not logged in!\n";
                        break;
                    }

                    // Logout with timeout (10 seconds)
                    auto logout_operation = [&]() {
                        Request logout(LOGOUT, current_user);
                        Response resp;
                        
                        bool success = execute_with_timeout([&]() {
                            resp = logging.send_request(logout);
                            return true;
                        }, 10);
                        
                        if (!success) {
                            cout << "Logout timed out after 10 seconds." << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            current_user = -1;
                            cout << "Logged out successfully\n";
                            return true;
                        } else {
                            cout << "Logout failed: " << resp.message << endl;
                            return false;
                        }

                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("logout", logout_operation);

                    // Unblock signals after transaction
                    unblock_signals();

                    break;
                }
                
                case 8: { 
                    SignalHandling::print_server_status();
                    break;
                }
                
                case 9: {  // Accrue Interest - new option
                    if (current_user == -1) {
                        cout << "Only user 0 can update interest!\n";
                        break;
                    }

                    int numThreads = 2;
                    cout << "Input a number of threads to use: ";
                    cin >> numThreads;

                    Request request(EARN_INTEREST, current_user, numThreads);
                    Response resp;
                    resp = finance.send_request(request);

                    if (!resp.success) {
                        cout << "Interest update failed" << endl;
                    } else {
                        cout << "Interest update successful!" << endl;
                            
                        Response log_resp = logging.send_request(request);
                        if (!log_resp.success) {
                            cout << "Warning: Failed to log transaction" << endl;
                        }
                    }

                    break;
                }
                
                default:
                    cout << "Invalid choice. Please try again.\n";
            }
        } catch (const exception& e) {
            cout << "Error during operation: " << e.what() << endl;
            log_signal_event("Exception caught: " + string(e.what()));
        }
    }

    // Graceful shutdown
    if (shutdown_requested) {
        cout << "\nPerforming graceful shutdown..." << endl;
        log_signal_event("Beginning graceful shutdown");
    } else {
        cout << "Exiting normally..." << endl;
        log_signal_event("Normal exit requested");
    }

    // Cleanup - send QUIT to all servers with timeout
    cout << "Sending shutdown signals to servers..." << endl;
    log_signal_event("Sending QUIT to all servers");

    Request quit(QUIT);
    SignalHandling::timeout_occurred = false;

    // Try to send QUIT to finance with 3-second timeout
    SignalHandling::wait_with_timeout(3);
    try {
        finance.send_request(quit);
        log_signal_event("QUIT sent to finance server");
    } catch (const exception& e) {
        log_signal_event("Failed to send QUIT to finance server: " + string(e.what()));
    }
    SignalHandling::cancel_timeout();

    // Try to send QUIT to file with 3-second timeout
    SignalHandling::wait_with_timeout(3);
    try {
        file.send_request(quit);
        log_signal_event("QUIT sent to file server");
    } catch (const exception& e) {
        log_signal_event("Failed to send QUIT to file server: " + string(e.what()));
    }
    SignalHandling::cancel_timeout();

    // Try to send QUIT to logging with 3-second timeout
    SignalHandling::wait_with_timeout(3);
    try {
        logging.send_request(quit);
        log_signal_event("QUIT sent to logging server");
    } catch (const exception& e) {
        log_signal_event("Failed to send QUIT to logging server: " + string(e.what()));
    }
    SignalHandling::cancel_timeout();
    
    // Wait for all child processes
    cout << "Waiting for all child processes to terminate..." << endl;
    log_signal_event("Waiting for child processes");
    while(wait(NULL) > 0);
    log_signal_event("All child processes terminated");
    
    log_signal_event("Client shutdown complete");
    cout << "Shutdown complete.\n";
    
    return 0;
}