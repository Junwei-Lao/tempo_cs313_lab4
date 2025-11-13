#include "common.h"
#include "channel.h"
#include "thread_pool.h"

using namespace std;

class Account {
public:
    int id;
    double balance;
    bool active;
    Account() : id(-1), balance(0.0), active(false) {} 
    Account(int _id) : id(_id), balance(0.0), active(true) {}
};

void applyInterest(Account& account) {
    // TODO: if the account is acctive and the balance is positive, then increase the account's balance by 1%
    // otherwise, just return without modification
    return;
}

int main(int argc, char* argv[]) {
    int max_accounts = 100;
    
    // Parse command line arguments
    for(int i = 1; i < argc; i++) {
        string arg = argv[i];
        if(arg == "-m" && i + 1 < argc) {
            max_accounts = atoi(argv[++i]) + 1;
        }
    }
    
    RequestChannel channel("finance", RequestChannel::SERVER_SIDE);
    Account* accounts = new Account[max_accounts];
    
    while (true) {
        Request r = channel.receive_request(0);

        if (r.type == QUIT) {
            Response resp(true, 0, "", "Server shutting down");
            channel.send_response(resp);
            //
            delete[] accounts;
            exit(0);
        }

        Response resp;
        resp.success = true;

        if (r.user_id < 0 || r.user_id >= max_accounts) {
            resp.success = false;
            resp.message = "Invalid account ID";
            channel.send_response(resp);
            continue;
        }

        // Create account if it doesn't exist
        if (!accounts[r.user_id].active) {
            accounts[r.user_id] = Account(r.user_id);
        }

        Account& acc = accounts[r.user_id];
        
        if (r.type == DEPOSIT) {
            acc.balance += r.amount;
            resp.balance = acc.balance;
            resp.message = "Deposit successful";
        } 
        else if (r.type == WITHDRAW) {
            if (acc.balance >= r.amount) {
                acc.balance -= r.amount;
                resp.balance = acc.balance;
                resp.message = "Withdrawal successful";
            } else {
                resp.success = false;
                resp.message = "Insufficient funds";
            }
        }
        else if (r.type == BALANCE) {
            resp.balance = acc.balance;
            resp.message = "View balance successful";
        }
        else if (r.type == EARN_INTEREST) {
            try {
                int numThreads = 2;
                if (r.amount > 0) numThreads = r.amount;
                // TODO: Create a ThreadPool and add all tasks to it
            } catch (const std::exception& e) {
                // TODO: Add error handling and set the response to have a false success value
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