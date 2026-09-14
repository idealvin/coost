%{
#define YY_NO_UNISTD_H 1
#include "gen.h"
#include "geny.hh"
inline int isatty(int) { return 0; }
%}

%option lex-compat
%option noyywrap
%option nounput


symbol        ([;\,\=\{\}\[\]])
intconstant   ([+-]?[0-9]+)
hexconstant   ([+-]?"0x"[0-9A-Fa-f]+)
dblconstant   ([+-]?[0-9]*(\.[0-9]+)?([eE][+-]?[0-9]+)?)
identifier    ([a-zA-Z_](\.[a-zA-Z_0-9]|[a-zA-Z_0-9])*)
whitespace    ([ \t\r\n]*)
comment       ("//"[^\n]*)
mlcm_begin    ("/*")
literal_begin (['\"])


%%
{whitespace} {}
{comment}    {}
{mlcm_begin} {
    int state = 0;
    while (state < 2) {
        const int c = yyinput();
        switch (c) {
            case '*':
                if (state != 1) state = 1;
                break;
            case '/':
                if (state == 1) state = 2;
                break;
            case EOF:
                co::println(
                    "unexpected end of file while parsing multiline comment at line: ", yylineno
                );
                exit(0);
            default:
                if (state != 0) state = 0;
                break;
        }
    }
}

{symbol}   { return yytext[0]; }

"false"    { yylval.bconst = false; return tok_bool_constant; }
"true"     { yylval.bconst = true; return tok_bool_constant; }

"package"  { return tok_package; }
"service"  { return tok_service; }
"bool"     { return tok_bool; }
"int"      { return tok_int; }
"int32"    { return tok_int32; }
"int64"    { return tok_int64; }
"uint32"   { return tok_uint32; }
"uint64"   { return tok_uint64; }
"double"   { return tok_double; }
"string"   { return tok_string; }
"object"   { return tok_object; }

{intconstant} {
    int e;
    yylval.iconst = co::stoi64(yytext, &e);
    if (e != 0) {
        co::println("integer overflow: ", yytext, " at line ", yylineno);
        exit(0);
    }
    return tok_int_constant;
}

{hexconstant} {
    int e;
    yylval.iconst = co::stoi64(yytext, &e);
    if (e != 0) {
        co::println("integer overflow: ", yytext, " at line ", yylineno);
        exit(0);
    }
    return tok_int_constant;
}

{dblconstant} {
    yylval.dconst = co::stod(yytext);
    return tok_dbl_constant;
}

{identifier} {
    yylval.iden = co::strdup(yytext);
    return tok_identifier;
}

{literal_begin} {
    char q = yytext[0];
    co::string s;
    for (;;) {
        int c = yyinput();
        switch (c) {
          case EOF:
            co::println("missing ", q, " at line ", yylineno);
            exit(0);
          case '\n':
            co::println("missing ", q, " at line ", (yylineno - 1));
            exit(0);
          case '\\':
            c = yyinput();
            switch (c) {
              case 'r':
                s.append('\r');
                continue;
              case 'n':
                s.append('\n');
                continue;
              case 't':
                s.append('\t');
                continue;
              case '"':
                s.append('"');
                continue;
              case '\'':
                s.append('\'');
                continue;
              case '\\':
                s.append('\\');
                continue;
              default:
                co::println("invalid escape character: ", c, " at line ", yylineno);
                exit(0);
            }
            break;
          default:
            if (c == q) {
                yylval.iden = co::strdup(s.c_str());
                return tok_literal;
            }
            s.append(c);
        }
    }
}

. {
    co::println("unexpected token: ", yytext, " at line ", yylineno);
    exit(0);
}

%%
