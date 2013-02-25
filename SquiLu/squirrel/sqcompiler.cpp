/*
	see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
#ifndef NO_COMPILER
#include <stdarg.h>
#include <setjmp.h>
#include "sqopcodes.h"
#include "sqstring.h"
#include "sqfuncproto.h"
#include "sqcompiler.h"
#include "sqfuncstate.h"
#include "sqlexer.h"
#include "sqvm.h"
#include "sqtable.h"

#define EXPR   1
#define OBJECT 2
#define BASE   3
#define LOCAL  4
#define OUTER  5

struct SQExpState {
  SQInteger  etype;       /* expr. type; one of EXPR, OBJECT, BASE, OUTER or LOCAL */
  SQInteger  epos;        /* expr. location on stack; -1 for OBJECT and BASE */
  bool       donot_get;   /* signal not to deref the next value */
};

struct SQScope {
	SQInteger outers;
	SQInteger stacksize;
	SQInteger nested;
};

#define BEGIN_SCOPE() SQScope __oldscope__ = _scope; \
					 ++_scope.nested; \
					 _scope.outers = _fs->_outers; \
					 _scope.stacksize = _fs->GetStackSize();\
					 _scope_consts.push_back(SQTable::Create(_ss(_vm),0));

#define RESOLVE_OUTERS() if(_fs->GetStackSize() != _scope.stacksize) { \
							if(_fs->CountOuters(_scope.stacksize)) { \
								_fs->AddInstruction(_OP_CLOSE,0,_scope.stacksize); \
							} \
						}

#define END_SCOPE_NO_CLOSE() {	if(_fs->GetStackSize() != _scope.stacksize) { \
							_fs->SetStackSize(_scope.stacksize); \
						} \
						_scope = __oldscope__; \
						_scope_consts.pop_back();\
					}

#define END_SCOPE() {	SQInteger oldouters = _fs->_outers;\
						if(_fs->GetStackSize() != _scope.stacksize) { \
							_fs->SetStackSize(_scope.stacksize); \
							if(oldouters != _fs->_outers) { \
								_fs->AddInstruction(_OP_CLOSE,0,_scope.stacksize); \
							} \
						} \
						_scope = __oldscope__; \
					}

#define BEGIN_BREAKBLE_BLOCK()	SQInteger __nbreaks__=_fs->_unresolvedbreaks.size(); \
							SQInteger __ncontinues__=_fs->_unresolvedcontinues.size(); \
							_fs->_breaktargets.push_back(0);_fs->_continuetargets.push_back(0);

#define END_BREAKBLE_BLOCK(continue_target) {__nbreaks__=_fs->_unresolvedbreaks.size()-__nbreaks__; \
					__ncontinues__=_fs->_unresolvedcontinues.size()-__ncontinues__; \
					if(__ncontinues__>0)ResolveContinues(_fs,__ncontinues__,continue_target); \
					if(__nbreaks__>0)ResolveBreaks(_fs,__nbreaks__); \
					_fs->_breaktargets.pop_back();_fs->_continuetargets.pop_back();}

class SQCompiler
{
public:
	SQCompiler(SQVM *v, SQLEXREADFUNC rg, SQUserPointer up, const SQChar* sourcename,
            bool raiseerror, bool lineinfo, bool show_warnings)
	{
		_vm=v;
		_lex.Init(_ss(v), rg, up,ThrowError,this);
		_sourcename = SQString::Create(_ss(v), sourcename);
		_lineinfo = lineinfo;_raiseerror = raiseerror;
		_show_warnings = show_warnings;
		_scope.outers = 0;
		_scope.stacksize = 0;
		_scope.nested = 0;
		compilererror = NULL;
		_globals = SQTable::Create(_ss(_vm),0);
	}
	~SQCompiler(){
	    _table(_globals)->Finalize();
	    _globals.Null();
	}

	static void ThrowError(void *ud, const SQChar *s) {
		SQCompiler *c = (SQCompiler *)ud;
		c->Error(s);
	}

	void Error(const SQChar *s, ...)
	{
		static SQChar temp[256];
		va_list vl;
		va_start(vl, s);
		scvsprintf(temp, s, vl);
		va_end(vl);
		compilererror = temp;
		longjmp(_errorjmp,1);
	}

	void Warning(const SQChar *s, ...)
	{
	    if(!_show_warnings) return;
		va_list vl;
		va_start(vl, s);
		scvfprintf(stderr, s, vl);
		va_end(vl);
	}

	void CheckGlobalName(const SQObject &name, bool addIfNotExists=false, bool checkLocals=true){
	    if(_table(_globals)->Exists(name)){
            if(checkLocals) Error(_SC("global '%s' already declared"), _stringval(name));
            else Warning(_SC("WARNING: %s:%d:%d global '%s' already declared will be shadowed\n"),
                        _stringval(_sourcename), _lex._currentline, _lex._currentcolumn, _stringval(name));
	    }
	    else if(checkLocals) CheckLocalNameScope(name, -1, false);
	    if(addIfNotExists) {
	        SQObjectPtr oname = name, otrue = true;
	        _table(_globals)->NewSlot(oname, otrue);
	    }
	}

	void CheckLocalNameScope(const SQObject &name, SQInteger scope, bool checkGlobals=true){
	    SQInteger found = _fs->GetLocalVariable(name);
	    if(found >= 0){
	        SQLocalVarInfo &lvi = _fs->_vlocals[found];
	        if(lvi._scope == scope)
                Error(_SC("local '%s' already declared"), _stringval(name));
            else
                Warning(_SC("WARNING: %s:%d:%d local '%s' already declared will be shadowed\n"),
                        _stringval(_sourcename), _lex._currentline, _lex._currentcolumn, _stringval(name));
	    }
	    else
	    {
	        found = _fs->FindOuterVariable(name);
	        if(found >= 0) Warning(_SC("WARNING: %s:%d:%d outer variable '%s' already declared will be shadowed\n"),
                        _stringval(_sourcename), _lex._currentline, _lex._currentcolumn, _stringval(name));
	    }
	    if(checkGlobals) CheckGlobalName(name, false, false);
	}

	bool IsConstant(const SQObject &name,SQObject &e){
        SQObjectPtr val;
	    for(int i=_scope.nested-1; i >= 0; --i){
	        if(_table(_scope_consts[i])->Get(name,val)) {
	            e = val;
	            return true;
	        }
	    }
        if(_table(_ss(_vm)->_consts)->Get(name,val)) {
            e = val;
            return true;
        }
        return false;
	}

	void CheckConstsExists(const SQObjectPtr &key){
	    int found = -1;
	    for(int i=_scope.nested-1; i >= 0; --i){
	        if(_table(_scope_consts[i])->Exists(key)) {
	            found = i+1;
	            break;
	        }
	    }
	    if(found < 0 && _table(_ss(_vm)->_consts)->Exists(key)) found = 0;
        if(found == _scope.nested) {
            Error(_SC("constant '%s' already exists\n"), _stringval(key));
        }
        if(found >= 0) Warning(_SC("WARNING: %s:%d:%d an already defined constant '%s' will be shadowed\n"),
            _stringval(_sourcename), _lex._currentline, _lex._currentcolumn,  _stringval(key));
	}

	bool ConstsGet(const SQObjectPtr &key,SQObjectPtr &val){
	    for(int i=_scope.nested-1; i >= 0; --i){
	        if(_table(_scope_consts[i])->Get(key,val)) return true;
	    }
	    return _table(_ss(_vm)->_consts)->Get(key,val);
	}

	bool ConstsNewSlot(const SQObjectPtr &key, const SQObjectPtr &val){
	    if(_scope.nested) return _table(_scope_consts[_scope.nested-1])->NewSlot(key,val);
	    return _table(_ss(_vm)->_consts)->NewSlot(key,val);
	}

