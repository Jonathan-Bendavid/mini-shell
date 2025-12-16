#pragma once
#include <stdio.h>
#include <string.h>
#include <conio.h> 

#define MAX_CRED_LENGTH 50
#define USER_FILE "FileName" 


// Function Prototypes (Declarations)
// The function to validate credentials against the user file
int validate_login(const char* username, const char* password);

// The function to read the password without displaying characters
void read_secure_input(char* buffer, int maxLength);