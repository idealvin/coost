#ifndef _yy_defines_h_
#define _yy_defines_h_

#define tok_identifier 257
#define tok_literal 258
#define tok_int_constant 259
#define tok_dbl_constant 260
#define tok_bool_constant 261
#define tok_package 262
#define tok_service 263
#define tok_bool 264
#define tok_int 265
#define tok_int32 266
#define tok_int64 267
#define tok_uint32 268
#define tok_uint64 269
#define tok_double 270
#define tok_string 271
#define tok_object 272
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union YYSTYPE {
    char* iden;
    char* keyword;
    bool bconst;
    int64 iconst;
    double dconst;
    Service* tservice;
    Field* tfield;
    Value* tvalue;
    Type* ttype;
    Object* tobject;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
extern YYSTYPE yylval;

#endif /* _yy_defines_h_ */
