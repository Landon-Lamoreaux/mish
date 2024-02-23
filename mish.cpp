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
    vector<string> commands;
    fstream fin;
    int pid;
    std::filesystem::path cwd;

    rl_bind_key('\t', rl_complete);
    using_history();

    if(argv == 2) {
        fin.open(argc[1]);
    }
    else if(argv > 2) {
        cout << "Incorrect Usage. Only 0-1 arguments allowed." << endl;
    }

    while (input != "exit" && !fin.eof())
    {
        cwd = std::filesystem::current_path();

        if(argv == 1) {
            input = readline(("mish:~" + cwd.string() + "> ").c_str());
        }
        else
            getline(fin, input);

        add_history(input.c_str());

        // Killing the program when an exit is requested by the user.
        if(input == "exit" || fin.eof())
            exit(0);

        // Splitting the string on every &.
        commands = splitString(input, "&");

        // Running all the commands in parallel.
        for(i = 0; i < commands.size(); i++)
        {
            boost::trim(commands[i]);
            if (commands[i].empty())
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

void runCommand(string command)
{
    stringstream sstream(command);
    vector<string> mods;
    vector<const char *> words;
    string word;
    int i, j;
    int inputNum;
    vector<int*> piped;

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
                runCommand(pipedCommands[i]);
                perror("Pipe Error");
                exit(0);
            }
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
        int file = open(mods[mods.size() - 1].c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        dup2(file, STDOUT_FILENO);
        dup2(file, STDERR_FILENO);
        mods.pop_back();
    }

    // Input redirection.
    if(checkSetting(command, '<')) {

        inputNum = getOperator(mods, '<');
        if(inputNum != -1) {
            fstream fin;
            fin.open(mods[mods.size() - 1], fstream::in);
            mods.pop_back();
            while(fin >> word) {
                mods.push_back(word);
            }
        }
    }

    char ** modifiers;
    for(i = 0; i < mods.size(); i++) {
        words.push_back(mods[i].c_str());
    }

    words.push_back(nullptr);
    modifiers = const_cast<char **>(&words[0]);

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
        chdir(mods[1].c_str());
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
                        commands.insert(commands.begin() + i + k, mods[k]);
                    else {
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