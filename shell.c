#include <stdio.h>
#include "tokens.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#define MAX_LINE 256

/**
 * @brief Executes a single command.
 *
 * @param args Full set of arguments, including the program name.
 * @return pid_t process id of the new launched process
 */
pid_t launch(char **args)
{
  pid_t pid = fork();
  if (pid == 0)
  {
    execvp(args[0], args);
    printf("Child: Should never get here (%s)\n", args[0]);
    exit(1);
  }
  else
  {
    return pid;
  }
}

/**
 * @brief Executes a command or combination of commands separated by tokens like ">" or "|".
 *
 * @param tokens list of tokens to execute
 */
void execute(char **tokens)
{
  pid_t pid = launch(tokens);
  waitpid(pid, NULL, 0);
}

/**
 * @brief Executes a line of input as tokens.
 *
 * @param tokens list of tokens (input to the shell)
 */
void execute_input(char **tokens)
{
  char *current[MAX_LINE]; // current command, ending at ; or \0
  int token_idx = 0;
  int current_idx = 0;
  while (1)
  {
    if (tokens[token_idx] == NULL || *tokens[token_idx] == ';')
    {
      current[current_idx] = NULL;
      current_idx = 0;
      execute(current);
      if (tokens[token_idx] == NULL)
      {
        break;
      }
    }
    else
    {
      current[current_idx] = tokens[token_idx];
      current_idx++;
    }
    token_idx++;
  }
}

/**
 * @brief Runs a loop to read and execute commands.
 *
 */
void loop()
{
  while (1)
  {
    char input[MAX_LINE];
    printf("shell $ ");
    char *eof = fgets(input, MAX_LINE, stdin);
    if (eof == NULL || strcmp(input, "exit\n") == 0)
    {
      break;
    }
    char **tokens = get_tokens(input);
    execute_input(tokens);
    free_tokens(tokens);
  }
}

/**
 * @brief Execute mini-shell.
 */
int main(int argc, char **argv)
{
  printf("Welcome to mini-shell.\n");
  loop();
  printf("Bye bye.\n");
  return 0;
}
