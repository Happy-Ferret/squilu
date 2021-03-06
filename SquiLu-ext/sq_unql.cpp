/* Code generated by script */
#include "sq_sqlite3.h"

extern "C" {
#include "xjd1.h"
}

static const SQChar *xjd1_TAG = "xjd1";
static const SQChar *xjd1_Stmt_TAG = "xjd1Stmt";

static SQRESULT get_xjd1_instance(HSQUIRRELVM v, SQInteger idx, xjd1 **db)
{
    SQRESULT _rc_;
    if((_rc_ = sq_getinstanceup(v,idx,(SQUserPointer*)db,(void*)xjd1_TAG)) < 0) return _rc_;
    if(!*db) return sq_throwerror(v, _SC("database is closed"));
    return _rc_;
}

static SQRESULT get_xjd1_stmt_instance(HSQUIRRELVM v, SQInteger idx, xjd1_stmt **stmt)
{
    SQRESULT _rc_;
    if((_rc_ = sq_getinstanceup(v,idx,(SQUserPointer*)stmt,(void*)xjd1_Stmt_TAG)) < 0) return _rc_;
    if(!*stmt) return sq_throwerror(v, _SC("statement is closed"));
    return _rc_;
}

static SQRESULT get_sqlite3_instance(HSQUIRRELVM v, SQInteger idx, sq_sqlite3_sdb **sdb)
{
    SQRESULT _rc_;
    if((_rc_ = sq_getinstanceup(v,idx,(SQUserPointer*)sdb,(void*)SQLite3_TAG)) < 0) return _rc_;
    if(!*sdb) return sq_throwerror(v, _SC("database is closed"));
    return _rc_;
}

#define push_xjd1_null(v) {sq_pushstring(v, nullName, sizeof(nullName)-1);sq_get(v, 1);}

#define GET_xjd1_INSTANCE_AT(idx) \
	xjd1 *self=NULL; \
	if((_rc_ = get_xjd1_instance(v,idx,&self)) < 0) return _rc_;

#define GET_xjd1_INSTANCE() GET_xjd1_INSTANCE_AT(1)

#define GET_xjd1_stmt_INSTANCE()  \
	xjd1_stmt *self=NULL; \
	if((_rc_ = get_xjd1_stmt_instance(v,1,&self)) < 0) return _rc_;


static SQRESULT sq_xjd1_stmt_releasehook(SQUserPointer p, SQInteger size, void */*ep*/)
{
    xjd1_stmt *stmt = ((xjd1_stmt *)p);
    if(stmt) xjd1_stmt_delete(stmt);
    return 0;
}

static SQRESULT sq_xjd1_stmt_constructor(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    _rc_ = SQ_ERROR;
    GET_xjd1_INSTANCE_AT(2);
    SQ_GET_STRING(v, 3, sql);
    xjd1_stmt *stmt = 0;
    if(xjd1_stmt_new(self, sql, &stmt, 0) != XJD1_OK)
    {
        return sq_throwerror(v, xjd1_errmsg(self));
    }
    sq_setinstanceup(v, 1, stmt); //replace self for this instance with this new sqlite3_stmt
    sq_setreleasehook(v,1, sq_xjd1_stmt_releasehook);
    return SQ_OK;
}

static SQRESULT sq_xjd1_stmt_finalize(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_stmt_INSTANCE();
    sq_setinstanceup(v, 1, 0); //next calls will fail with "statement is closed"
    sq_pushinteger(v, xjd1_stmt_delete(self));
    return 1;
}

static SQRESULT sq_xjd1_stmt_step(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_stmt_INSTANCE();
    sq_pushinteger(v, xjd1_stmt_step(self));
    return 1;
}

static SQRESULT sq_xjd1_stmt_next_row(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_stmt_INSTANCE();
    sq_pushbool(v, xjd1_stmt_step(self) == XJD1_ROW);
    return 1;
}

static SQRESULT sq_xjd1_stmt_rewind(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_stmt_INSTANCE();
    sq_pushinteger(v, xjd1_stmt_rewind(self));
    return 1;
}

static SQRESULT sq_xjd1_stmt_value(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_stmt_INSTANCE();
    const char* value;
    int rc = xjd1_stmt_value(self, &value);
    if(rc == XJD1_OK) sq_pushstring(v, value, -1);
    else return sq_throwerror(v, _SC("Erro when getting value from xjd1_stmt -> %d"), rc);
    return 1;
}

#define _DECL_FUNC(name,nparams,tycheck, isStatic) {_SC(#name),  sq_xjd1_stmt_##name,nparams,tycheck, isStatic}
static SQRegFunction sq_xjd1_stmt_methods[] =
{
    _DECL_FUNC(constructor,  3, _SC("xxs"), SQFalse),

    _DECL_FUNC(finalize,  1, _SC("x"), SQFalse),
    _DECL_FUNC(step,  1, _SC("x"), SQFalse),
    _DECL_FUNC(next_row,  1, _SC("x"), SQFalse),
    _DECL_FUNC(rewind,  1, _SC("x"), SQFalse),
    _DECL_FUNC(value,  1, _SC("x"), SQFalse),
    {0,0}
};
#undef _DECL_FUNC


