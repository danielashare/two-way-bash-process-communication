#include <string>    // for strings
#include <vector>    // for vectors
#include <unistd.h>  // for fork, read
#include <signal.h>  // for kill
#include <wait.h>    // for waitpid
#include <iostream>  // for cout
#include <random>    // for mt19937
#include <algorithm> // for generate
#include <fcntl.h>   // for fnctl

using namespace std;

// Fork the program to create a child
bool startChildShell(int& in, int& out, pid_t& childPID)
{
    // Store file descriptors that will be used to communicate with the child
    int inPipeFD[2];
    int outPipeFD[2]; 

    pipe(inPipeFD);
    pipe(outPipeFD);

    // Set read pipes to non-blocking and handle waiting in the runCommand method
    fcntl(inPipeFD[0], F_SETFL, fcntl(inPipeFD[0], F_GETFL) & O_NONBLOCK);
    fcntl(inPipeFD[1], F_SETFL, fcntl(inPipeFD[1], F_GETFL) & O_NONBLOCK);

    // Fork here to create a child that will run the commands
    pid_t pid = fork();

    if(pid < 0)
    {
        // Failed to fork process
        return false;
    }
    else if(pid == 0) // The child
    {
        // Close STDIN and replace it with outPipeFD read end
        dup2(outPipeFD[0], STDIN_FILENO);
        close(outPipeFD[0]); // not needed anymore

        // Close STDOUT and replace it with inPipe read end
        dup2(inPipeFD[1], STDOUT_FILENO);
        close(inPipeFD[1]); // not needed anymore

        // Start a bash shell with the runuser command
        execl("/bin/bash", "bash", (char*) NULL);

        // Make child kill itself if execl fails
        kill(getpid(), SIGTERM);
        return false;
    }
    else // The parent
    {
        // Close the read end of the write pipe
        close(outPipeFD[0]);

        // Close the write end of the read pipe
        close(inPipeFD[1]);
    }

    // Store the childPID so that it can be killed later
    childPID = pid;

    // For ease of reading store the in and out pipes that the parent will use to communicate
    out = outPipeFD[1];
    in = inPipeFD[0];

    // Wait for the child to change state
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);

    return true;
}

// Generate a random string for use when reading the output of a command
string generateDelimiter()
{
    thread_local mt19937 prng(random_device{}());
    thread_local uniform_int_distribution<> dist('a', 'z');
    thread_local auto gen = [&]() { return dist(prng); };

    string delimiter(128, 0);
    std::generate(delimiter.begin(), delimiter.end(), gen);

    return delimiter;
}

// Run a command 
pair<string, int> runCommand(string command, int& in, int& out)
{
    // Buffer size for things being sent through pipes
    constexpr size_t bufferSize = 1024;

    // a string that is unlikely to show up in the output:
    const string delimiter = generateDelimiter() + "\n";

    // Add an echo of the status of the command that was ran
    command += " 2>&1\necho -e $?\"\\n\"" + delimiter;

    // Send the command to the child process
    auto len = write(out, command.c_str(), command.size());

    // If the length is 0 or less, the pipe couldn't be written to
    if(len <= 0)
    {
        return {{}, -1};
    }

    // Wipe command and use it to collect all read data
    command.resize(0);

    // Use buffer to store each line being read
    string buffer(bufferSize, 0);

    // Loop and read all data until the delimiter is found
    fd_set read_set{};
    FD_SET(in, &read_set);
    while(true) {
        // Wait until something happens on the pipe
        select(in + 1, &read_set, nullptr, nullptr, nullptr);

        // If the read failed on the pipe
        if((len = read(in, &buffer[0], buffer.size())) <= 0)
        {
            // Set an error code and break reading
            command += "\n" + to_string(-1) + "\n" + delimiter;
            break;
        }

        // Append what was read to command
        command.append(buffer.begin(), buffer.begin() + len);

        // If the delimiter was found, break
        if(command.size() >= delimiter.size() && command.substr(command.size() - delimiter.size()) == delimiter)
        {
            break;
        }
    }

    // Remove the delimiter from the command output and trailing newline
    command.resize(command.size() - (delimiter.size() + 1));

    // Get the position of the newline that is in front of the exit status
    int newLinePosition = command.rfind('\n');

    // Extract the exit status and convert to int
    int exitStatus = stoi(command.substr(newLinePosition + 1, command.size()));

    // Remove the exit status from the command output
    string output = command.substr(0, newLinePosition);

    // Return the pair
    return {output, exitStatus};
}

int main(int argc, char *argv[])
{
    // Your list of commands
    vector<string> commands {"BINGO=BONGO", "echo $BINGO", "whoami"};

    // Store the process ID of the child that will be running the bash commands
    // so that we can kill it once all the commands have been ran
    pid_t childPID;

    // Create empty ints to store the pipes file descriptors that will be 
    // used to comuunicate with the child process. The ints will be populated 
    // when passed to the startChildShell function.
    int in, out;

    // Create a child shell and pass back the pipe file descriptors
    startChildShell(in, out, childPID);

    // Loop through your commands and run them
    for(string& command : commands) {
        auto [output, exitStatus] = runCommand(command, in, out);
        cout << output << endl;
        cout << exitStatus << endl;
    }

    // Kill the child process
    kill(childPID, SIGTERM);

    return 0;
}