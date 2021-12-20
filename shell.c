#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INPUT_SIZE 500 //max length of an entire line of input
#define NUM_TOKENS 50  //number of tokens in a single line of input

char current_dir[500]; //holds the current directory

//SHELL FUNCTIONS-----
void split_tokens(char *dest[NUM_TOKENS], char src[INPUT_SIZE]);
void exec_commands(char *tokens[NUM_TOKENS]);
void start_process(int in, int out, char *command[INPUT_SIZE]);
void do_redirection(char **tokenArray);
//--------------------------

void breakout_run(); //run the Breakout game

int main()
{
	char input[INPUT_SIZE];			   //"raw" line of input
	char *tokens[NUM_TOKENS] = {NULL}; //array of whitespace-separated tokens
	getcwd(current_dir, INPUT_SIZE);

	while (1)
	{
		printf("\x1B[1;31m%s>\x1B[0m", current_dir); //print current directoryy

		fgets(input, INPUT_SIZE, stdin); //get input
		split_tokens(tokens, input);     //split input string into tokens
		exec_commands(tokens);           //execute commands
	}
}

/***
 * Take a "raw" line of input in src and split it into an array of whitespace-separated
 * tokens. The array of tokens is returned in dest
 ***/
void split_tokens(char *dest[NUM_TOKENS], char src[INPUT_SIZE])
{
	int inQuotes = 0;		//used to process quoted arguments as a single token
	char token[INPUT_SIZE]; //array to hold each individual token
	int src_idx = 0,		//index into src
		dst_idx = 0,		//index into dest
		token_idx = 0;		//index into token

	for (int i = 0; i < NUM_TOKENS; i++) //clear out dest
		if (dest[i] != NULL)
		{
			free(dest[i]);
			dest[i] = NULL;
		}

	while (src[src_idx] != '\n') //copy tokens to dest
	{
		if (src[src_idx] == '"')
		{
			inQuotes = 1 - inQuotes;
		}
		else if (src[src_idx] == ' ' && inQuotes == 0) //end of token
		{
			if (strlen(token) == 0) //if token is just empty space, do nothing
			{
			}
			else //otherwise, copy token to dest
			{
				token[token_idx++] = '\0';
				dest[dst_idx] = malloc(token_idx * sizeof(char));
				strcpy(dest[dst_idx], token);
				dst_idx++;
			}

			token_idx = 0; //reset token array to start copying new token
			token[0] = '\0';
		}
		else //middle of token, just copy character
		{
			token[token_idx] = src[src_idx];
			token_idx++;
		}

		src_idx++;
	}
	token[token_idx] = '\0';
	dest[dst_idx] = malloc(token_idx * sizeof(char));
	strcpy(dest[dst_idx], token);
	dest[++dst_idx] = NULL;
}

/***
 * Execute the command represented by an array of tokens. Handles pipes.
 ***/
void exec_commands(char *tokens[NUM_TOKENS])
{
	//check for "built in" command (cd, breakout, or exit)
	if (strcmp(tokens[0], "cd") == 0)
	{
		chdir(tokens[1]);
		getcwd(current_dir, INPUT_SIZE);
		return;
	}
	if(strcmp(tokens[0], "breakout") == 0){
		breakout_run();
		return;
	}
	if (strcmp(tokens[0], "exit") == 0)
	{
		exit(0);
	}

	//set up piping
	int tkn = 0;             //index into tokens array
	int num_processes = 0;   //number of processes separated by pipes
	char **process = tokens; //string for a single process
	while(tokens[tkn] != NULL){  //replace "|" with NULL to separate processes
		if(strcmp(tokens[tkn], "|") == 0){
			free(tokens[tkn]);
			tokens[tkn] = NULL;
			num_processes++;
		}
		tkn++;
	}
	num_processes++;
	tkn = 0;

	int pipe_fd[2];			  //the pipe
	int input = STDIN_FILENO; //first process gets input from stdin

	//execute the commands
	for (int i = 0; i < num_processes - 1; i++)
	{
		if (pipe(pipe_fd) != 0)
		{
			fputs("Pipe error.\n", stdout);
			exit(1);
		}

		start_process(input, pipe_fd[1], process);

		close(pipe_fd[1]);
		input = pipe_fd[0];

		//advance process pointer to next process
		while(tokens[tkn] != NULL){
			tkn++;
			process++;
		}
		tkn++;
		process++;
	}

	start_process(input, STDOUT_FILENO, process); //last process in the pipeline*/
}

/***
 * Fork and start a new process with the specified input and output streams, and replace the new
 * process image with that of the command specified in the command array. Also handles input
 * and output redirection via the '<' and '>' symbols
 ***/
void start_process(int in, int out, char *command[NUM_TOKENS])
{
	int status;
	pid_t child_pid = fork();

	if (child_pid == -1)
	{
		fputs("Error with forking.\n", stdout);
		exit(1);
	}

	if (child_pid == 0) //if this process is the child
	{
		if (in != STDIN_FILENO)
		{
			dup2(in, STDIN_FILENO);
			close(in);
		}

		if (out != STDOUT_FILENO)
		{
			dup2(out, STDOUT_FILENO);
			close(out);
		}

		do_redirection(command); //process input and output redirection (< >)

		if (execvp(command[0], command) < 0)
		{
			fputs("Error executing command.\n", stdout);
			exit(1);
		}
	}

	else //if this process is the parent
	{
		waitpid(child_pid, &status, 0);
	}
}

/***
 * Process input and output redirection in an array of tokens.
 * Redirect symbol (> or <) and file name must be separated by whitespace
 ***/
void do_redirection(char **tokenArray)
{
	char input_file[INPUT_SIZE] = {'\0'}, output_file[INPUT_SIZE] = {'\0'};
	int fd_0, fd_1;
	int j;

	for (int i = 0; tokenArray[i] != NULL; i++)
	{
		if (strcmp(tokenArray[i], "<") == 0) //input redirection
		{
			strcpy(input_file, tokenArray[i + 1]);
			for (j = i + 2; tokenArray[j] != NULL; j++) //shift array so that these tokens are not interpreted as arguments
			{
				tokenArray[j - 2] = tokenArray[j];
			}
			tokenArray[j - 2] = NULL;
			i--;
		}
		if (strcmp(tokenArray[i], ">") == 0) //output redirection
		{
			strcpy(output_file, tokenArray[i + 1]);
			for (j = i + 2; tokenArray[j] != NULL; j++) //shift array so that these tokens are not interpreted as arguments
			{
				tokenArray[j - 2] = tokenArray[j];
			}
			tokenArray[j - 2] = NULL;
			i--;
		}
	}

	if (strlen(input_file) > 0) //do input redirection
	{
		fd_0 = open(input_file, O_RDONLY);
		if (fd_0 == -1)
		{
			fputs("Error opening input file.", stdout);
			exit(1);
		}
		dup2(fd_0, STDIN_FILENO);
		close(fd_0);
	}

	if (strlen(output_file) > 0) //do output redirection
	{
		fd_1 = open(output_file, O_CREAT | O_WRONLY, 0777);
		if (fd_1 == -1)
		{
			fputs("Error opening output file.", stdout);
			exit(1);
		}
		dup2(fd_1, STDOUT_FILENO);
		close(fd_1);
	}
}