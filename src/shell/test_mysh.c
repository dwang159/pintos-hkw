#include "minunit.h"
#include "tokenize.h"
#include "command.h"
#include <stdbool.h>
#include <stdio.h>

int tests_run = 0;

static char *test_match_gen() {
    token tkn = {EMPTY, {0}};
    token *t = &tkn;
    int out;
    out = match_gen_redirect("1>", t);
    mu_assert("doesn't fail", out != -1);
    mu_assert("gets correct length", out == 2);
    mu_assert("filles out token type correctly", t->type == GENOUTRED);
    mu_assert("filles out number correctly", t->data.onefiledes == 1);
    out = match_gen_redirect("45667>", t);
    mu_assert("doesn't fail 2", out != -1);
    mu_assert("gets correct length 2", out == 6);
    mu_assert("filles out token type correctly 2", t->type == GENOUTRED);
    mu_assert("filles out number correctly 2", t->data.onefiledes == 45667);
    out = match_gen_redirect(">", t);
    mu_assert("fails correctly", out == -1);
    out = match_gen_redirect("1234", t);
    mu_assert("fails correctly 2", out == -1);
    return 0;
}

static char *test_match_bi_gen() {
    token tkn = {EMPTY, {0}};
    token *t = &tkn;
    int out;
    out = match_gen_bi_redirect("1>&2", t);
    mu_assert("doesn't fail", out != -1);
    mu_assert("gets length right", out == 4);
    mu_assert("fills out type correctly", t->type == GENINOUTRED);
    mu_assert("fills out a correctly", t->data.filedespair[1] == 1);
    mu_assert("fills out b correctly", t->data.filedespair[0] == 2);
    return 0;    
}

static char *test_tokenizer() {
    token outp[MAXTOKENS];
    int length;
    length = tokenize_input("a arg1 arg2", outp);
    mu_assert("captures correct length (basic)", length == 3);
    token firstout[4] = {{STRING, {.str = "a"}}, 
                         {STRING, {.str = "arg1"}}, 
                         {STRING, {.str = "arg2"}}, 
                         {EMPTY, {0}}};
    mu_assert("captures correct structure (basic)", eq_token_list(outp, firstout)); 
    length = tokenize_input("a arg1 arg2 | b arg1 | c arg1 arg2 arg3", outp);
    mu_assert("captures correct length (med)", length == 11);
    token secondout[12] = {{STRING, {.str = "a"}}, 
                            {STRING, {.str = "arg1"}},
                            {STRING, {.str = "arg2"}},
                            {PIPE, {0}},
                            {STRING, {.str = "b"}}, 
                            {STRING, {.str = "arg1"}},
                            {PIPE, {0}},
                            {STRING, {.str = "c"}},
                            {STRING, {.str = "arg1"}},
                            {STRING, {.str = "arg2"}},
                            {STRING, {.str = "arg3"}},
                            {EMPTY, {0}}};
    mu_assert("captures correct structure (med)", eq_token_list(outp, secondout));
    length = tokenize_input("a arg1 arg2 < inp.txt | b arg1 |" 
                            "c arg1 arg2 \"arg3\" > out.txt", outp);
    mu_assert("captures correct lengtH (hard)", length == 15);
    token thirdout[16] = {{STRING, {.str = "a"}},
                          {STRING, {.str = "arg1"}},
                          {STRING, {.str = "arg2"}},
                          {CHINP, {0}},
                          {STRING, {.str = "inp.txt"}},
                          {PIPE, {0}},
                          {STRING, {.str = "b"}}, 
                          {STRING, {.str = "arg1"}},
                          {PIPE, {0}},
                          {STRING, {.str = "c"}},
                          {STRING, {.str = "arg1"}},
                          {STRING, {.str = "arg2"}},
                          {STRING, {.str = "arg3"}},
                          {CHOUT, {0}},
                          {STRING, {.str = "out.txt"}},
                          {EMPTY, {0}}};
    mu_assert("catures correct structure (hard)", eq_token_list(outp, thirdout));
    return 0;
}

static char *test_sep_cmd() {
   token inp[12] = {{STRING, {.str = "a"}},
                    {STRING, {.str = "arg1"}},
                    {STRING, {.str = "arg2"}},
                    {PIPE, {0}},
                    {STRING, {.str = "b"}}, 
                    {STRING, {.str = "arg1"}},
                    {PIPE, {0}},
                    {STRING, {.str = "c"}},
                    {STRING, {.str = "arg1"}},
                    {STRING, {.str = "arg2"}},
                    {STRING, {.str = "arg3"}},
                    {EMPTY, {0}}};
   token basic[2] = {{.type = STRING, {.str = "hi"}},
                     {.type = EMPTY, {0}}};
   command *out = separate_commands(basic);
   command e = CMDBLANK;
   char *args1[2] = {"hi", NULL};
   e.argv_cmds = args1;
   mu_assert("simple parsing", eq_command(out[0], e));
   out = separate_commands(inp);
   char *args2[4] = {"a", "arg1", "arg2", NULL};
   e.argv_cmds = args2;
   mu_assert("Basic command parsing", eq_command(out[0], e));
   char *args3[3] = {"b", "arg1", NULL};
   e.argv_cmds = args3;
   mu_assert("BCP: again", eq_command(out[1], e));
   char *args4[5] = {"c", "arg1", "arg2", "arg3", NULL};
   e.argv_cmds = args4;
   mu_assert("BCP: third time", eq_command(out[2], e));
   return 0;
}

static char *all_tests() {
    mu_run_test(test_match_gen);
    mu_run_test(test_match_bi_gen);
    mu_run_test(test_tokenizer);
    mu_run_test(test_sep_cmd);
    return 0;
}

int main() {
    char *result = all_tests();
    if (result != 0) {
        printf("%s\n", result);
    } else
        printf("All tests passed.\n");
    printf("%d tests run\n", tests_run);
    return result != 0;
}


