#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

char buffer[2048];

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

/* Create Enumeration of Possible lispval Types */
enum { LISPVAL_NUM, LISPVAL_ERR, LISPVAL_SYM, LISPVAL_SEXPR };

/* Declare New lisp value Struct */
typedef struct lispval {
  int type;

  long num;
  char* err;
  char* sym;

  int count;
  struct lispval** cell;
} lispval;

/* Construct a pointer to a new Number lispval */
lispval* lispval_num(long x) {
  lispval* v = malloc(sizeof(lispval));
  v->type = LISPVAL_NUM;
  v->num = x;
  return v;
}

/* Construct a pointer to a new Error lispval */
lispval* lispval_err(char* m) {
  lispval* v = malloc(sizeof(lispval));
  v->type = LISPVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

/* Construct a pointer to a new Symbol lispval */
lispval* lispval_sym(char* s) {
  lispval* v = malloc(sizeof(lispval));
  v->type = LISPVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* Construct a pointer to a new empty Sexpr lispval */
lispval* lispval_sexpr(void) {
  lispval* v = malloc(sizeof(lispval));
  v->type = LISPVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lispval_del(lispval* v) {

  switch (v->type) {
    /* Do nothing special for number type */
    case LISPVAL_NUM: break;

    /* For Err or Sym free the string data */
    case LISPVAL_ERR: free(v->err); break;
    case LISPVAL_SYM: free(v->sym); break;

    /* If Sexpr then delete all elements inside */
    case LISPVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lispval_del(v->cell[i]);
      }
      /* Also free the memory allocated to contain the pointers */
      free(v->cell);
    break;
  }

  /* Free the memory allocated for the "lispval" struct itself */
  free(v);
}

lispval* lispval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lispval_num(x) : lispval_err("invalid number");
}

lispval* lispval_add(lispval* v, lispval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lispval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lispval* lispval_read(mpc_ast_t* t) {

  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lispval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lispval_sym(t->contents); }

  /* If root (>) or sexpr then create empty list */
  lispval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lispval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lispval_sexpr(); }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lispval_add(x, lispval_read(t->children[i]));
  }

  return x;
}

void lispval_print(lispval* v);

void lispval_expr_print(lispval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {

    /* Print Value contained within */
    lispval_print(v->cell[i]);

    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lispval_print(lispval* v) {
  switch (v->type) {
    case LISPVAL_NUM:   printf("%li", v->num); break;
    case LISPVAL_ERR:   printf("Error: %s", v->err); break;
    case LISPVAL_SYM:   printf("%s", v->sym); break;
    case LISPVAL_SEXPR: lispval_expr_print(v, '(', ')'); break;
  }
}

void lispval_println(lispval* v) { lispval_print(v); putchar('\n'); }


lispval* lispval_pop(lispval* v, int i) {
  /* Find the item at "i" */
  lispval* x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i+1],
    sizeof(lispval*) * (v->count-i-1));

  /* Decrease the count of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lispval*) * v->count);
  return x;
}

lispval* lispval_take(lispval* v, int i) {
  lispval* x = lispval_pop(v, i);
  lispval_del(v);
  return x;
}

long minl(long a, long b) {
  return a > b ? b : a;
}

long maxl(long a, long b) {
  return a > b ? a : b;
}

lispval* builtin_op(lispval* a, char* op) {

  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LISPVAL_NUM) {
      lispval_del(a);
      return lispval_err("Cannot operate on non-number!");
    }
  }

  /* Pop the first element */
  lispval* x = lispval_pop(a, 0);

  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  /* While there are still elements remaining */
  while (a->count > 0) {

    /* Pop the next element */
    lispval* y = lispval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lispval_del(x); lispval_del(y);
        x = lispval_err("Division By Zero!"); break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0) { x->num %= y->num; }
    if (strcmp(op, "^") == 0) { x->num = powl(x->num, y->num); }
    if (strcmp(op, "min") == 0) { x->num = minl(x->num, y->num); }
    if (strcmp(op, "max") == 0) { x->num = maxl(x->num, y->num); }

    lispval_del(y);
  }

  lispval_del(a); return x;
}

lispval* lispval_eval(lispval* v);

lispval* lispval_eval_sexpr(lispval* v) {

  /* Evaluate Children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lispval_eval(v->cell[i]);
  }

  /* Error Checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LISPVAL_ERR) { return lispval_take(v, i); }
  }

  /* Empty Expression */
  if (v->count == 0) { return v; }

  /* Single Expression */
  if (v->count == 1) { return lispval_take(v, 0); }

  /* Ensure First Element is Symbol */
  lispval* f = lispval_pop(v, 0);
  if (f->type != LISPVAL_SYM) {
    lispval_del(f); lispval_del(v);
    return lispval_err("S-expression Does not start with symbol!");
  }

  /* Call builtin with operator */
  lispval* result = builtin_op(v, f->sym);
  lispval_del(f);
  return result;
}

lispval* lispval_eval(lispval* v) {
  /* Evaluate Sexpressions */
  if (v->type == LISPVAL_SEXPR) { return lispval_eval_sexpr(v); }
  /* All other lval types remain the same */
  return v;
}

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

void evalAndPrint(char* input, mpc_parser_t* parser) {
  /* Attempt to Parse the user Input */
  mpc_result_t r;
  if (mpc_parse("<stdin>", input, parser, &r)) {
    lispval* x = lispval_eval(lispval_read(r.output));
    lispval_println(x);
    lispval_del(x);
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
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  // Define language
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                      \
    number: /-?[0-9]+/ ;                                                   \
    symbol: '+' | '-' | '*' | '/' | '%' | '^' | /min/ | /max/ ;            \
    sexpr  : '(' <expr>* ')' ;                                             \
    expr: <number> | <symbol> | <sexpr> ;                                  \
    lispy: /^/ <expr>* /$/ ;                                               \
    ",
    Number, Symbol, Sexpr, Expr, Lispy);


  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while(1) {
    char* input = readline("lispy> ");
    add_history(input);
    evalAndPrint(input, Lispy);
    free(input);
  }

  /* Undefine and Delete our Parsers */
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
  return 0;
}