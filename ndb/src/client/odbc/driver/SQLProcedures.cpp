/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "driver.hpp"

#if ODBCVER >= 0x0000
SQLRETURN SQL_API
SQLProcedures(
    SQLHSTMT hstmt,
    SQLCHAR* szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLCHAR* szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLCHAR* szProcName,
    SQLSMALLINT cbProcName)
{
#ifndef auto_SQLProcedures
    const char* const sqlFunction = "SQLProcedures";
    Ctx ctx;
    ctx_log1(("*** not implemented: %s", sqlFunction));
    return SQL_ERROR;
#else
    driver_enter(SQL_API_SQLPROCEDURES);
    const char* const sqlFunction = "SQLProcedures";
    HandleRoot* const pRoot = HandleRoot::instance();
    HandleStmt* pStmt = pRoot->findStmt(hstmt);
    if (pStmt == 0) {
	driver_exit(SQL_API_SQLPROCEDURES);
        return SQL_INVALID_HANDLE;
    }
    Ctx& ctx = *new Ctx;
    ctx.logSqlEnter(sqlFunction);
    if (ctx.ok())
        pStmt->sqlProcedures(
            ctx,
            &szCatalogName,
            cbCatalogName,
            &szSchemaName,
            cbSchemaName,
            &szProcName,
            cbProcName
        );
    pStmt->saveCtx(ctx);
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    driver_exit(SQL_API_SQLPROCEDURES);
    return ret;
#endif
}
#endif // ODBCVER >= 0x0000
