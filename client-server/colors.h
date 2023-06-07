/**
 * The below code defines functions to print colored text in the terminal.
 */
#include <stdio.h>

void black()
{
  printf("\033[0;30m");
}

void bold_black()
{
  printf("\033[1;30m");
}

void red()
{
  printf("\033[0;31m");
}

void bold_red()
{
  printf("\033[1;31m");
}

void green()
{
  printf("\033[0;32m");
}

void bold_green()
{
  printf("\033[1;32m");
}

void yellow()
{
  printf("\033[0;33m");
}

void bold_yellow()
{
  printf("\033[1;33m");
}

void blue()
{
  printf("\033[0;34m");
}

void bold_blue()
{
  printf("\033[1;34m");
}

void magenta()
{
  printf("\033[0;35m");
}

void bold_magenta()
{
  printf("\033[1;35m");
}

void cyan()
{
  printf("\033[0;36m");
}

void bold_cyan()
{
  printf("\033[1;36m");
}

void white()
{
  printf("\033[0;37m");
}

void bold_white()
{
  printf("\033[1;37m");
}

void default_color()
{
  printf("\033[0m");
}