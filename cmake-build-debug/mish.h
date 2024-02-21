#include <string>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

using namespace std;

#ifndef MISH_MISH_H
#define MISH_MISH_H

vector<string> splitString(string str, string delimiters);

void runCommand(string command);

bool checkSetting(string str, char findee);

int getOperator(vector<string> &commands, char op);

bool executeBuiltIns(string command);

#endif //MISH_MISH_H