	void Lex(){	_token = _lex.Lex();}
	SQObjectPtr GetTokenObject(SQInteger tok)
	{
		SQObjectPtr ret;
		switch(tok)
		{
		case TK_IDENTIFIER:
			ret = _fs->CreateString(_lex._svalue);
			break;
		case TK_STRING_LITERAL:
			ret = _fs->CreateString(_lex._svalue,_lex._longstr.size()-1);
			break;
		case TK_INTEGER:
			ret = SQObjectPtr(_lex._nvalue);
			break;
		case TK_FLOAT:
			ret = SQObjectPtr(_lex._fvalue);
			break;
		}
		Lex();
		return ret;
	}
	void ErrorIfNotToken(SQInteger tok){
		if(_token != tok) {
			if(_token == TK_CONSTRUCTOR && tok == TK_IDENTIFIER) {
				//do nothing
			}
			else {
				const SQChar *etypename;
				if(tok > 255) {
					switch(tok)
					{
					case TK_IDENTIFIER:
						etypename = _SC("IDENTIFIER");
						break;
					case TK_STRING_LITERAL:
						etypename = _SC("STRING_LITERAL");
						break;
					case TK_INTEGER:
						etypename = _SC("INTEGER");
						break;
					case TK_FLOAT:
						etypename = _SC("FLOAT");
						break;
					default:
						etypename = _lex.Tok2Str(tok);
					}
					Error(_SC("expected '%s'"), etypename);
				}
				Error(_SC("expected '%c'"), tok);
			}
		}
	}
	SQObject Expect(SQInteger tok)
	{
        ErrorIfNotToken(tok);
		return GetTokenObject(tok);
	}
	bool IsEndOfStatement() { return ((_lex._prevtoken == _SC('\n')) || (_token == SQUIRREL_EOB) || (_token == _SC('}')) || (_token == _SC(';'))); }
	void OptionalSemicolon()
	{
		if(_token == _SC(';')) { Lex(); return; }
		if(!IsEndOfStatement()) {
			Error(_SC("end of statement expected (; or lf) found (%d)"), _token);
		}
	}
	void MoveIfCurrentTargetIsLocal() {
		SQInteger trg = _fs->TopTarget();
		if(_fs->IsLocal(trg)) {
			trg = _fs->PopTarget(); //no pops the target and move it
			_fs->AddInstruction(_OP_MOVE, _fs->PushTarget(), trg);
		}
	}
	bool Compile(SQObjectPtr &o)
	{
		_debugline = 1;
		_debugop = 0;

		SQFuncState funcstate(_ss(_vm), NULL,ThrowError,this);
		funcstate._name = SQString::Create(_ss(_vm), _SC("main"));
		_fs = &funcstate;
		_fs->AddParameter(_fs->CreateString(_SC("this")), _scope.nested+1);
		_fs->AddParameter(_fs->CreateString(_SC("vargv")), _scope.nested+1);
		_fs->_varparams = true;
		_fs->_sourcename = _sourcename;
		SQInteger stacksize = _fs->GetStackSize();
		if(setjmp(_errorjmp) == 0) {
			Lex();
			while(_token > 0){
				Statement();
				if(_lex._prevtoken != _SC('}') && _lex._prevtoken != _SC(';')) OptionalSemicolon();
			}
			_fs->SetStackSize(stacksize);
			_fs->AddLineInfos(_lex._currentline, _lineinfo, true);
			_fs->AddInstruction(_OP_RETURN, 0xFF);
			_fs->SetStackSize(0);
			o =_fs->BuildProto();
#ifdef _DEBUG_DUMP
			_fs->Dump(_funcproto(o));
#endif
		}
		else {
			if(_raiseerror && _ss(_vm)->_compilererrorhandler) {
				_ss(_vm)->_compilererrorhandler(_vm, compilererror, type(_sourcename) == OT_STRING?_stringval(_sourcename):_SC("unknown"),
					_lex._currentline, _lex._currentcolumn);
			}
			_vm->_lasterror = SQString::Create(_ss(_vm), compilererror, -1);
			return false;
		}
		return true;
	}
	void Statements()
	{
		while(_token != _SC('}') && _token != TK_DEFAULT && _token != TK_CASE) {
			Statement();
			if(_lex._prevtoken != _SC('}') && _lex._prevtoken != _SC(';')) OptionalSemicolon();
		}
	}
	void Statement(bool closeframe = true)
	{
		_fs->AddLineInfos(_lex._currentline, _lineinfo);
		switch(_token){
		case _SC(';'):	Lex();					break;
		case TK_IF:		IfStatement();			break;
		case TK_WHILE:		WhileStatement();		break;
		case TK_DO:		DoWhileStatement();		break;
		case TK_FOR:		ForStatement();			break;
		case TK_FOREACH:	ForEachStatement();		break;
		case TK_SWITCH:	SwitchStatement();		break;
		case TK_LOCAL_CHAR_T:
		case TK_LOCAL_WCHAR_T:
		case TK_LOCAL_BOOL_T:
		case TK_LOCAL_TABLE_T:
		case TK_LOCAL_ARRAY_T:
		case TK_LOCAL_INT8_T:
		case TK_LOCAL_INT16_T:
		case TK_LOCAL_INT32_T:
		case TK_LOCAL_INT64_T:
		case TK_LOCAL_INT_T:
		case TK_LOCAL_UINT8_T:
		case TK_LOCAL_UINT16_T:
		case TK_LOCAL_UINT32_T:
		case TK_LOCAL_UINT64_T:
		case TK_LOCAL_UINT_T:
		case TK_LOCAL_FLOAT_T:
		case TK_LOCAL_DOUBLE_T:
		case TK_LOCAL_LONG_DOUBLE_T:
		//case TK_CONST:
		case TK_LOCAL:		LocalDeclStatement();	break;
		case TK_RETURN:
		case TK_YIELD: {
			SQOpcode op;
			if(_token == TK_RETURN) {
				op = _OP_RETURN;
			}
			else {
				op = _OP_YIELD;
				_fs->_bgenerator = true;
			}
			Lex();
			if(!IsEndOfStatement()) {
				SQInteger retexp = _fs->GetCurrentPos()+1;
				CommaExpr(true);
				if(op == _OP_RETURN && _fs->_traps > 0)
					_fs->AddInstruction(_OP_POPTRAP, _fs->_traps, 0);
				_fs->_returnexp = retexp;
				_fs->AddInstruction(op, 1, _fs->PopTarget(),_fs->GetStackSize());
			}
			else{
				if(op == _OP_RETURN && _fs->_traps > 0)
					_fs->AddInstruction(_OP_POPTRAP, _fs->_traps ,0);
				_fs->_returnexp = -1;
				_fs->AddInstruction(op, 0xFF,0,_fs->GetStackSize());
			}
			break;}
		case TK_BREAK:
			if(_fs->_breaktargets.size() <= 0)Error(_SC("'break' has to be in a loop block"));
			if(_fs->_breaktargets.top() > 0){
				_fs->AddInstruction(_OP_POPTRAP, _fs->_breaktargets.top(), 0);
			}
			RESOLVE_OUTERS();
			_fs->AddInstruction(_OP_JMP, 0, -1234);
			_fs->_unresolvedbreaks.push_back(_fs->GetCurrentPos());
			Lex();
			break;
		case TK_CONTINUE:
			if(_fs->_continuetargets.size() <= 0)Error(_SC("'continue' has to be in a loop block"));
			if(_fs->_continuetargets.top() > 0) {
				_fs->AddInstruction(_OP_POPTRAP, _fs->_continuetargets.top(), 0);
			}
			RESOLVE_OUTERS();
			_fs->AddInstruction(_OP_JMP, 0, -1234);
			_fs->_unresolvedcontinues.push_back(_fs->GetCurrentPos());
			Lex();
			break;
		case TK_FUNCTION:
			FunctionStatement();
			break;
		case TK_CLASS:
			ClassStatement();
			break;
		case TK_ENUM:
			EnumStatement();
			break;
		case _SC('{'):{
				BEGIN_SCOPE();
				Lex();
				Statements();
				Expect(_SC('}'));
				if(closeframe) {
					END_SCOPE();
				}
				else {
					END_SCOPE_NO_CLOSE();
				}
			}
			break;
		case TK_TRY:
			TryCatchStatement();
			break;
		case TK_THROW:
			Lex();
			CommaExpr();
			_fs->AddInstruction(_OP_THROW, _fs->PopTarget());
			break;

		case TK_CONST:
			{
			Lex();
			SQObject id = Expect(TK_IDENTIFIER);
			Expect('=');
			SQObjectPtr strongid = id;
			CheckLocalNameScope(id, _scope.nested);
			CheckConstsExists(strongid);
			SQObject val = ExpectScalar();
			OptionalSemicolon();
			ConstsNewSlot(strongid,SQObjectPtr(val));
			strongid.Null();

			}
			break;

		default:
			CommaExpr();
			_fs->DiscardTarget();
/*
//Fagiano says that this is not a bug
//and with this modification stack grow by one lement all the time
            if(_token == TK_IDENTIFIER){
                CommaExpr();
                if(_token == TK_IDENTIFIER){
                    Error(_SC(" '=' expected near '%s'"), _lex._svalue);
                }
            }
*/
			//_fs->PopTarget();
			break;
		}
		_fs->SnoozeOpt();
	}
	void EmitDerefOp(SQOpcode op)
	{
		SQInteger val = _fs->PopTarget();
		SQInteger key = _fs->PopTarget();
		SQInteger src = _fs->PopTarget();
        _fs->AddInstruction(op,_fs->PushTarget(),src,key,val);
	}
	void Emit2ArgsOP(SQOpcode op, SQInteger p3 = 0)
	{
		SQInteger p2 = _fs->PopTarget(); //src in OP_GET
		SQInteger p1 = _fs->PopTarget(); //key in OP_GET
		_fs->AddInstruction(op,_fs->PushTarget(), p1, p2, p3);
	}
	void EmitCompoundArith(SQInteger tok, SQInteger etype, SQInteger pos)
	{
		/* Generate code depending on the expression type */
		switch(etype) {
		case LOCAL:{
			SQInteger p2 = _fs->PopTarget(); //src in OP_GET
			SQInteger p1 = _fs->PopTarget(); //key in OP_GET
			_fs->PushTarget(p1);
			//EmitCompArithLocal(tok, p1, p1, p2);
			_fs->AddInstruction(ChooseArithOpByToken(tok),p1, p2, p1, 0);
				   }
			break;
		case OBJECT:
		case BASE:
			{
				SQInteger val = _fs->PopTarget();
				SQInteger key = _fs->PopTarget();
				SQInteger src = _fs->PopTarget();
				/* _OP_COMPARITH mixes dest obj and source val in the arg1 */
				_fs->AddInstruction(_OP_COMPARITH, _fs->PushTarget(), (src<<16)|val, key, ChooseCompArithCharByToken(tok));
			}
			break;
		case OUTER:
			{
				SQInteger val = _fs->TopTarget();
				SQInteger tmp = _fs->PushTarget();
				_fs->AddInstruction(_OP_GETOUTER,   tmp, pos);
				_fs->AddInstruction(ChooseArithOpByToken(tok), tmp, val, tmp, 0);
				_fs->AddInstruction(_OP_SETOUTER, tmp, pos, tmp);
			}
			break;
		}
	}
	void CommaExpr(bool warningAssign=false)
	{
		for(Expression(warningAssign);_token == ',';_fs->PopTarget(), Lex(), CommaExpr(warningAssign));
	}
	void ErrorIfConst(){
        SQLocalVarInfo &vsrc = _fs->_vlocals[_fs->TopTarget()];
//printf("%d %d %d %d %s\n", __LINE__, vsrc._scope, vsrc._type, vsrc._pos, vsrc._name._unVal.pString ? _stringval(vsrc._name) : "?");
        if(vsrc._type & _VAR_CONST) Error(_SC("can't assign to a const variable"));
	}
	void Expression(bool warningAssign=false)
	{
		 SQExpState es = _es;
		_es.etype     = EXPR;
		_es.epos      = -1;
		_es.donot_get = false;
		LogicalOrExp();
		switch(_token)  {
		case _SC('='):
		case TK_NEWSLOT:
		case TK_MINUSEQ:
		case TK_PLUSEQ:
		case TK_MULEQ:
		case TK_DIVEQ:
		case TK_MODEQ:{
			SQInteger op = _token;
			SQInteger ds = _es.etype;
			SQInteger pos = _es.epos;

			if(ds == EXPR) Error(_SC("can't assign expression"));
			Lex(); Expression();

			switch(op){
			case TK_NEWSLOT:
				if(ds == OBJECT || ds == BASE)
					EmitDerefOp(_OP_NEWSLOT);
				else //if _derefstate != DEREF_NO_DEREF && DEREF_FIELD so is the index of a local
					Error(_SC("can't 'create' a local slot"));
				break;
			case _SC('='): //ASSIGN
                if(warningAssign) Warning(_SC("WARNING: making assignment at line %d:%d maybe is not what you want\n"),
                                          _lex._currentline, _lex._currentcolumn);
				switch(ds) {
				case LOCAL:
					{
						SQInteger src = _fs->PopTarget();
						SQInteger dst = _fs->TopTarget();
						_fs->AddInstruction(_OP_MOVE, dst, src);
					}
					break;
				case OBJECT:
				case BASE:
					EmitDerefOp(_OP_SET);
					break;
				case OUTER:
					{
						SQInteger src = _fs->PopTarget();
						SQInteger dst = _fs->PushTarget();
						_fs->AddInstruction(_OP_SETOUTER, dst, pos, src);
					}
				}
				break;
			case TK_MINUSEQ:
			case TK_PLUSEQ:
			case TK_MULEQ:
			case TK_DIVEQ:
			case TK_MODEQ:
				EmitCompoundArith(op, ds, pos);
				break;
			}
			}
			break;
		case _SC('?'): {
			Lex();
			_fs->AddInstruction(_OP_JZ, _fs->PopTarget());
			SQInteger jzpos = _fs->GetCurrentPos();
			SQInteger trg = _fs->PushTarget();
			Expression();
			SQInteger first_exp = _fs->PopTarget();
			if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
			SQInteger endfirstexp = _fs->GetCurrentPos();
			_fs->AddInstruction(_OP_JMP, 0, 0);
			Expect(_SC(':'));
			SQInteger jmppos = _fs->GetCurrentPos();
			Expression();
			SQInteger second_exp = _fs->PopTarget();
			if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
			_fs->SetIntructionParam(jmppos, 1, _fs->GetCurrentPos() - jmppos);
			_fs->SetIntructionParam(jzpos, 1, endfirstexp - jzpos + 1);
			_fs->SnoozeOpt();
			}
			break;
		}
		_es = es;
	}
	template<typename T> void BIN_EXP(SQOpcode op, T f,SQInteger op3 = 0)
	{
		Lex(); (this->*f)();
		SQInteger op1 = _fs->PopTarget();SQInteger op2 = _fs->PopTarget();
		_fs->AddInstruction(op, _fs->PushTarget(), op1, op2, op3);
	}
	void LogicalOrExp()
	{
		LogicalAndExp();
		for(;;) if(_token == TK_OR) {
			SQInteger first_exp = _fs->PopTarget();
			SQInteger trg = _fs->PushTarget();
			_fs->AddInstruction(_OP_OR, trg, 0, first_exp, 0);
			SQInteger jpos = _fs->GetCurrentPos();
			if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
			Lex(); LogicalOrExp();
			_fs->SnoozeOpt();
			SQInteger second_exp = _fs->PopTarget();
			if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
			_fs->SnoozeOpt();
			_fs->SetIntructionParam(jpos, 1, (_fs->GetCurrentPos() - jpos));
			break;
		}else return;
	}
	void LogicalAndExp()
	{
		BitwiseOrExp();
		for(;;) switch(_token) {
		case TK_AND: {
			SQInteger first_exp = _fs->PopTarget();
			SQInteger trg = _fs->PushTarget();
			_fs->AddInstruction(_OP_AND, trg, 0, first_exp, 0);
			SQInteger jpos = _fs->GetCurrentPos();
			if(trg != first_exp) _fs->AddInstruction(_OP_MOVE, trg, first_exp);
			Lex(); LogicalAndExp();
			_fs->SnoozeOpt();
			SQInteger second_exp = _fs->PopTarget();
			if(trg != second_exp) _fs->AddInstruction(_OP_MOVE, trg, second_exp);
			_fs->SnoozeOpt();
			_fs->SetIntructionParam(jpos, 1, (_fs->GetCurrentPos() - jpos));
			break;
			}
		case TK_IN: BIN_EXP(_OP_EXISTS, &SQCompiler::BitwiseOrExp); break;
		case TK_INSTANCEOF: BIN_EXP(_OP_INSTANCEOF, &SQCompiler::BitwiseOrExp); break;
		default:
			return;
		}
	}
	void BitwiseOrExp()
	{
		BitwiseXorExp();
		for(;;) if(_token == _SC('|'))
		{BIN_EXP(_OP_BITW, &SQCompiler::BitwiseXorExp,BW_OR);
		}else return;
	}
	void BitwiseXorExp()
	{
		BitwiseAndExp();
		for(;;) if(_token == _SC('^'))
		{BIN_EXP(_OP_BITW, &SQCompiler::BitwiseAndExp,BW_XOR);
		}else return;
	}
	void BitwiseAndExp()
	{
		EqExp();
		for(;;) if(_token == _SC('&'))
		{BIN_EXP(_OP_BITW, &SQCompiler::EqExp,BW_AND);
		}else return;
	}
	void EqExp()
	{
		CompExp();
		for(;;) switch(_token) {
		case TK_EQ: BIN_EXP(_OP_EQ, &SQCompiler::CompExp); break;
		case TK_NE: BIN_EXP(_OP_NE, &SQCompiler::CompExp); break;
		case TK_3WAYSCMP: BIN_EXP(_OP_CMP, &SQCompiler::CompExp,CMP_3W); break;
		default: return;
		}
	}
	void CompExp()
	{
		ShiftExp();
		for(;;) switch(_token) {
		case _SC('>'): BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_G); break;
		case _SC('<'): BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_L); break;
		case TK_GE: BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_GE); break;
		case TK_LE: BIN_EXP(_OP_CMP, &SQCompiler::ShiftExp,CMP_LE); break;
		default: return;
		}
	}
	void ShiftExp()
	{
		PlusExp();
		for(;;) switch(_token) {
		case TK_USHIFTR: BIN_EXP(_OP_BITW, &SQCompiler::PlusExp,BW_USHIFTR); break;
		case TK_SHIFTL: BIN_EXP(_OP_BITW, &SQCompiler::PlusExp,BW_SHIFTL); break;
		case TK_SHIFTR: BIN_EXP(_OP_BITW, &SQCompiler::PlusExp,BW_SHIFTR); break;
		default: return;
		}
	}
	SQOpcode ChooseArithOpByToken(SQInteger tok)
	{
		switch(tok) {
			case TK_PLUSEQ: case '+': return _OP_ADD;
			case TK_MINUSEQ: case '-': return _OP_SUB;
			case TK_MULEQ: case '*': return _OP_MUL;
			case TK_DIVEQ: case '/': return _OP_DIV;
			case TK_MODEQ: case '%': return _OP_MOD;
			default: assert(0);
		}
		return _OP_ADD;
	}
	SQInteger ChooseCompArithCharByToken(SQInteger tok)
	{
		SQInteger oper;
		switch(tok){
		case TK_MINUSEQ: oper = '-'; break;
		case TK_PLUSEQ: oper = '+'; break;
		case TK_MULEQ: oper = '*'; break;
		case TK_DIVEQ: oper = '/'; break;
		case TK_MODEQ: oper = '%'; break;
		default: oper = 0; //shut up compiler
			assert(0); break;
		};
		return oper;
	}
	void PlusExp()
	{
		MultExp();
		for(;;) switch(_token) {
		case _SC('+'): case _SC('-'):
			BIN_EXP(ChooseArithOpByToken(_token), &SQCompiler::MultExp); break;
		default: return;
		}
	}

	void MultExp()
	{
		PrefixedExpr();
		for(;;) switch(_token) {
		case _SC('*'): case _SC('/'): case _SC('%'):
			BIN_EXP(ChooseArithOpByToken(_token), &SQCompiler::PrefixedExpr); break;
		default: return;
		}
	}
	//if 'pos' != -1 the previous variable is a local variable
	void PrefixedExpr()
	{
		SQInteger pos = Factor();
		for(;;) {
			switch(_token) {
			case _SC('.'):
				pos = -1;
				Lex();

				_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(Expect(TK_IDENTIFIER)));
				if(_es.etype==BASE) {
					Emit2ArgsOP(_OP_GET);
					pos = _fs->TopTarget();
					_es.etype = EXPR;
					_es.epos   = pos;
				}
				else {
					if(NeedGet()) {
						Emit2ArgsOP(_OP_GET);
					}
					_es.etype = OBJECT;
				}
				break;
			case _SC('['):
				if(_lex._prevtoken == _SC('\n')) Error(_SC("cannot brake deref/or comma needed after [exp]=exp slot declaration"));
				Lex(); Expression(); Expect(_SC(']'));
				pos = -1;
				if(_es.etype==BASE) {
					Emit2ArgsOP(_OP_GET);
					pos = _fs->TopTarget();
					_es.etype = EXPR;
					_es.epos   = pos;
				}
				else {
					if(NeedGet()) {
						Emit2ArgsOP(_OP_GET);
					}
					_es.etype = OBJECT;
				}
				break;
			case TK_MINUSMINUS:
			case TK_PLUSPLUS:
				{
					if(IsEndOfStatement()) return;
					SQInteger diff = (_token==TK_MINUSMINUS) ? -1 : 1;
					Lex();
					switch(_es.etype)
					{
						case EXPR: Error(_SC("can't '++' or '--' an expression")); break;
						case OBJECT:
						case BASE:
							Emit2ArgsOP(_OP_PINC, diff);
							break;
						case LOCAL: {
							SQInteger src = _fs->PopTarget();
							_fs->AddInstruction(_OP_PINCL, _fs->PushTarget(), src, 0, diff);
									}
							break;
						case OUTER: {
							SQInteger tmp1 = _fs->PushTarget();
							SQInteger tmp2 = _fs->PushTarget();
							_fs->AddInstruction(_OP_GETOUTER, tmp2, _es.epos);
							_fs->AddInstruction(_OP_PINCL,    tmp1, tmp2, 0, diff);
							_fs->AddInstruction(_OP_SETOUTER, tmp2, _es.epos, tmp2);
							_fs->PopTarget();
						}
					}
				}
				return;
				break;
			case _SC('('):
				switch(_es.etype) {
					case OBJECT: {
						SQInteger key     = _fs->PopTarget();  /* location of the key */
						SQInteger table   = _fs->PopTarget();  /* location of the object */
						SQInteger closure = _fs->PushTarget(); /* location for the closure */
						SQInteger ttarget = _fs->PushTarget(); /* location for 'this' pointer */
						_fs->AddInstruction(_OP_PREPCALL, closure, key, table, ttarget);
						}
						break;
					case BASE:
						//Emit2ArgsOP(_OP_GET);
						_fs->AddInstruction(_OP_MOVE, _fs->PushTarget(), 0);
						break;
					case OUTER:
						_fs->AddInstruction(_OP_GETOUTER, _fs->PushTarget(), _es.epos);
						_fs->AddInstruction(_OP_MOVE,     _fs->PushTarget(), 0);
						break;
					default:
						_fs->AddInstruction(_OP_MOVE, _fs->PushTarget(), 0);
				}
				_es.etype = EXPR;
				Lex();
				FunctionCallArgs();
				break;
			default: return;
			}
		}
	}
	SQInteger Factor()
	{
		_es.etype = EXPR;
		switch(_token)
		{
		case TK_STRING_LITERAL:
			_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_fs->CreateString(_lex._svalue,_lex._longstr.size()-1)));
			Lex();
			break;
		case TK_BASE:
			Lex();
			_fs->AddInstruction(_OP_GETBASE, _fs->PushTarget());
			_es.etype  = BASE;
			_es.epos   = _fs->TopTarget();
			return (_es.epos);
			break;
		case TK_IDENTIFIER:
		case TK_CONSTRUCTOR:
		case TK_THIS:{
				SQObject id;
				SQObject constant;

				switch(_token) {
					case TK_IDENTIFIER:  id = _fs->CreateString(_lex._svalue);       break;
					case TK_THIS:        id = _fs->CreateString(_SC("this"));        break;
					case TK_CONSTRUCTOR: id = _fs->CreateString(_SC("constructor")); break;
				}

				SQInteger pos = -1;
				Lex();
				if((pos = _fs->GetLocalVariable(id)) != -1) {
					/* Handle a local variable (includes 'this') */
					_fs->PushTarget(pos);
					_es.etype  = LOCAL;
					_es.epos   = pos;
				}

				else if((pos = _fs->GetOuterVariable(id)) != -1) {
					/* Handle a free var */
					if(NeedGet()) {
						_es.epos  = _fs->PushTarget();
						_fs->AddInstruction(_OP_GETOUTER, _es.epos, pos);
						/* _es.etype = EXPR; already default value */
					}
					else {
						_es.etype = OUTER;
						_es.epos  = pos;
					}
				}

				else if(IsConstant(id, constant)) {
					/* Handle named constant */
					SQObjectPtr constval;
					SQObject    constid;
					if(type(constant) == OT_TABLE) {
						Expect('.');
						constid = Expect(TK_IDENTIFIER);
						if(!_table(constant)->Get(constid, constval)) {
							constval.Null();
							Error(_SC("invalid constant [%s.%s]"), _stringval(id), _stringval(constid));
						}
					}
					else {
						constval = constant;
					}
					_es.epos = _fs->PushTarget();

					/* generate direct or literal function depending on size */
					SQObjectType ctype = type(constval);
					switch(ctype) {
						case OT_INTEGER: EmitLoadConstInt(_integer(constval),_es.epos); break;
						case OT_FLOAT: EmitLoadConstFloat(_float(constval),_es.epos); break;
						default: _fs->AddInstruction(_OP_LOAD,_es.epos,_fs->GetConstant(constval)); break;
					}
					_es.etype = EXPR;
				}
				else {
					/* Handle a non-local variable, aka a field. Push the 'this' pointer on
					* the virtual stack (always found in offset 0, so no instruction needs to
					* be generated), and push the key next. Generate an _OP_LOAD instruction
					* for the latter. If we are not using the variable as a dref expr, generate
					* the _OP_GET instruction.
					*/
					_fs->PushTarget(0);
					_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
					if(NeedGet()) {
						Emit2ArgsOP(_OP_GET);
					}
					_es.etype = OBJECT;
				}
				return _es.epos;
			}
			break;
		case TK_DOUBLE_COLON:  // "::"
			_fs->AddInstruction(_OP_LOADROOT, _fs->PushTarget());
			_es.etype = OBJECT;
			_token = _SC('.'); /* hack: drop into PrefixExpr, case '.'*/
			_es.epos = -1;
			return _es.epos;
			break;
		case TK_NULL:
			_fs->AddInstruction(_OP_LOADNULLS, _fs->PushTarget(),1);
			Lex();
			break;
		case TK_INTEGER: EmitLoadConstInt(_lex._nvalue,-1); Lex();	break;
		case TK_FLOAT: EmitLoadConstFloat(_lex._fvalue,-1); Lex(); break;
		case TK_TRUE: case TK_FALSE:
			_fs->AddInstruction(_OP_LOADBOOL, _fs->PushTarget(),_token == TK_TRUE?1:0);
			Lex();
			break;
		case _SC('['): {
				_fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,0,NOT_ARRAY);
				SQInteger apos = _fs->GetCurrentPos(),key = 0;
				Lex();
				while(_token != _SC(']')) {
                    Expression();
					if(_token == _SC(',')) Lex();
					SQInteger val = _fs->PopTarget();
					SQInteger array = _fs->TopTarget();
					_fs->AddInstruction(_OP_APPENDARRAY, array, val, AAT_STACK);
					key++;
				}
				_fs->SetIntructionParam(apos, 1, key);
				Lex();
			}
			break;
		case _SC('{'):
			_fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,NOT_TABLE);
			Lex();ParseTableOrClass(_SC(','),_SC('}'));
			break;
		case TK_FUNCTION: FunctionExp(_token);break;
		case _SC('@'): FunctionExp(_token,true);break;
		case TK_CLASS: Lex(); ClassExp();break;
		case _SC('-'):
			Lex();
			switch(_token) {
			case TK_INTEGER: EmitLoadConstInt(-_lex._nvalue,-1); Lex(); break;
			case TK_FLOAT: EmitLoadConstFloat(-_lex._fvalue,-1); Lex(); break;
			default: UnaryOP(_OP_NEG);
			}
			break;
		case _SC('!'): Lex(); UnaryOP(_OP_NOT); break;
		case _SC('~'):
			Lex();
			if(_token == TK_INTEGER)  { EmitLoadConstInt(~_lex._nvalue,-1); Lex(); break; }
			UnaryOP(_OP_BWNOT);
			break;
		case TK_TYPEOF : Lex() ;UnaryOP(_OP_TYPEOF); break;
		case TK_RESUME : Lex(); UnaryOP(_OP_RESUME); break;
		case TK_CLONE : Lex(); UnaryOP(_OP_CLONE); break;
		case TK_MINUSMINUS :
		case TK_PLUSPLUS :PrefixIncDec(_token); break;
		case TK_DELETE : DeleteExpr(); break;
		case _SC('('): Lex(); CommaExpr(); Expect(_SC(')'));
			break;
		case TK___LINE__: EmitLoadConstInt(_lex._currentline,-1); Lex();	break;
		case TK___FILE__:
			_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_sourcename));
			Lex();
			break;
		case TK___FUNCTION__:
			_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(_fs->_name));
			Lex();
			break;
		case TK_IGNORE:
            //Warning("Keyword ignored \"%s\" at line %d:%d\n", _lex.Tok2Str(_token),
            //        _lex._currentline, _lex._currentcolumn);
            Lex(); Factor();
            break;
		default: Error(_SC("expression expected"));
		}
		return -1;
	}
	void EmitLoadConstInt(SQInteger value,SQInteger target)
	{
		if(target < 0) {
			target = _fs->PushTarget();
		}
		if((value & (~((SQInteger)0xFFFFFFFF))) == 0) { //does it fit in 32 bits?
			_fs->AddInstruction(_OP_LOADINT, target,value);
		}
		else {
			_fs->AddInstruction(_OP_LOAD, target, _fs->GetNumericConstant(value));
		}
	}
	void EmitLoadConstFloat(SQFloat value,SQInteger target)
	{
		if(target < 0) {
			target = _fs->PushTarget();
		}
		if(sizeof(SQFloat) == sizeof(SQInt32)) {
			_fs->AddInstruction(_OP_LOADFLOAT, target,*((SQInt32 *)&value));
		}
		else {
			_fs->AddInstruction(_OP_LOAD, target, _fs->GetNumericConstant(value));
		}
	}
	void UnaryOP(SQOpcode op)
	{
		PrefixedExpr();
		SQInteger src = _fs->PopTarget();
		_fs->AddInstruction(op, _fs->PushTarget(), src);
	}
	bool NeedGet()
	{
		switch(_token) {
		case _SC('='): case _SC('('): case TK_NEWSLOT: case TK_MODEQ: case TK_MULEQ:
	    case TK_DIVEQ: case TK_MINUSEQ: case TK_PLUSEQ: case TK_PLUSPLUS: case TK_MINUSMINUS:
			return false;
		}
		return (!_es.donot_get || ( _es.donot_get && (_token == _SC('.') || _token == _SC('['))));
	}
	void FunctionCallArgs()
	{
		SQInteger nargs = 1;//this
		 while(_token != _SC(')')) {
			 Expression();
			 MoveIfCurrentTargetIsLocal();
			 nargs++;
			 if(_token == _SC(',')){
				 Lex();
				 if(_token == ')') Error(_SC("expression expected, found ')'"));
			 }
		 }
		 Lex();
		 for(SQInteger i = 0; i < (nargs - 1); i++) _fs->PopTarget();
		 SQInteger stackbase = _fs->PopTarget();
		 SQInteger closure = _fs->PopTarget();
         _fs->AddInstruction(_OP_CALL, _fs->PushTarget(), closure, stackbase, nargs);
	}
	void ParseTableOrClass(SQInteger separator,SQInteger terminator)
	{
		SQInteger tpos = _fs->GetCurrentPos(),nkeys = 0;
		while(_token != terminator) {
			bool hasattrs = false;
			bool isstatic = false;
			//check if is an attribute
			if(separator == ';') {
				if(_token == TK_ATTR_OPEN) {
					_fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,NOT_TABLE); Lex();
					ParseTableOrClass(',',TK_ATTR_CLOSE);
					hasattrs = true;
				}
				if(_token == TK_STATIC) {
					isstatic = true;
					Lex();
				}
			}
			switch(_token) {
			case TK_FUNCTION:
			case TK_CONSTRUCTOR:{
				SQInteger tk = _token;
				Lex();
				SQObject id = tk == TK_FUNCTION ? Expect(TK_IDENTIFIER) : _fs->CreateString(_SC("constructor"));
				Expect(_SC('('));
				_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
				CreateFunction(id);
				_fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, 0);
								}
								break;
			case _SC('['):
				Lex(); CommaExpr(); Expect(_SC(']'));
				Expect(_SC('=')); Expression();
				break;

			case TK_STRING_LITERAL: //JSON
			case TK_IDENTIFIER: {//JSON
                SQObjectPtr obj = GetTokenObject(_token);
                SQInteger next_token = _SC('=');
                if(separator == ',' && _token == _SC(':')){ //only works for tables
                    next_token = _token;
                }
                Expect(next_token);
				_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(obj));
				Expression();
				break;
			}
			default :
                ErrorIfNotToken(TK_IDENTIFIER);
			}
			if(_token == separator) Lex();//optional comma/semicolon
			nkeys++;
			SQInteger val = _fs->PopTarget();
			SQInteger key = _fs->PopTarget();
			SQInteger attrs = hasattrs ? _fs->PopTarget():-1;
			assert((hasattrs && (attrs == key-1)) || !hasattrs);
			unsigned char flags = (hasattrs?NEW_SLOT_ATTRIBUTES_FLAG:0)|(isstatic?NEW_SLOT_STATIC_FLAG:0);
			SQInteger table = _fs->TopTarget(); //<<BECAUSE OF THIS NO COMMON EMIT FUNC IS POSSIBLE
			if(separator == _SC(',')) { //hack recognizes a table from the separator
				_fs->AddInstruction(_OP_NEWSLOT, 0xFF, table, key, val);
			}
			else {
				_fs->AddInstruction(_OP_NEWSLOTA, flags, table, key, val); //this for classes only as it invokes _newmember
			}
		}
		if(separator == _SC(',')) //hack recognizes a table from the separator
			_fs->SetIntructionParam(tpos, 1, nkeys);
		Lex();
	}
	void LocalDeclStatement()
	{
		SQObject varname;
		bool is_const_declaration = _token == TK_CONST;
		bool is_reference_declaration = false;
		SQInteger declType = _token;
		Lex();
		if( _token == TK_FUNCTION) {
			Lex();
			varname = Expect(TK_IDENTIFIER);
			CheckLocalNameScope(varname, _scope.nested);
			Expect(_SC('('));
			//the following is an attempt to allow local declared functions be called recursivelly
			SQInteger old_pos = _fs->GetCurrentPos(); //save current instructions position
			_fs->PushLocalVariable(varname, _scope.nested, _VAR_CLOSURE); //add function name to find it as outer var if needed
			CreateFunction(varname,false);
			_fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, 0);
			//rellocate any stack operation (default parameters & _OP_Closure)
			for(int i=old_pos+1, curr_pos = _fs->GetCurrentPos(); i <= curr_pos; ++i){
			    SQInstruction & inst = _fs->GetInstruction(i);
			    _fs->SetIntructionParam(i, 0, inst._arg0 -1);
			}
			_fs->PopTarget();
			return;
		}

		do {
		    if(_token == _SC('&')){
		        is_reference_declaration = true;
		        Lex();
		    }
			varname = Expect(TK_IDENTIFIER);
			CheckLocalNameScope(varname, _scope.nested);
			if(_token == _SC('=')) {
				Lex(); Expression();
				SQInteger src = _fs->PopTarget();
				SQInteger dest = _fs->PushTarget();
				if(dest != src) _fs->AddInstruction(_OP_MOVE, dest, src);
				declType = _VAR_ANY;
			}
			else if(is_const_declaration || is_reference_declaration)
                Error(_SC("const/reference '%s' need an initializer"), _stringval(varname));
			else{
			    SQInteger dest = _fs->PushTarget();
			    switch(declType){
                    case TK_LOCAL_CHAR_T:
                    case TK_LOCAL_WCHAR_T:
                        _fs->AddInstruction(_OP_LOADNULLS, dest,1);
                        declType = _VAR_STRING;
                        break;

                    case TK_LOCAL_BOOL_T:
                        //default value false
                        _fs->AddInstruction(_OP_LOADBOOL, dest,0);
                        declType = _VAR_BOOL;
                        break;

                    case TK_LOCAL_TABLE_T:
                        _fs->AddInstruction(_OP_LOADNULLS, dest,1);
                        declType = _VAR_TABLE;
                        break;
                    case TK_LOCAL_ARRAY_T:
                        _fs->AddInstruction(_OP_LOADNULLS, dest,1);
                        declType = _VAR_ARRAY;
                        break;

                    case TK_LOCAL_INT8_T:
                    case TK_LOCAL_INT16_T:
                    case TK_LOCAL_INT32_T:
                    case TK_LOCAL_INT64_T:
                    case TK_LOCAL_INT_T:
                    case TK_LOCAL_UINT8_T:
                    case TK_LOCAL_UINT16_T:
                    case TK_LOCAL_UINT32_T:
                    case TK_LOCAL_UINT64_T:
                    case TK_LOCAL_UINT_T:
                        //default value 0
                        _fs->AddInstruction(_OP_LOADINT, dest,0);
                        declType = _VAR_INTEGER;
                        break;
                    case TK_LOCAL_FLOAT_T:
                    case TK_LOCAL_DOUBLE_T:
                    case TK_LOCAL_LONG_DOUBLE_T:
                        //default value 0.0
                        //_OP_LOADFLOAT is only valid when SQFloat size == SQInt32 size
                        _fs->AddInstruction(_OP_LOADINT, dest,0);
                        declType = _VAR_FLOAT;
                        break;
                    //case TK_LOCAL:
                    default:
                        //default value null
                        _fs->AddInstruction(_OP_LOADNULLS, dest,1);
                        declType = _VAR_ANY;
			    }
			}
			_fs->PopTarget();
			_fs->PushLocalVariable(varname, _scope.nested, (is_const_declaration ? _VAR_CONST : declType)
                          | (is_reference_declaration ? _VAR_REFERENCE : 0));
			if(_token == _SC(',')) Lex(); else break;
		} while(1);
	}
	void IfStatement()
	{
		SQInteger jmppos;
		bool haselse = false;
		Lex(); Expect(_SC('(')); CommaExpr(true); Expect(_SC(')'));
		_fs->AddInstruction(_OP_JZ, _fs->PopTarget());
		SQInteger jnepos = _fs->GetCurrentPos();
		BEGIN_SCOPE();

        //there is a situation where the if statement has a statement enclosed by {}
        //and after the closing "}" there is no newline or semicolom
        //it's a valid construction but the compiler was complaining about it
        //for now added "&& _token != TK_IDENTIFIER" to the check after "Statement()" call
/*
local color = "blue";
if(color == "yellow"){
	print(color);
} print("Waht color is it ?");
*/
		Statement();
		//
		if(_token != _SC('}') && _token != TK_ELSE && _token != TK_IDENTIFIER) OptionalSemicolon();

		END_SCOPE();
		SQInteger endifblock = _fs->GetCurrentPos();
		if(_token == TK_ELSE){
			haselse = true;
			BEGIN_SCOPE();
			_fs->AddInstruction(_OP_JMP);
			jmppos = _fs->GetCurrentPos();
			Lex();
			Statement(); if(_token != TK_IDENTIFIER) OptionalSemicolon();
			END_SCOPE();
			_fs->SetIntructionParam(jmppos, 1, _fs->GetCurrentPos() - jmppos);
		}
		_fs->SetIntructionParam(jnepos, 1, endifblock - jnepos + (haselse?1:0));
	}
	void WhileStatement()
	{
		SQInteger jzpos, jmppos;
		jmppos = _fs->GetCurrentPos();
		Lex(); Expect(_SC('(')); CommaExpr(true); Expect(_SC(')'));

		BEGIN_BREAKBLE_BLOCK();
		_fs->AddInstruction(_OP_JZ, _fs->PopTarget());
		jzpos = _fs->GetCurrentPos();
		BEGIN_SCOPE();

		Statement();

		END_SCOPE();
		_fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1);
		_fs->SetIntructionParam(jzpos, 1, _fs->GetCurrentPos() - jzpos);

		END_BREAKBLE_BLOCK(jmppos);
	}
	void DoWhileStatement()
	{
		Lex();
		SQInteger jmptrg = _fs->GetCurrentPos();
		BEGIN_BREAKBLE_BLOCK()
		BEGIN_SCOPE();
		Statement();
		END_SCOPE();
		//fix proposed by frosch to correct line number info in stack dumps
		_fs->AddLineInfos(_lex._currentline, _lineinfo, true);
		Expect(TK_WHILE);
		SQInteger continuetrg = _fs->GetCurrentPos();
		Expect(_SC('(')); CommaExpr(true); Expect(_SC(')'));
		_fs->AddInstruction(_OP_JZ, _fs->PopTarget(), 1);
		_fs->AddInstruction(_OP_JMP, 0, jmptrg - _fs->GetCurrentPos() - 1);
		END_BREAKBLE_BLOCK(continuetrg);
	}
	void ForStatement()
	{
		Lex();
		BEGIN_SCOPE();
		Expect(_SC('('));
		switch(_token){
            case TK_LOCAL_CHAR_T:
            case TK_LOCAL_WCHAR_T:
            case TK_LOCAL_BOOL_T:
            case TK_LOCAL_TABLE_T:
            case TK_LOCAL_ARRAY_T:
            case TK_LOCAL_INT8_T:
            case TK_LOCAL_INT16_T:
            case TK_LOCAL_INT32_T:
            case TK_LOCAL_INT64_T:
            case TK_LOCAL_INT_T:
            case TK_LOCAL_UINT8_T:
            case TK_LOCAL_UINT16_T:
            case TK_LOCAL_UINT32_T:
            case TK_LOCAL_UINT64_T:
            case TK_LOCAL_UINT_T:
            case TK_LOCAL_FLOAT_T:
            case TK_LOCAL_DOUBLE_T:
            case TK_LOCAL_LONG_DOUBLE_T:
            case TK_LOCAL:
                LocalDeclStatement();
                break;
            default:
                if(_token != _SC(';')){
			CommaExpr();
			_fs->PopTarget();
		}
		}

		Expect(_SC(';'));
		_fs->SnoozeOpt();
		SQInteger jmppos = _fs->GetCurrentPos();
		SQInteger jzpos = -1;
		if(_token != _SC(';')) { CommaExpr(); _fs->AddInstruction(_OP_JZ, _fs->PopTarget()); jzpos = _fs->GetCurrentPos(); }
		Expect(_SC(';'));
		_fs->SnoozeOpt();
		SQInteger expstart = _fs->GetCurrentPos() + 1;
		if(_token != _SC(')')) {
			CommaExpr();
			_fs->PopTarget();
		}
		Expect(_SC(')'));
		_fs->SnoozeOpt();
		SQInteger expend = _fs->GetCurrentPos();
		SQInteger expsize = (expend - expstart) + 1;
		SQInstructionVec exp;
		if(expsize > 0) {
			for(SQInteger i = 0; i < expsize; i++)
				exp.push_back(_fs->GetInstruction(expstart + i));
			_fs->PopInstructions(expsize);
		}
		BEGIN_BREAKBLE_BLOCK()
		Statement();
		SQInteger continuetrg = _fs->GetCurrentPos();
		if(expsize > 0) {
			for(SQInteger i = 0; i < expsize; i++)
				_fs->AddInstruction(exp[i]);
		}
		_fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1, 0);
		if(jzpos>  0) _fs->SetIntructionParam(jzpos, 1, _fs->GetCurrentPos() - jzpos);
		END_SCOPE();

		END_BREAKBLE_BLOCK(continuetrg);
	}
	void ForEachStatement()
	{
		SQObject idxname, valname;
		Lex(); Expect(_SC('(')); valname = Expect(TK_IDENTIFIER);
		if(_token == _SC(',')) {
			idxname = valname;
			Lex(); valname = Expect(TK_IDENTIFIER);
		}
		else{
			idxname = _fs->CreateString(_SC("@INDEX@"));
		}
		Expect(TK_IN);

		//save the stack size
		BEGIN_SCOPE();
		//put the table in the stack(evaluate the table expression)
		Expression(); Expect(_SC(')'));
		SQInteger container = _fs->TopTarget();
		//push the index local var
		SQInteger indexpos = _fs->PushLocalVariable(idxname, _scope.nested);
		_fs->AddInstruction(_OP_LOADNULLS, indexpos,1);
		//push the value local var
		SQInteger valuepos = _fs->PushLocalVariable(valname, _scope.nested);
		_fs->AddInstruction(_OP_LOADNULLS, valuepos,1);
		//push reference index
		SQInteger itrpos = _fs->PushLocalVariable(_fs->CreateString(_SC("@ITERATOR@")), _scope.nested); //use invalid id to make it inaccessible
		_fs->AddInstruction(_OP_LOADNULLS, itrpos,1);
		SQInteger jmppos = _fs->GetCurrentPos();
		_fs->AddInstruction(_OP_FOREACH, container, 0, indexpos);
		SQInteger foreachpos = _fs->GetCurrentPos();
		_fs->AddInstruction(_OP_POSTFOREACH, container, 0, indexpos);
		//generate the statement code
		BEGIN_BREAKBLE_BLOCK()
		Statement();
		_fs->AddInstruction(_OP_JMP, 0, jmppos - _fs->GetCurrentPos() - 1);
		_fs->SetIntructionParam(foreachpos, 1, _fs->GetCurrentPos() - foreachpos);
		_fs->SetIntructionParam(foreachpos + 1, 1, _fs->GetCurrentPos() - foreachpos);
		END_BREAKBLE_BLOCK(foreachpos - 1);
		//restore the local variable stack(remove index,val and ref idx)
		_fs->PopTarget();
		END_SCOPE();
	}
	void SwitchStatement()
	{
		Lex(); Expect(_SC('(')); CommaExpr(true); Expect(_SC(')'));
		Expect(_SC('{'));
		SQInteger expr = _fs->TopTarget();
		bool bfirst = true;
		SQInteger tonextcondjmp = -1;
		SQInteger skipcondjmp = -1;
		SQInteger __nbreaks__ = _fs->_unresolvedbreaks.size();
		_fs->_breaktargets.push_back(0);
		while(_token == TK_CASE) {
			if(!bfirst) {
				_fs->AddInstruction(_OP_JMP, 0, 0);
				skipcondjmp = _fs->GetCurrentPos();
				_fs->SetIntructionParam(tonextcondjmp, 1, _fs->GetCurrentPos() - tonextcondjmp);
			}
			//condition
			Lex(); Expression(); Expect(_SC(':'));
			SQInteger trg = _fs->PopTarget();
			_fs->AddInstruction(_OP_EQ, trg, trg, expr);
			_fs->AddInstruction(_OP_JZ, trg, 0);
			//end condition
			if(skipcondjmp != -1) {
				_fs->SetIntructionParam(skipcondjmp, 1, (_fs->GetCurrentPos() - skipcondjmp));
			}
			tonextcondjmp = _fs->GetCurrentPos();
			BEGIN_SCOPE();
			Statements();
			END_SCOPE();
			bfirst = false;
		}
		if(tonextcondjmp != -1)
			_fs->SetIntructionParam(tonextcondjmp, 1, _fs->GetCurrentPos() - tonextcondjmp);
		if(_token == TK_DEFAULT) {
			Lex(); Expect(_SC(':'));
			BEGIN_SCOPE();
			Statements();
			END_SCOPE();
		}
		Expect(_SC('}'));
		_fs->PopTarget();
		__nbreaks__ = _fs->_unresolvedbreaks.size() - __nbreaks__;
		if(__nbreaks__ > 0)ResolveBreaks(_fs, __nbreaks__);
		_fs->_breaktargets.pop_back();
	}
	void FunctionStatement()
	{
		SQObject id;
		Lex(); id = Expect(TK_IDENTIFIER);
		CheckGlobalName(id, true);
		_fs->PushTarget(0);
		_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
		if(_token == TK_DOUBLE_COLON) Emit2ArgsOP(_OP_GET);

		while(_token == TK_DOUBLE_COLON) {
			Lex();
			id = Expect(TK_IDENTIFIER);
			_fs->AddInstruction(_OP_LOAD, _fs->PushTarget(), _fs->GetConstant(id));
			if(_token == TK_DOUBLE_COLON) Emit2ArgsOP(_OP_GET);
		}
		Expect(_SC('('));
		CreateFunction(id);
		_fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, 0);
		EmitDerefOp(_OP_NEWSLOT);
		_fs->PopTarget();
	}
	void ClassStatement()
	{
		SQExpState es;
		Lex();
		if(_token == TK_IDENTIFIER) {
		    SQObjectPtr str = SQString::Create(_ss(_vm), _lex._svalue);
		    CheckGlobalName(str, true);
		}
		es = _es;
		_es.donot_get = true;
		PrefixedExpr();
		if(_es.etype == EXPR) {
			Error(_SC("invalid class name"));
		}
		else if(_es.etype == OBJECT || _es.etype == BASE) {
			ClassExp();
			EmitDerefOp(_OP_NEWSLOT);
			_fs->PopTarget();
		}
		else {
			Error(_SC("cannot create a class in a local with the syntax(class <local>)"));
		}
		_es = es;
	}
	SQObject ExpectScalar()
	{
		SQObject val;
		val._type = OT_NULL; val._unVal.nInteger = 0; //shut up GCC 4.x
		switch(_token) {
			case TK_INTEGER:
				val._type = OT_INTEGER;
				val._unVal.nInteger = _lex._nvalue;
				break;
			case TK_FLOAT:
				val._type = OT_FLOAT;
				val._unVal.fFloat = _lex._fvalue;
				break;
			case TK_STRING_LITERAL:
				val = _fs->CreateString(_lex._svalue,_lex._longstr.size()-1);
				break;
			case '-':
				Lex();
				switch(_token)
				{
				case TK_INTEGER:
					val._type = OT_INTEGER;
					val._unVal.nInteger = -_lex._nvalue;
				break;
				case TK_FLOAT:
					val._type = OT_FLOAT;
					val._unVal.fFloat = -_lex._fvalue;
				break;
				default:
					Error(_SC("scalar expected : integer,float"));
				}
				break;
			default:
				Error(_SC("scalar expected : integer,float or string"));
		}
		Lex();
		return val;
	}
	void EnumStatement()
	{
		Lex();
		SQObject id = Expect(TK_IDENTIFIER);
		Expect(_SC('{'));

        //CheckLocalNameScope(id, _scope.nested);
		SQObjectPtr strongid = id;
		CheckLocalNameScope(id, _scope.nested);
        CheckConstsExists(strongid);
		SQObject table = _fs->CreateTable();
		//_fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,NOT_TABLE);
		SQInteger nval = 0;
		while(_token != _SC('}')) {
			SQObject key = Expect(TK_IDENTIFIER);
			SQObject val;
			if(_token == _SC('=')) {
				Lex();
				val = ExpectScalar();
			}
			else {
				val._type = OT_INTEGER;
				val._unVal.nInteger = nval++;
			}
			//SQInteger table = _fs->TopTarget(); //<<BECAUSE OF THIS NO COMMON EMIT FUNC IS POSSIBLE
            //_fs->AddInstruction(_OP_NEWSLOT, 0xFF, table, key, val);

			_table(table)->NewSlot(SQObjectPtr(key),SQObjectPtr(val));
			if(_token == ',') Lex();
		}

		ConstsNewSlot(SQObjectPtr(strongid),SQObjectPtr(table));
		strongid.Null();
		Lex();
	}
	void TryCatchStatement()
	{
		SQObject exid;
		Lex();
		_fs->AddInstruction(_OP_PUSHTRAP,0,0);
		_fs->_traps++;
		if(_fs->_breaktargets.size()) _fs->_breaktargets.top()++;
		if(_fs->_continuetargets.size()) _fs->_continuetargets.top()++;
		SQInteger trappos = _fs->GetCurrentPos();
		{
			BEGIN_SCOPE();
			Statement();
			END_SCOPE();
		}
		_fs->_traps--;
		_fs->AddInstruction(_OP_POPTRAP, 1, 0);
		if(_fs->_breaktargets.size()) _fs->_breaktargets.top()--;
		if(_fs->_continuetargets.size()) _fs->_continuetargets.top()--;
		_fs->AddInstruction(_OP_JMP, 0, 0);
		SQInteger jmppos = _fs->GetCurrentPos();
		_fs->SetIntructionParam(trappos, 1, (_fs->GetCurrentPos() - trappos));
		Expect(TK_CATCH); Expect(_SC('(')); exid = Expect(TK_IDENTIFIER); Expect(_SC(')'));
		{
			BEGIN_SCOPE();
			SQInteger ex_target = _fs->PushLocalVariable(exid, _scope.nested);
			_fs->SetIntructionParam(trappos, 0, ex_target);
			Statement();
			_fs->SetIntructionParams(jmppos, 0, (_fs->GetCurrentPos() - jmppos), 0);
			END_SCOPE();
		}
	}
	void FunctionExp(SQInteger ftype,bool lambda = false)
	{
		Lex(); Expect(_SC('('));
		SQObjectPtr dummy;
		CreateFunction(dummy,lambda);
		_fs->AddInstruction(_OP_CLOSURE, _fs->PushTarget(), _fs->_functions.size() - 1, ftype == TK_FUNCTION?0:1);
	}
	void ClassExp()
	{
		SQInteger base = -1;
		SQInteger attrs = -1;
		if(_token == TK_EXTENDS) {
			Lex(); Expression();
			base = _fs->TopTarget();
		}
		if(_token == TK_ATTR_OPEN) {
			Lex();
			_fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(),0,NOT_TABLE);
			ParseTableOrClass(_SC(','),TK_ATTR_CLOSE);
			attrs = _fs->TopTarget();
		}
		Expect(_SC('{'));
		if(attrs != -1) _fs->PopTarget();
		if(base != -1) _fs->PopTarget();
		_fs->AddInstruction(_OP_NEWOBJ, _fs->PushTarget(), base, attrs,NOT_CLASS);
		ParseTableOrClass(_SC(';'),_SC('}'));
	}
	void DeleteExpr()
	{
		SQExpState es;
		Lex();
		es = _es;
		_es.donot_get = true;
		PrefixedExpr();
		if(_es.etype==EXPR) Error(_SC("can't delete an expression"));
		if(_es.etype==OBJECT || _es.etype==BASE) {
			Emit2ArgsOP(_OP_DELETE);
		}
		else {
			Error(_SC("cannot delete an (outer) local"));
		}
		_es = es;
	}
	void PrefixIncDec(SQInteger token)
	{
		SQExpState  es;
		SQInteger diff = (token==TK_MINUSMINUS) ? -1 : 1;
		Lex();
		es = _es;
		_es.donot_get = true;
		PrefixedExpr();
		if(_es.etype==EXPR) {
			Error(_SC("can't '++' or '--' an expression"));
		}
		else if(_es.etype==OBJECT || _es.etype==BASE) {
			Emit2ArgsOP(_OP_INC, diff);
		}
		else if(_es.etype==LOCAL) {
			SQInteger src = _fs->TopTarget();
			_fs->AddInstruction(_OP_INCL, src, src, 0, diff);

		}
		else if(_es.etype==OUTER) {
			SQInteger tmp = _fs->PushTarget();
			_fs->AddInstruction(_OP_GETOUTER, tmp, _es.epos);
			_fs->AddInstruction(_OP_INCL,     tmp, tmp, 0, diff);
			_fs->AddInstruction(_OP_SETOUTER, tmp, _es.epos, tmp);
		}
		_es = es;
	}
	void CreateFunction(SQObject &name,bool lambda = false)
	{
		SQFuncState *funcstate = _fs->PushChildState(_ss(_vm));
		funcstate->_name = name;
		SQObject paramname;
		funcstate->AddParameter(_fs->CreateString(_SC("this")), _scope.nested+1);
		funcstate->_sourcename = _sourcename;
		SQInteger defparams = 0;
		SQInteger is_reference = 0;
		while(_token!=_SC(')')) {
            is_reference = 0; //reset is_reference
			if(_token == TK_VARPARAMS) {
				if(defparams > 0) Error(_SC("function with default parameters cannot have variable number of parameters"));
				funcstate->AddParameter(_fs->CreateString(_SC("vargv")), _scope.nested+1);
				funcstate->_varparams = true;
				Lex();
				if(_token != _SC(')')) Error(_SC("expected ')'"));
				break;
			}
			else {
			    if(_token == _SC('&')){
                    is_reference = 1;
                    Lex();
                }
				paramname = Expect(TK_IDENTIFIER);
				funcstate->AddParameter(paramname, _scope.nested+1, is_reference ? _VAR_REFERENCE : _VAR_ANY);
				if(_token == _SC('=')) {
				    if(is_reference) Error(_SC("parameter passed by reference can't have default value"));
					Lex();
					if(_token == _SC('[') || _token == _SC('{')) Error(_SC("default parameter with array/table values not supported"));
					Expression();
					funcstate->AddDefaultParam(_fs->TopTarget());
					defparams++;
				}
				else {
					if(defparams > 0) Error(_SC("expected '='"));
				}
				if(_token == _SC(',')) Lex();
				else if(_token != _SC(')')) Error(_SC("expected ')' or ','"));
			}
		}
		Expect(_SC(')'));
		for(SQInteger n = 0; n < defparams; n++) {
			_fs->PopTarget();
		}

		SQFuncState *currchunk = _fs;
		_fs = funcstate;
		if(lambda) {
			Expression();
			_fs->AddInstruction(_OP_RETURN, 1, _fs->PopTarget());}
		else {
			Statement(false);
		}
		funcstate->AddLineInfos(_lex._prevtoken == _SC('\n')?_lex._lasttokenline:_lex._currentline, _lineinfo, true);
        funcstate->AddInstruction(_OP_RETURN, -1);
		funcstate->SetStackSize(0);

		SQFunctionProto *func = funcstate->BuildProto();
#ifdef _DEBUG_DUMP
		funcstate->Dump(func);
#endif
		_fs = currchunk;
		_fs->_functions.push_back(func);
		_fs->PopChildState();
	}
	void ResolveBreaks(SQFuncState *funcstate, SQInteger ntoresolve)
	{
		while(ntoresolve > 0) {
			SQInteger pos = funcstate->_unresolvedbreaks.back();
			funcstate->_unresolvedbreaks.pop_back();
			//set the jmp instruction
			funcstate->SetIntructionParams(pos, 0, funcstate->GetCurrentPos() - pos, 0);
			ntoresolve--;
		}
	}
	void ResolveContinues(SQFuncState *funcstate, SQInteger ntoresolve, SQInteger targetpos)
	{
		while(ntoresolve > 0) {
			SQInteger pos = funcstate->_unresolvedcontinues.back();
			funcstate->_unresolvedcontinues.pop_back();
			//set the jmp instruction
			funcstate->SetIntructionParams(pos, 0, targetpos - pos, 0);
			ntoresolve--;
		}
	}
private:
	SQInteger _token;
	SQFuncState *_fs;
	SQObjectPtr _sourcename;
	SQLexer _lex;
	bool _lineinfo;
	bool _raiseerror;
	bool _show_warnings;
	SQInteger _debugline;
	SQInteger _debugop;
	SQExpState   _es;
	SQScope _scope;
	SQChar *compilererror;
	jmp_buf _errorjmp;
	SQVM *_vm;
	SQObjectPtrVec _scope_consts;
	SQObjectPtr _globals;
};

bool Compile(SQVM *vm,SQLEXREADFUNC rg, SQUserPointer up, const SQChar *sourcename, SQObjectPtr &out,
             bool raiseerror, bool lineinfo, bool show_warnings)
{
	SQCompiler p(vm, rg, up, sourcename, raiseerror, lineinfo, show_warnings);
	return p.Compile(out);
}

#endif
