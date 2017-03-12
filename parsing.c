#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#endif

/*
static int numberOfNodes(mpc_ast_t* tree) {
  if (tree->children_num == 0) {
    return 1; 
  }
  if (tree->children_num >= 1) {
    int total = 1;
    for(int i = 0; i < tree->children_num; i++) {
      total += numberOfNodes(tree->children[i]);
    }
    return total;
  }
  return 0;
}
*/

static long minl(long a, long b) {
  return a > b ? b : a;
}

static long maxl(long a, long b) {
  return a > b ? a : b;
}

/* Use operator string to see which operation to perform */
static long eval_op(long x, char* op, long y) {
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  if (strcmp(op, "%") == 0) { return x % y; }
  if (strcmp(op, "^") == 0) { return powl(x, y); }
  if (strcmp(op, "min") == 0) { return minl(x, y); }
  if (strcmp(op, "max") == 0) { return maxl(x, y); }
  return 0;
}

static long eval(mpc_ast_t* t) {

  /* If tagged as number return it directly. */
  if (strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  /* The operator is always second child. */
  char* op = t->children[1]->contents;

  /* We store the third child in `x` */
  long x = eval(t->children[2]);

  /* Iterate the remaining children and combining. */
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

static void evalAndPrint(char* input, mpc_parser_t* parser) {
  /* Attempt to Parse the user Input */
  mpc_result_t r;
  if (mpc_parse("<stdin>", input, parser, &r)) {
    /* On Success Print the AST */
    long result = eval(r.output);
    printf("%li\n", result);
    //mpc_ast_print(r.output);    //print AST
    //printf("Number of nodes: %d\n", numberOfNodes(r.output)); // print NUmber of nodes
    mpc_ast_delete(r.output);
  } else {
    /* Otherwise Print the Error */
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
  }
}

int main(int argc, char** argv) {
  // Crete some parsers
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  // Define language
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                      \
    number: /-?[0-9]+/ ;                                                   \
    operator: '+' | '-' | '*' | '/' | '%' | '^' | /min/ | /max/ ;          \
    expr: <number> | '(' <operator> <expr>+ ')' ;                          \
    lispy: /^/ <operator> <expr>+ /$/ ;                                    \
    ",
    Number, Operator, Expr, Lispy);


  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while(1) {
    char* input = readline("lispy> ");
    add_history(input);
    evalAndPrint(input, Lispy);
    free(input);
  }

  /* Undefine and Delete our Parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}