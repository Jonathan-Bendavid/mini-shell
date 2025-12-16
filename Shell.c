#include "Headers.h"
#include "UserLogin.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1 

// Function Declarations for shell:
void shell_loop();
char* shell_read_line();
char** shell_parse_line(char* line);
int shell_launch(char** args);
int shell_execute(char** args);

/* Function Declarations for helper functions */
int execute_child_process_direct(char* command_line, HANDLE hStdOutput);
int handle_io_write(char** args, char* full_command_line, int redirect_pos);
int handle_io_append(char** args, char* full_command_line, int redirect_pos);

// Function Declarations for shell built-in commands:
int shell_cd(char** args);
int shell_dir(char** args);
int shell_pwd(char** args);
int shell_type(char** args);
int shell_echo(char** args);
int shell_help(char** args);
char* program_descriptions(char* name);
int shell_exit(char** args);

// List of built in commands, followed by their functions.
char* builtin_str[] = {
  "cd",
  "dir",
  "pwd",
  "type",
  "echo",
  "help",
  "exit"
};

int (*builtin_func[]) (char**) = {
  &shell_cd,
  &shell_dir,
  &shell_pwd,
  &shell_type,
  &shell_echo,
  &shell_help,
  &shell_exit
};

int shell_num_builtins() {
	return sizeof(builtin_str) / sizeof(char*);
}

int main(int argc, char** argv) {

	shell_cd(NULL); // Start in home directory

	char username[MAX_CRED_LENGTH];
	char password[MAX_CRED_LENGTH];
	int attempts = 0;
	const int MAX_ATTEMPTS = 3;

	printf("\nWelcome to the Shell\n\n");

	while (attempts < MAX_ATTEMPTS) {
		printf("Username: ");
		fgets(username, sizeof(username), stdin);
		username[strcspn(username, "\r\n")] = 0;
		printf("Password: ");
		read_secure_input(password, sizeof(password));
		printf("\n");
		if (validate_login(username, password)) {
			printf("Login successful!\n\n");
			break;
		}
		else {
			printf("Invalid username or password. Please try again.\n\n");
			attempts++;
		}
	}
	if (attempts == MAX_ATTEMPTS) {
		printf("Maximum login attempts exceeded. Exiting.\n");
		return EXIT_FAILURE;
	}

	printf("Type 'help' to see available commands.\n\n");

	shell_loop();

	return EXIT_SUCCESS;
}

void shell_loop() {
	char* line;
	char** args;
	int status;
	char buffer[MAX_PATH];

	do {
		if (GetCurrentDirectoryA(MAX_PATH, buffer)) {
			printf("%s", buffer);
		}
		printf("> ");
		line = shell_read_line();
		args = shell_parse_line(line);
		status = shell_execute(args);
		free(line);
		free(args);
	} while (status);
}