static SQRESULT sq_xjd1_close_release(HSQUIRRELVM v, xjd1 *db)
{
    SQRESULT rc = SQ_ERROR;
    if(db)
    {
        int close_rc = xjd1_close(db) == XJD1_OK;
        if(!v || (v && close_rc))
        {
            rc = SQ_OK;
        }
        else
        {
            if(v) return sq_throwerror(v, xjd1_errmsg(db));
        }
    }
    return rc;
}

static SQRESULT sq_xjd1_releasehook(SQUserPointer p, SQInteger size, void */*ep*/)
{
    xjd1 *db = ((xjd1 *)p);
    sq_xjd1_close_release(0, db);
    return 0;
}

static SQRESULT sq_xjd1_constructor(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    xjd1 *db;
    sq_sqlite3_sdb *sdb = NULL;
    int rc = XJD1_ERROR;
    switch(sq_gettype(v, 2))
    {
    case OT_STRING:
        {
            SQ_GET_STRING(v, 2, dbname);
            rc = xjd1_open(0, dbname, &db);
        }
        break;
    case OT_INSTANCE:
        {
            if(get_sqlite3_instance(v, 2, &sdb) == SQ_OK)
            {
                rc = xjd1_open_with_db(0, sdb->db, &db);
            }
        }
        break;
    default:
        return sq_throwerror(v, _SC("Wrong parameters to open xjd1 file name or sqlite3 expected !"));
    }
    if(rc != SQ_OK) return sq_throwerror(v, _SC("Failed to open database ! %d"), rc);
    sq_setinstanceup(v, 1, db);
    sq_setreleasehook(v,1, sq_xjd1_releasehook);

    return SQ_OK;
}

static SQRESULT sq_xjd1_close(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_INSTANCE();
    if(sq_xjd1_close_release(v, self) != SQ_OK) return SQ_ERROR;
    sq_setinstanceup(v, 1, 0); //next calls will fail with "database is closed"
    return 0;
}

static SQRESULT sq_xjd1_complete(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    SQ_GET_STRING(v, 2, szSql);
    sq_pushinteger(v, xjd1_complete(szSql));
    return 1;
}

/* stmt * prepare( const char * sql  ) */
static SQRESULT sq_xjd1_prepare(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    //GET_xjd1_INSTANCE();
    SQ_GET_STRING(v, 2, sql);
    sq_pushstring(v, xjd1_Stmt_TAG, -1);
    if(sq_getonroottable(v) == SQ_ERROR) return SQ_ERROR;
    sq_pushroottable(v);
    sq_push(v, 1);
    sq_push(v, 2);
    if(sq_call(v, 3, SQTrue, SQFalse) != SQ_OK) return SQ_ERROR;
//printf("%d:%s\n", __LINE__, __FILE__);
    return 1;
}

static SQRESULT sq_xjd1_errcode_name(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_INSTANCE();
    sq_pushstring(v, xjd1_errcode_name(self), -1);
    return 1;
}

static SQRESULT sq_xjd1_errmsg(HSQUIRRELVM v)
{
    SQ_FUNC_VARS_NO_TOP(v);
    GET_xjd1_INSTANCE();
    sq_pushstring(v, xjd1_errmsg(self), -1);
    return 1;
}


#define _DECL_FUNC(name,nparams,tycheck) {_SC(#name),  sq_xjd1_##name,nparams,tycheck}
static SQRegFunction sq_xjd1_methods[] =
{
    _DECL_FUNC(constructor,  2, _SC("x x|s")),
    _DECL_FUNC(close,  1, _SC("x")),

    _DECL_FUNC(errcode_name,  1, _SC("x")),
    _DECL_FUNC(errmsg,  1, _SC("x")),

    _DECL_FUNC(complete,  1, _SC("s")),
    _DECL_FUNC(prepare,  2, _SC("xs")),
    {0,0}
};

#undef _DECL_FUNC

#define INT_CONST(v,num) 	sq_pushstring(v,_SC(#num),-1);sq_pushinteger(v,num);sq_newslot(v,-3,SQTrue);



#ifdef __cplusplus
extern "C" {
#endif

    SQRESULT sqext_register_xjd1(HSQUIRRELVM v)
    {

        sq_pushstring(v,xjd1_TAG,-1);
        sq_newclass(v,SQFalse);
        sq_settypetag(v,-1,(void*)xjd1_TAG);
        sq_insert_reg_funcs(v, sq_xjd1_methods);

        INT_CONST(v,XJD1_OK);
        INT_CONST(v,XJD1_ERROR);
        INT_CONST(v,XJD1_MISUSE);
        INT_CONST(v,XJD1_NOMEM);
        INT_CONST(v,XJD1_UNKNOWN);
        INT_CONST(v,XJD1_SYNTAX);
        INT_CONST(v,XJD1_ERROR_OPEN_DB);
        INT_CONST(v,XJD1_ROW);
        INT_CONST(v,XJD1_DONE);

        sq_newslot(v,-3,SQTrue);

        sq_pushstring(v, xjd1_Stmt_TAG,-1);
        sq_newclass(v,SQFalse);
        sq_settypetag(v,-1,(void*)xjd1_Stmt_TAG);
        sq_insert_reg_funcs(v, sq_xjd1_stmt_methods);

        INT_CONST(v,XJD1_OK);
        INT_CONST(v,XJD1_ROW);
        INT_CONST(v,XJD1_DONE);

        sq_newslot(v,-3,SQTrue);
        return 1;
    }

#ifdef __cplusplus
}
#endif

