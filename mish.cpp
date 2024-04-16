#include <iostream>
#include "cmake-build-debug/mish.h"
#include <boost/algorithm/string.hpp>
#include <sys/wait.h>
#include <fstream>
#include <readline/readline.h>
#include <readline/history.h>
#include <filesystem>

using namespace std;

int main(int argv, char **argc) {
    int i;
    string input = " ";
    char* cinput;
    vector<string> commands;
    fstream fin;
    int pid;
    std::filesystem::path cwd;

    // Adding tab completion and history to the shell.
    rl_bind_key('\t', rl_complete);
    using_history();

    // Opening a file for input if there was a file specified at start up.
    if(argv == 2) {
        fin.open(argc[1]);
    }
    else if(argv > 2) {
        cout << "Incorrect Usage. Only 0-1 arguments allowed." << endl;
    }

    while (input != "exit" && !fin.eof())
    {
        // Getting the current working directory to be displayed to the user.
        cwd = std::filesystem::current_path();

        // Reading in commands from the user or a file.
        if(argv == 1) {
            cinput = readline(("mish:~" + cwd.string() + "> ").c_str());

            // Exiting if an EOF was entered.
            if(!cinput)
                exit(0);
            input = cinput;
        }
        else
            getline(fin, input);

        // Adding the last command to the history list so users can have a command history.
        add_history(input.c_str());

        // Killing the program when an exit is requested by the user.
        if(input == "exit" || fin.eof())
            exit(0);

        // Splitting the string on every &.
        commands = splitString(input, "&");

        // Running all the commands in parallel.
        for(i = 0; i < commands.size(); i++)
        {
            boost::trim(commands[i]);  // Trimming white space out of the commands.
            if (commands[i].empty())  // Skipping execution if the command is empty.
                continue;
            if(!executeBuiltIns(commands[i])) {
                pid = fork();
                if(pid == 0) // Child process.
                {
                    runCommand(commands[i]);
                    perror("Program Error");
                    exit(0);
                }
                else if(pid < 0) // Failed to fork.
                    perror("Epic Fork Fail, AHHHHHH!");
            }
        }

        // Waiting for all the child processes to finish.
        while(wait(nullptr) > 0);
    }
    fin.close();
}


// Splits the given string on the delimiter into a vector of strings.
vector<string> splitString(string str, string delimiter)
{
    vector<string> commands;
    boost::split(commands, str, boost::is_any_of(delimiter));
    return commands;
}

