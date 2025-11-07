#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>

using namespace std;

struct Instance {
    int id;
    bool active = false;
    int partiesServed = 0;
    int totalTime = 0;
};

// Shared data
int n;              // number of instances
int tanksLeft;
int healersLeft;
int dpsLeft;
int t1, t2;         // min and max duration (seconds)
int freeInstances;

vector<Instance> instances;
vector<thread> partyThreads;

mutex mtx;
condition_variable cond;

// Random number generator
int randomDuration(int min, int max) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

// Print current instance status (assumes lock held)
void printInstanceStatus() {
    cout << "Current instance status:\n";
    for (auto &inst : instances) {
        cout << "  Instance " << inst.id
             << ": " << (inst.active ? "active" : "empty") << "\n";
    }
    cout << endl;
}

// Party thread function
void runParty(int instanceId) {
    int duration = randomDuration(t1, t2);

    {
        lock_guard<mutex> lock(mtx);
        instances[instanceId].active = true;
        cout << "[Instance " << instanceId << "] Party started, duration "
             << duration << "s\n";
        printInstanceStatus();
    }

    this_thread::sleep_for(chrono::seconds(duration));

    {
        unique_lock<mutex> lock(mtx);
        instances[instanceId].active = false;
        instances[instanceId].partiesServed++;
        instances[instanceId].totalTime += duration;
        freeInstances++;

        cout << "[Instance " << instanceId << "] Party finished.\n";
        printInstanceStatus();

        cond.notify_all(); // wake matchmaker
    }
}

// Check if we can form a party
bool canFormParty() {
    return tanksLeft >= 1 && healersLeft >= 1 && dpsLeft >= 3;
}

// Find a free instance
int findFreeInstance() {
    for (auto &inst : instances) {
        if (!inst.active)
            return inst.id;
    }
    return -1;
}

// Matchmaker function
void formParties() {
    while (true) {
        unique_lock<mutex> lock(mtx);

        // Wait for a free instance if needed
        while (freeInstances == 0 && canFormParty()) {
            cond.wait(lock);
        }

        // Stop if we can no longer form parties
        if (!canFormParty()) {
            break;
        }

        if (freeInstances == 0)
            continue; // just to be safe

        // Consume players for one party
        tanksLeft--;
        healersLeft--;
        dpsLeft -= 3;

        int instId = findFreeInstance();
        if (instId == -1)
            continue;

        freeInstances--;
        instances[instId].active = true;

        cout << "[Matchmaker] Created party on Instance " << instId << "\n";
        printInstanceStatus();

        // Spawn a new thread for the party
        partyThreads.emplace_back(runParty, instId);
    }
}

// Print final summary
void printSummary() {
    cout << "\n=== Final Summary ===\n";
    for (auto &inst : instances) {
        cout << "Instance " << inst.id
             << ": parties served = " << inst.partiesServed
             << ", total time = " << inst.totalTime << "s\n";
    }
}

int main() {
    cout << "Number of dungeon instances (n): ";
    cin >> n;
    cout << "Number of TANKS (t): ";
    cin >> tanksLeft;
    cout << "Number of HEALERS (h): ";
    cin >> healersLeft;
    cout << "Number of DPS (d): ";
    cin >> dpsLeft;
    cout << "Minimum dungeon time t1 (seconds): ";
    cin >> t1;
    cout << "Maximum dungeon time t2 (seconds): ";
    cin >> t2;

    if (t2 < t1) swap(t1, t2);

    instances.resize(n);
    for (int i = 0; i < n; i++) {
        instances[i].id = i;
    }
    freeInstances = n;

    cout << "\n=== LFG Matchmaking Started ===\n\n";

    // Run the matchmaker in main thread
    formParties();

    // Wait for all parties to finish
    for (auto &t : partyThreads)
        t.join();

    printSummary();

    cout << "\nAll parties finished. Exiting.\n";
    return 0;
}