#define shell_ln_BUFSIZE 1024
char* shell_read_line(void) {
	int bufSize = shell_ln_BUFSIZE;
	int position = 0;
	char* buffer = malloc(sizeof(char) * bufSize);
	int c;

	if (!buffer) {
		fprintf(stderr, "Shell: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		c = getchar();
		if (c == EOF || c == '\n') {
			buffer[position] = '\0';
			return buffer;
		}

		if (position >= bufSize - 1) {
			bufSize += shell_ln_BUFSIZE;
			buffer = realloc(buffer, bufSize);
			if (!buffer) {
				fprintf(stderr, "Shell: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		buffer[position] = c;
		position++;
	}
}

#define shell_TOK_BUFSIZE 64
#define SHELL_TOK_DELIMETERS " \t\r\n\a"
char** shell_parse_line(char* line) {

	int bufSize = shell_TOK_BUFSIZE, position = 0;
	char** tokens = malloc(bufSize * sizeof(char*));
	char* token;
	char* context = NULL;

	if (!tokens) {
		fprintf(stderr, "Shell: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok_s(line, SHELL_TOK_DELIMETERS, &context);
	while (token != NULL) {
		tokens[position] = token;
		position++;
		if (position >= bufSize) {
			bufSize += shell_TOK_BUFSIZE;
			tokens = realloc(tokens, bufSize * sizeof(char*));
			if (!tokens) {
				fprintf(stderr, "Shell: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
		token = strtok_s(NULL, SHELL_TOK_DELIMETERS, &context);
	}
	tokens[position] = NULL;
	return tokens;
}

#define MAX_COMMAND_LENGTH 1024
int execute_builtin_with_redirect(char** args, int redirect_pos, HANDLE hFile) {
	char output[8192] = { 0 };

	if (strcmp(args[0], "echo") == 0) {
		for (int i = 1; args[i] != NULL && i < redirect_pos; i++) {
			strcat_s(output, sizeof(output), args[i]);
			if (args[i + 1] != NULL && i + 1 < redirect_pos) {
				strcat_s(output, sizeof(output), " ");
			}
		}
		strcat_s(output, sizeof(output), "\r\n");
	}
	else if (strcmp(args[0], "pwd") == 0) {
		char buffer[MAX_PATH];
		if (GetCurrentDirectoryA(MAX_PATH, buffer)) {
			snprintf(output, sizeof(output), "%s\r\n", buffer);
		}
	}
	else if (strcmp(args[0], "dir") == 0) {
		WIN32_FIND_DATAA findFileData;
		HANDLE hFind;
		char* path = (args[1] == NULL || strcmp(args[1], ">") == 0 || strcmp(args[1], ">>") == 0) ? "." : args[1];

		char searchPath[MAX_COMMAND_LENGTH];
		snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

		hFind = FindFirstFileA(searchPath, &findFileData);

		if (hFind == INVALID_HANDLE_VALUE) {
			snprintf(output, sizeof(output),
				"shell: dir: cannot access '%s': No such file or directory\r\n", path);
		}
		else {
			do {
				if (strcmp(findFileData.cFileName, ".") != 0 &&
					strcmp(findFileData.cFileName, "..") != 0) {
					strcat_s(output, sizeof(output), findFileData.cFileName);
					strcat_s(output, sizeof(output), "\r\n");
				}
			} while (FindNextFileA(hFind, &findFileData) != 0);
			FindClose(hFind);
		}
	}
	else if (strcmp(args[0], "type") == 0) {
		if (args[1] == NULL || redirect_pos <= 1) {
			snprintf(output, sizeof(output), "shell: type: missing operand\r\n");
		}
		else {
			FILE* file;
			errno_t err = fopen_s(&file, args[1], "r");
			if (err != 0 || file == NULL) {
				snprintf(output, sizeof(output),
					"shell: type: cannot open file '%s'\r\n", args[1]);
			}
			else {
				size_t len = 0;
				int ch;
				while ((ch = fgetc(file)) != EOF && len < sizeof(output) - 1) {
					output[len++] = ch;
				}
				output[len] = '\0';
				fclose(file);
			}
		}
	}
	else if (strcmp(args[0], "help") == 0) {
		if (args[1] != NULL && args[1][0] != '\0' && strcmp(args[1], ">") != 0 && strcmp(args[1], ">>") != 0) {
			snprintf(output, sizeof(output), "%s", program_descriptions(args[1]));
		}
		else {
			strcat_s(output, sizeof(output),
				"Type program names and arguments, and hit enter.\r\n");
			strcat_s(output, sizeof(output), "The following are built in:\r\n");
			for (int i = 0; i < shell_num_builtins(); i++) {
				strcat_s(output, sizeof(output), "  ");
				strcat_s(output, sizeof(output), builtin_str[i]);
				strcat_s(output, sizeof(output), "\r\n");
			}
			strcat_s(output, sizeof(output),
				"\r\nUse the help command with a program name for more information on the program.\r\n");
		}
	}
	else {
		snprintf(output, sizeof(output),
			"shell: redirection not supported for '%s'\r\n", args[0]);
	}

	DWORD bytes_written;
	BOOL success = WriteFile(hFile, output, (DWORD)strlen(output), &bytes_written, NULL);

	if (!success) {
		fprintf(stderr, "shell: write error: %lu\n", GetLastError());
		return 1;
	}

	return 1;
}

int shell_launch(char** args) {
	char command_line[MAX_COMMAND_LENGTH] = { 0 };
	char full_path_buffer[MAX_PATH];
	LPSTR lpFilePart = NULL;
	DWORD search_result;
	char* command = args[0];

	if (args[0] == NULL || args[0][0] == '\0') {
		return 1;
	}

	// Find redirection operator position
	int redirect_pos = -1;
	for (int i = 1; args[i] != NULL; i++) {
		if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0 || strcmp(args[i], "<") == 0) {
			redirect_pos = i;
			break;
		}
	}

	// Check if this is a built-in command
	int is_builtin = 0;
	for (int i = 0; i < shell_num_builtins(); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			is_builtin = 1;
			break;
		}
	}

	// If it's a built-in with redirection, handle it specially
	if (is_builtin && redirect_pos != -1) {
		if (args[redirect_pos + 1] == NULL) {
			fprintf(stderr, "shell: redirection error: missing filename.\n");
			return 1;
		}

		const char* filename = args[redirect_pos + 1];
		HANDLE hFile = INVALID_HANDLE_VALUE;

		if (strcmp(args[redirect_pos], ">") == 0) {
			hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		else if (strcmp(args[redirect_pos], ">>") == 0) {
			hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				SetFilePointer(hFile, 0, NULL, FILE_END);
			}
		}

		if (hFile == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "shell: redirection error: Failed to open file '%s'. Error %lu.\n",
				filename, GetLastError());
			return 1;
		}

		int result = execute_builtin_with_redirect(args, redirect_pos, hFile);
		CloseHandle(hFile);
		return result;
	}

	// Not a built-in, so try to launch as external command
	search_result = SearchPathA(NULL, command, NULL, MAX_PATH, full_path_buffer, &lpFilePart);

	if (search_result > 0 && search_result < MAX_PATH) {
		command = full_path_buffer;
	}

	// Build command line
	strcat_s(command_line, MAX_COMMAND_LENGTH, command);

	int limit = (redirect_pos != -1) ? redirect_pos : INT_MAX;
	for (int i = 1; args[i] != NULL && i < limit; i++) {
		strcat_s(command_line, MAX_COMMAND_LENGTH, " ");
		strcat_s(command_line, MAX_COMMAND_LENGTH, args[i]);
	}

	// Handle redirection for external commands
	if (redirect_pos != -1) {
		if (strcmp(args[redirect_pos], ">") == 0) {
			return handle_io_write(args, command_line, redirect_pos + 1);
		}
		else if (strcmp(args[redirect_pos], ">>") == 0) {
			return handle_io_append(args, command_line, redirect_pos + 1);
		}
		else if (strcmp(args[redirect_pos], "<") == 0) {
			fprintf(stderr, "shell: input redirection not yet implemented\n");
			return 1;
		}
	}

	return execute_child_process_direct(command_line, GetStdHandle(STD_OUTPUT_HANDLE));
}

int execute_child_process_direct(char* command_line, HANDLE hStdOutput) {
	// Only print to console if output is going to console
	if (hStdOutput == GetStdHandle(STD_OUTPUT_HANDLE)) {
		printf("%s\n\n", command_line);
	}

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	DWORD exit_code = 0;

	char writable_command_line[MAX_COMMAND_LENGTH];
	strcpy_s(writable_command_line, MAX_COMMAND_LENGTH, command_line);

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = hStdOutput;
	si.hStdError = hStdOutput;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcessA(NULL, writable_command_line, NULL, NULL, TRUE, 0,
		NULL, NULL, &si, &pi)) {
		fprintf(stderr, "shell: command failed (%lu)\n", GetLastError());
		return 1;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &exit_code);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return 1;
}

int shell_execute(char** args)
{
	if (args[0] == NULL) {
		return 1;
	}

	// Check for redirection operators
	int has_redirection = 0;
	for (int j = 0; args[j] != NULL; j++) {
		if (strcmp(args[j], ">") == 0 || strcmp(args[j], ">>") == 0 || strcmp(args[j], "<") == 0) {
			has_redirection = 1;
			break;
		}
	}

	// If there's redirection, always use shell_launch to handle it
	if (has_redirection) {
		return shell_launch(args);
	}

	// Otherwise, check for built-ins
	for (int i = 0; i < shell_num_builtins(); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			return (*builtin_func[i])(args);
		}
	}
	return shell_launch(args);
}

int shell_cd(char** args)
{
	char* target_path;

	if (args == NULL || args[1] == NULL) {
		target_path = "C:\\";
	}
	else {
		target_path = args[1];
	}
	if (SetCurrentDirectoryA(target_path) == 0) {
		fprintf(stderr, "shell: cd: failed to change directory to %s.\n", target_path);
	}
	return 1;
}

int shell_dir(char** args)
{
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind;
	char* path;

	if (args[1] == NULL) {
		path = ".";
	}
	else {
		path = args[1];
	}

	char searchPath[MAX_COMMAND_LENGTH];
	snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

	hFind = FindFirstFileA(searchPath, &findFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "shell: dir: cannot access '%s': No such file or directory\n", path);
		return 1;
	}

	do {
		if (strcmp(findFileData.cFileName, ".") != 0 &&
			strcmp(findFileData.cFileName, "..") != 0)
		{
			printf("%s\n", findFileData.cFileName);
		}
	} while (FindNextFileA(hFind, &findFileData) != 0);

	FindClose(hFind);

	return 1;
}

int shell_type(char** args) {
	if (args[1] == NULL) {
		fprintf(stderr, "shell: type: missing operand\n");
		return 1;
	}

	FILE* file;
	errno_t err = fopen_s(&file, args[1], "r");
	if (err != 0 || file == NULL) {
		fprintf(stderr, "shell: type: cannot open file '%s'\n", args[1]);
		return 1;
	}

	int ch;
	int last_was_newline = 0;
	while ((ch = fgetc(file)) != EOF) {
		putchar(ch);
		last_was_newline = (ch == '\n');
	}

	if (!last_was_newline && ftell(file) > 0) {
		printf("\n");
	}

	fclose(file);
	return 1;
}

int shell_pwd(char** args)
{
	char buffer[MAX_PATH];
	if (GetCurrentDirectoryA(MAX_PATH, buffer)) {
		printf("%s\n", buffer);
	}
	else {
		fprintf(stderr, "shell: pwd: error retrieving current directory. Error code: %lu\n", GetLastError());
	}
	return 1;
}

bool check_args(char* arg) {
	if (arg == NULL || strcmp(arg, ">") == 0 || strcmp(arg, ">>") == 0 || strcmp(arg, "<") == 0) {
		return false;
	}
	return true;
}

int shell_echo(char** args) {
	for (int i = 1; check_args(args[i]); i++) {
		printf("%s", args[i]);
		if (check_args(args[i + 1])) {
			printf(" ");
		}
	}
	printf("\n");
	return 1;
}

int shell_help(char** args)
{
	if (args[1] != NULL && args[1][0] != '\0') {
		printf("%s", program_descriptions(args[1]));
		return 1;
	}

	printf("Type program names and arguments, and hit enter.\n");
	printf("The following are built in:\n");

	for (int i = 0; i < shell_num_builtins(); i++) {
		printf("  %s\n", builtin_str[i]);
	}

	printf("\nUse the help command with a program name for more information on the program.\n");
	return 1;
}

char* program_descriptions(char* name) {
	if (strcmp(name, "cd") == 0) {
		return "cd [directory]: Change the current directory to 'directory'. If no directory is provided, changes to the root directory C:\\.\n";
	}
	else if (strcmp(name, "dir") == 0) {
		return "dir [directory]: List the contents of 'directory'. If no directory is provided, lists the contents of the current directory.\n";
	}
	else if (strcmp(name, "pwd") == 0) {
		return "pwd: Print the current working directory.\n";
	}
	else if (strcmp(name, "type") == 0) {
		return "type [file]: Display the contents of 'file'.\n";
	}
	else if (strcmp(name, "echo") == 0) {
		return "echo [text]: Display a line of text.\n";
	}
	else if (strcmp(name, "help") == 0) {
		return "help [command]: Display information about builtin commands.\n";
	}
	else if (strcmp(name, "exit") == 0) {
		return "exit: Exit the shell.\n";
	}
	else {
		static char error_message[100];
		snprintf(error_message, sizeof(error_message), "No help available for '%s'.\n", name);
		return error_message;
	}
}

int handle_io_write(char** args, char* full_command_line, int redirect_pos) {
	if (args[redirect_pos] == NULL) {
		fprintf(stderr, "shell: redirection error: missing filename after '>'.\n");
		return 1;
	}

	const char* filename = args[redirect_pos];

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "shell: redirection error: Failed to open file '%s'. Error %lu.\n",
			filename, GetLastError());
		return 1;
	}

	int result = execute_child_process_direct(full_command_line, hFile);

	CloseHandle(hFile);
	return result;
}

int handle_io_append(char** args, char* full_command_line, int redirect_pos) {
	if (args[redirect_pos] == NULL) {
		fprintf(stderr, "shell: redirection error: missing filename after '>>'.\n");
		return 1;
	}

	const char* filename = args[redirect_pos];

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "shell: redirection error: Failed to open file '%s'. Error %lu.\n",
			filename, GetLastError());
		return 1;
	}

	SetFilePointer(hFile, 0, NULL, FILE_END);

	int result = execute_child_process_direct(full_command_line, hFile);

	CloseHandle(hFile);
	return result;
}

int shell_exit(char** args)
{
	return 0;
}