// Running the command that was sent into the program.
void runCommand(string command)
{
    stringstream sstream(command);
    vector<string> mods;
    vector<const char *> words;
    string word;
    int i, j;
    int inputNum;
    vector<int*> piped;
    char ** modifiers;

    if(!checkCommand(command)) {
        return;
    }

    // Clearing white space off of the arguments of the command.
    while(sstream >> word) {
        mods.push_back(word);
    }

    // Error out if there is a trailing pipe character in the command.
    if(mods[mods.size() - 1] == "|") {
        cout << "Error: Command cannot have a trailing pipe." << endl;
        exit(0);
    }

    // Doing piping things.
    if (getOperator(mods, '|') != -1) {
        vector<string> pipedCommands = splitString(command, "|");
        int pid;

        // Setting up all the pipes.
        piped.resize(pipedCommands.size() - 1);
        for(i = 0; i < piped.size(); i++) {
            int *pipefd = new int[2];
            pipe(pipefd);
            piped[i] = pipefd;
        }

        // Running all the piped commands.
        for (i = 0; i < pipedCommands.size(); i++) {
            pid = fork();
            if (pid == 0) {

                // If you're not the first process, redirect your stdin to the pipe.
                if(i != 0) {
                    dup2(piped[i-1][0], STDIN_FILENO);
                }

                // If you're not the last process, redirect your stdout to the pipe.
                if(i < pipedCommands.size() - 1) {
                    dup2(piped[i][1], STDOUT_FILENO);
                    dup2(piped[i][1], STDERR_FILENO);
                }

                // Closing all the pipes in the child process.
                for(j = 0; j < piped.size(); j++) {
                    close(piped[j][0]);
                    close(piped[j][1]);
                }

                // Running the command and outputting an error if there was a problem.
                runCommand(pipedCommands[i]);
                perror("Pipe Error");
                exit(0);
            }
            // Do nothing in the parent.
            else if(pid > 0) {
            }
        }

        // Closing all the pipes in the parent.
        for(i = 0; i < piped.size(); i++) {
            close(piped[i][0]);
            close(piped[i][1]);
            delete piped[i];
        }
        while(wait(nullptr) > 0);  // Waiting for all the processes to finish.
        exit(0);
    }

    // Output redirection.
    if(getOperator(mods, '>') != -1) {
        // Opening the file for output with all the proper flags.
        int file = open(mods[mods.size() - 1].c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        dup2(file, STDOUT_FILENO);  // Redirecting standard out.
        dup2(file, STDERR_FILENO);  // Redirecting standard error.
        mods.pop_back();  // Taking the file name off the argument list.
    }

    // Input redirection.
    if(checkSetting(command, '<')) {

        int file = open(mods[mods.size() - 1].c_str(), O_RDONLY);
        if(file == -1) {
            perror(("Error Opening File " + mods[mods.size()]).c_str());
            return;
        }
        mods.pop_back();
        mods.pop_back();
        if(dup2(file, STDIN_FILENO) == -1) {
            perror("Error redirecting STDIN: ");
            return;
        }
        close(file);
    }

    // Converting the string vector into a vector of character pointers.
    for(i = 0; i < mods.size(); i++) {
        words.push_back(mods[i].c_str());
    }

    // Converting the vector of character pointers to a double pointer array so execvp can use it.
    words.push_back(nullptr);
    modifiers = const_cast<char **>(&words[0]);

    // Running the command.
    execvp(modifiers[0], modifiers);
}

// Executing the build-in (built into the shell) commands, and outputting true if the command was a built-in.
// Outputting false if the command wasn't a built-in command.
bool executeBuiltIns(string command) {
    stringstream sstream(command);
    vector<string> mods;
    string word;

    // Clearing out the white space in the string and pushing it onto the instruction vector.
    while(sstream >> word) {
        mods.push_back(word);
    }

    // Implementing a directory change if requested.
    if(mods[0] == "cd") {
        if(mods.size() != 2) {
            perror("Incorrect number of arguments");
            return true;
        }
        chdir(mods[1].c_str());  // Changing the directory.
        return true;
    }
    // Exiting if the exit command was entered.
    else if(mods[0] == "exit")
    {
        exit(0);
        return true;
    }

    // Setting an environment variable.
    else if(checkSetting(command, '=')) {
        vector<string> strs = splitString(command, "=");
        setenv(strs[0].c_str(), strs[1].c_str(), 1);
        return true;
    }
    return false;
}

// Checking if the findee is in the string.
bool checkSetting(string str, char findee)
{
    int i;

    for(i =0; i < str.size(); i++)
    {
        if(str[i] == findee)
            return true;  // Returning true if it is found.
    }
    return false;  // False if it is not.
}

// Finding if an operator is in the vector of commands, and fixing the vector if it is so it can be used later.
// Returns -1 if not found, the approximate location of it if it is found.
int getOperator(vector<string> &commands, char op)
{
    int i, j, k;
    for(i = 0; i < commands.size(); i++) {
        for(j = 0; j < commands[i].size(); j++) {
            if(commands[i][j] == op) {

                // Fixing the vector so it is usable later in the code.
                vector<string> mods = splitString(commands[i], string(1, op));
                commands.erase(commands.begin() + i);
                for(k = 0; k < mods.size(); k++) {
                    if(!mods[k].empty())
                        // Inserting the newly split string into the correct spot in the commands vector.
                        commands.insert(commands.begin() + i + k, mods[k]);
                    else {
                        // Removing the empty string and decrementing the counter, so we can check the rest of the vector.
                        mods.erase(mods.begin() + k);
                        k--;
                    }
                }
                return i + 1;
            }
        }
    }
    return -1;
}


bool checkCommand(string command) {
    int i = 0;
    int count1 = 0;
    int count2 = 0;

    for(i = 0; i < command.length(); i++) {
        if(command[i] == '<') {
            count1++;
        }
        if(command[i] == '>') {
            count2++;
        }
    }
    if(count1 > 1) {
        perror("Error: Multiple input redirect");
        return false;
    }
    if(count2 > 1) {
        perror("Error: Multiple output redirect");
        return false;
    }

    stringstream sstream(command);
    string word;
    vector<string> mods;

    // Clearing out the white space in the string and pushing it onto the instruction vector.
    while(sstream >> word) {
        mods.push_back(word);
    }

    // Checking for incorrect sequences in the command.
    for(i = 1; i < mods.size(); i++) {
        if(mods[i-1] == "|" && mods[i] == "<") {  // Checking for an input redirect after a pipe.
            perror("Error: Input redirect after pipe.");
            return false;
        }
        if(mods[i-1] == ">" && mods[i] == "|") {  // Checking for an output redirect before a pipe.
            perror("Error: Output redirect before pipe.");
            return false;
        }
        if(mods[i-1] == "|" && mods[i] == "|") {  // Checking for no commands between 2 pipes.
            perror("Error: No Command between pipes.");
            return false;
        }
        for(int j = 1; j < mods[i].length(); j++) {
            if(mods[i][j-1] == '|' && mods[i][j] == '<') {  // Checking for an input redirect after a pipe.
                perror("Error: Input redirect after pipe.");
                return false;
            }
            if(mods[i][j-1] == '>' && mods[i][j] == '|') {  // Checking for an output redirect before a pipe.
                perror("Error: Output redirect before pipe.");
                return false;
            }
            if(mods[i][j-1] == '|' && mods[i][j] == '|') {  // Checking for no commands between 2 pipes.
                perror("Error: No Command between pipes.");
                return false;
            }
        }
    }

    return true;
}