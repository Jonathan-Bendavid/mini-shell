#include "userLogin.h"

#define MAX_CRED_LENGTH 50
#define USER_FILE "Shell/users.txt"

int validate_login(const char* username, const char* password) {
    FILE* file = fopen(USER_FILE, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open user database (%s).\n", USER_FILE);
        return 0; 
    }

    char line[MAX_CRED_LENGTH * 2];
    char file_user[MAX_CRED_LENGTH];
    char file_pass[MAX_CRED_LENGTH];
    char* token;

    int result = 0; 

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\r\n")] = 0;

        char line_copy[sizeof(line)];
        strcpy_s(line_copy, sizeof(line_copy), line);

        char* context = NULL;

        token = strtok_s(line_copy, ":", &context);
        if (token != NULL) {
            strcpy_s(file_user, sizeof(file_user), token);
        }
        else continue;

        token = strtok_s(NULL, ":", &context);
        if (token != NULL) {
            strcpy_s(file_pass, sizeof(file_pass), token);
        }
        else continue;

        if (strcmp(username, file_user) == 0 && strcmp(password, file_pass) == 0) {
            result = 1; 
            break;
        }
    }

    fclose(file);
    return result;
}

// Reads a password securely, printing '*' instead of the character.
void read_secure_input(char* buffer, int maxLength) {
    int i = 0;
    char c;

    while (i < maxLength - 1 && (c = (char)_getch()) != '\r' && c != '\n') {
        if (c == '\b') {
            if (i > 0) {
                i--;
                printf("\b \b");
            }
        }
        else {
            buffer[i++] = c;
            printf("*");
        }
    }
    buffer[i] = '\0';
    printf("\n");
}