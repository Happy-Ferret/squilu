/*	see copyright notice in squirrel.h */
#ifndef _SQCOMPILER_H_
#define _SQCOMPILER_H_

struct SQVM;

#define SQ_KEYWORDS_LIST() \
    ENUM_TK(3WAYSCMP)\
    ENUM_TK(AND)\
    ENUM_TK(ARROW)\
    ENUM_TK(ATTR_CLOSE)\
    ENUM_TK(ATTR_OPEN)\
    ENUM_TK(BASE)\
    ENUM_TK(BIT_AND_EQ)\
    ENUM_TK(BIT_OR_EQ)\
    ENUM_TK(BIT_SHIFT_LEFT_EQ)\
    ENUM_TK(BIT_SHIFT_RIGHT_EQ)\
    ENUM_TK(BIT_XOR_EQ)\
    ENUM_TK(BREAK)\
    ENUM_TK(CASE)\
    ENUM_TK(CATCH)\
    ENUM_TK(CLASS)\
    ENUM_TK(CLONE)\
    ENUM_TK(CONST)\
    ENUM_TK(CONSTRUCTOR)\
    ENUM_TK(CONTINUE)\
    ENUM_TK(DEFAULT)\
    ENUM_TK(DELETE)\
    ENUM_TK(DESTRUCTOR)\
    ENUM_TK(DIVEQ)\
    ENUM_TK(DO)\
    ENUM_TK(DOUBLE_COLON)\
    ENUM_TK(ELSE)\
    ENUM_TK(ENUM)\
    ENUM_TK(EQ)\
    ENUM_TK(EQ_IDENTITY)\
    ENUM_TK(EXTENDS)\
    ENUM_TK(EXTERN) \
    ENUM_TK(FALSE)\
    ENUM_TK(__FILE__)\
    ENUM_TK(FLOAT)\
    ENUM_TK(FOR)\
    ENUM_TK(FOREACH)\
    ENUM_TK(FRIEND)\
    ENUM_TK(__FUNCTION__)\
    ENUM_TK(FUNCTION)\
    ENUM_TK(GE)\
    ENUM_TK(IDENTIFIER)\
    ENUM_TK(IF)\
    ENUM_TK(IGNORE)\
    ENUM_TK(IN)\
    ENUM_TK(INSTANCEOF)\
    ENUM_TK(INTEGER)\
    ENUM_TK(LE)\
    ENUM_TK(__LINE__)\
    ENUM_TK(LOCAL)\
    ENUM_TK(LOCAL_ANY_T)\
    ENUM_TK(LOCAL_ARRAY_T)\
    ENUM_TK(LOCAL_BOOL_T)\
    ENUM_TK(LOCAL_CHAR_T)\
    ENUM_TK(LOCAL_DOUBLE_T)\
    ENUM_TK(LOCAL_FLOAT_T)\
    ENUM_TK(LOCAL_INT16_T)\
    ENUM_TK(LOCAL_INT32_T)\
    ENUM_TK(LOCAL_INT64_T)\
    ENUM_TK(LOCAL_INT8_T)\
    ENUM_TK(LOCAL_INT_T)\
    ENUM_TK(LOCAL_LONG_DOUBLE_T)\
    ENUM_TK(LOCAL_NUMBER_T)\
    ENUM_TK(LOCAL_STRING_T)\
    ENUM_TK(LOCAL_TABLE_T)\
    ENUM_TK(LOCAL_UINT16_T)\
    ENUM_TK(LOCAL_UINT32_T)\
    ENUM_TK(LOCAL_UINT64_T)\
    ENUM_TK(LOCAL_UINT8_T)\
    ENUM_TK(LOCAL_UINT_T)\
    ENUM_TK(LOCAL_VOIDPTR_T)\
    ENUM_TK(LOCAL_WCHAR_T)\
    ENUM_TK(LOCAL_WEAKREF_T)\
    ENUM_TK(MINUSEQ)\
    ENUM_TK(MINUSMINUS)\
    ENUM_TK(MODEQ)\
    ENUM_TK(MODULO)\
    ENUM_TK(MULEQ)\
    ENUM_TK(NE)\
    ENUM_TK(NE_IDENTITY)\
    ENUM_TK(NEWSLOT)\
    ENUM_TK(NOEXCEPT)\
    ENUM_TK(NULL)\
    ENUM_TK(OR)\
    ENUM_TK(PLUSEQ)\
    ENUM_TK(PLUSPLUS)\
    ENUM_TK(PRAGMA)\
    ENUM_TK(PRIVATE)\
    ENUM_TK(PUBLIC)\
    ENUM_TK(RESUME)\
    ENUM_TK(RETURN)\
    ENUM_TK(SHIFTL)\
    ENUM_TK(SHIFTR)\
    ENUM_TK(STATIC)\
    ENUM_TK(STRING_LITERAL)\
    ENUM_TK(STRUCT) \
    ENUM_TK(SWITCH)\
    ENUM_TK(THIS)\
    ENUM_TK(THROW)\
    ENUM_TK(TYPEDEF)\
    ENUM_TK(TRUE)\
    ENUM_TK(TRY)\
    ENUM_TK(TYPEOF)\
    ENUM_TK(UMINUS)\
    ENUM_TK(USING)\
    ENUM_TK(USHIFTR)\
    ENUM_TK(VARPARAMS)\
    ENUM_TK(VIRTUAL)\
    ENUM_TK(VOID)\
    ENUM_TK(VOLATILE)\
    ENUM_TK(WHILE)\
    ENUM_TK(YIELD)
    //ENUM_TK(VARGC)
    //ENUM_TK(VARGV)

#define ENUM_TK(tk) TK_##tk,
enum SQKeywordsEnum {
    TK_FIRST_ENUM_TOKEN = 258,
    /*
    the above token is only for internal purposes
    like calculate total enum_tokens = TK_LAST_ENUM_TOKEN - TK_FIRST_ENUM_TOKEN -1
    */
    SQ_KEYWORDS_LIST()
    /*
    the next token is only for internal purposes
    like calculate total enum_tokens = TK_LAST_ENUM_TOKEN - TK_FIRST_ENUM_TOKEN -1
    */
    TK_LAST_ENUM_TOKEN
};
#undef ENUM_TK

typedef void(*CompilerErrorFunc)(void *ud, const SQChar *s);
bool Compile(SQVM *vm, SQLEXREADFUNC rg, SQUserPointer up, const SQChar *sourcename, SQObjectPtr &out,
             bool raiseerror, bool lineinfo, bool show_warnings, SQInteger max_nested_includes=10);
#endif //_SQCOMPILER_H_
