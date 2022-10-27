#include <stdio.h>
#include "tokens.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#define MAX_LINE 256

char **prev_line;

/**
 * @brief Empty an array by setting its contents to null.
 *
 * @param arr The array to empty.
 * @param size The size of the array.
 */
void empty_arr(char **arr, int size)
{
  for (int i = 0; i < size; i++)
  {
    arr[i] = NULL;
  }
}

int execute_line(char **tokens);
void sub_prev(char **tokens);

/**
 * @brief Execute a non-builtin command and accounts for redirection.
 *
 * @param tokens all tokens in the command line
 * @param start start of the relevant command
 * @param end end of the relevant command
 */
void execute(char **tokens, int start, int end)
{
  char *cmd[MAX_LINE] = {NULL};
  int cmd_idx = 0;
  for (int i = start; i < end; i++)
  {
    if (strcmp(tokens[i], ">") == 0)
    {
      int fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(fd, 1);
      close(fd);
      i++;
    }
    else if (strcmp(tokens[i], "<") == 0)
    {
      int fd = open(tokens[i + 1], O_RDONLY);
      dup2(fd, 0);
      close(fd);
      i++;
    }
    else
    {
      cmd[cmd_idx] = tokens[i];
      cmd_idx++;
    }
  }
  cmd[cmd_idx] = NULL;
  if (execvp(cmd[0], cmd) == -1)
  {
    printf("%s: command not found\n", cmd[0]);
  }
  exit(1);
}

/**
 * @brief Changes working directory.
 *
 * @param tokens all tokens in the command
 * @return int whether to exit the shell
 */
void cd(char **tokens)
{
  if (tokens[1] == NULL)
  {
    chdir(getenv("HOME"));
  }
  else
  {
    chdir(tokens[1]);
  }
}

/**
 * @brief Executes a script.
 *
 * @param tokens tokens in the command
 */
int source(char **tokens)
{
  if (tokens[1] == NULL)
  {
    printf("source: must provide file\n");
    return 0;
  }
  FILE *fp = fopen(tokens[1], "r");
  if (fp == NULL)
  {
    printf("source: %s: No such file or directory\n", tokens[1]);
    return 0;
  }
  char line[MAX_LINE];
  while (fgets(line, MAX_LINE, fp) != NULL)
  {
    char **tokens = get_tokens(line);
    sub_prev(tokens);
    if (execute_line(prev_line) == 1)
    {
      return 1;
    }
    free_tokens(tokens);
  }
  return 0;
}

/**
 * @brief Executes a line of input as tokens.
 *
 * @param tokens list of tokens (input to the shell)
 * @param end index of the end of the command
 * @return int whether to exit the shell
 */
int execute_command(char **tokens, int end)
{
  int token_idx = end - 1;
  if (end > 0)
  {
    if (strcmp(tokens[0], "exit") == 0)
    {
      return 1;
    }
    else if (strcmp(tokens[0], "cd") == 0)
    {
      cd(tokens);
      return 0;
    }
    else if (strcmp(tokens[0], "source") == 0)
    {
      return source(tokens);
    }
    else if (strcmp(tokens[0], "help") == 0)
    {
      printf("cd: change directory, source: execute a script, exit: exit the shell, prev: execute the previous command, help: display this message\n");
      return 0;
    }
  }
  while (1)
  {
    if (token_idx == 0)
    {
      // just run command up to len
      pid_t pid = fork();
      if (pid == 0)
      {
        execute(tokens, 0, end);
      }
      else
      {
        waitpid(pid, NULL, 0);
      }
      return 0;
    }
    if (strcmp(tokens[token_idx], "|") == 0)
    {
      // run command from token_idx to len, pipe results to next recursive command
      pid_t pid_a = fork();
      if (pid_a == 0)
      {
        // create pipe
        int pipefd[2];
        pipe(pipefd);
        pid_t pid_b = fork();
        if (pid_b == 0)
        {
          // close read end
          close(pipefd[0]);
          // redirect stdout to write end
          dup2(pipefd[1], 1);
          // close write end
          close(pipefd[1]);
          // execute command
          execute_command(tokens, token_idx);
          exit(0);
        }
        else
        {
          // child a
          // close write end
          close(pipefd[1]);
          // redirect stdin to read end
          dup2(pipefd[0], 0);
          // close read end
          close(pipefd[0]);
          // execute command
          waitpid(pid_b, NULL, 0);
          execute(tokens, token_idx + 1, end);
          exit(0);
        }
      }
      else
      {
        waitpid(pid_a, NULL, 0);
      }
      return 0;
    }
    token_idx--;
  }
  return 0;
}

/**
 * @brief Execute a line of input by separating by semicolons and executing each command.
 *
 * @param tokens
 * @return int whether to exit the shell
 */
int execute_line(char **tokens)
{
  int i = 0;
  char *cmd[MAX_LINE] = {NULL};
  int cmd_idx = 0;
  while (1)
  {
    if (tokens[i] == NULL || strcmp(tokens[i], ";") == 0)
    {
      if (execute_command(cmd, cmd_idx) == 1)
      {
        return 1;
      }
      cmd_idx = 0;
      empty_arr(cmd, MAX_LINE);
      if (tokens[i] == NULL)
      {
        break;
      }
    }
    else
    {
      cmd[cmd_idx] = tokens[i];
      cmd_idx++;
    }
    i++;
  }
  return 0;
}

/**
 * @brief Substitute the previous command in for the "prev" token.
 *
 * @param tokens
 */
void sub_prev(char **tokens)
{
  // allocate temp array of strings
  char **new = calloc(MAX_LINE, sizeof(char *));
  for (int i = 0; i < MAX_LINE; i++)
  {
    new[i] = malloc(sizeof(char *));
  }

  // copy tokens to new array, substituting for temp
  int token_idx = 0;
  int new_idx = 0;
  while (tokens[token_idx] != NULL && new_idx < MAX_LINE - 1)
  {
    if (strcmp(tokens[token_idx], "prev") == 0)
    {
      // copy prev command into new
      int prev_idx = 0;
      while (prev_line[prev_idx] != NULL && new_idx < MAX_LINE - 1)
      {
        strcpy(new[new_idx], prev_line[prev_idx]);
        prev_idx++;
        new_idx++;
      }
    }
    else
    {
      strcpy(new[new_idx], tokens[token_idx]);
      new_idx++;
    }
    token_idx++;
  }
  new[new_idx] = NULL;

  // free the strings in prev_line
  int prev_idx = 0;
  while (prev_line[prev_idx] != NULL)
  {
    free(prev_line[prev_idx]);
    prev_idx++;
  }

  prev_line = new;
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
    if (eof == NULL)
    {
      break;
    }
    char **tokens = get_tokens(input);
    sub_prev(tokens);
    if (execute_line(prev_line) == 1)
    {
      break;
    }
    free_tokens(tokens);
  }
}

/**
 * @brief Execute mini-shell.
 */
int main(int argc, char **argv)
{
  prev_line = calloc(MAX_LINE, sizeof(char *));
  printf("Welcome to mini-shell.\n");
  loop();
  printf("Bye bye.\n");
  free(prev_line);
  return 0;
}
