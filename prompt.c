#include <stdio.h>

#define BUFFER_SIZE 2048
static char userInputBuffer[BUFFER_SIZE];

void outputPrompt() {
    fputs("lispy> ", stdout);
}

void readInput() {
    fgets(userInputBuffer, BUFFER_SIZE, stdin);
}

void echoInput() {
    printf("No, you're a %s", userInputBuffer);
}

int main(int argc, char** argv) {
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1) {
        outputPrompt();
        readInput();
        echoInput();
    }
    return 0;
}