#include <stdio.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include "test_util.h"

/*
 * PHP-ODBC-002 - SQL_COLUMN_PRECISION must report the column size.
 *
 * odbc_field_len() maps onto the ODBC 2.x identifier SQL_COLUMN_PRECISION,
 * which for character, binary and datetime types means "column size" and has to
 * come from SQL_DESC_LENGTH.  The driver forwards it to SQL_DESC_PRECISION,
 * which odbc_set_ird() deliberately sets to 0 for those types, so every string
 * column reports length 0.  record->precision is a short and could not hold
 * MAX_CUBRID_CHAR_LEN anyway.
 *
 * Expected to fail until the SQL_COLUMN_PRECISION branch is applied.
 */

#define TABLE "t_php_collen"
#define MAX_CUBRID_CHAR_LEN 1073741823

int
sql_php_collen (int case_num, char *dsn)
{
  RETCODE retcode;
  SQLHENV env;
  SQLHDBC dbc;
  SQLHSTMT stmt;
  SQLLEN v;
  int nok = 0;

  retcode = SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
  retcode = SQLSetEnvAttr (env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
  retcode = SQLAllocHandle (SQL_HANDLE_DBC, env, &dbc);
  AreNotEqual (retcode, SQL_ERROR);

  retcode = SQLConnect (dbc, (SQLCHAR *) dsn, SQL_NTS, NULL, 0, NULL, 0);
  AreNotEqual (retcode, SQL_ERROR);
  SQLSetConnectAttr (dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, 0);

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, dbc, &stmt);
  AreNotEqual (retcode, SQL_ERROR);

  SQLExecDirect (stmt, (SQLCHAR *) "DROP TABLE IF EXISTS " TABLE, SQL_NTS);
  SQLFreeStmt (stmt, SQL_CLOSE);

  retcode = SQLExecDirect (stmt, (SQLCHAR *) "CREATE TABLE " TABLE " ("
			   "c_string STRING, c_vc100 VARCHAR(100), c_char CHAR(10),"
			   "c_date DATE, c_int INTEGER, c_num NUMERIC(10,2))", SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);
  SQLFreeStmt (stmt, SQL_CLOSE);

  retcode = SQLExecDirect (stmt, (SQLCHAR *) "SELECT * FROM " TABLE, SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);

  v = 0;
  SQLColAttribute (stmt, 1, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_string COLUMN_PRECISION", v, MAX_CUBRID_CHAR_LEN);

  v = 0;
  SQLColAttribute (stmt, 2, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_vc100  COLUMN_PRECISION", v, 100);

  v = 0;
  SQLColAttribute (stmt, 3, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_char   COLUMN_PRECISION", v, 10);

  v = 0;
  SQLColAttribute (stmt, 4, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_date   COLUMN_PRECISION", v, 10);

  v = 0;
  SQLColAttribute (stmt, 5, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_int    COLUMN_PRECISION", v, 10);

  v = 0;
  SQLColAttribute (stmt, 6, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_num    COLUMN_PRECISION", v, 10);

  /* SQL_DESC_LENGTH already carries the right value; only the 2.x alias is wrong. */
  v = 0;
  SQLColAttribute (stmt, 2, SQL_DESC_LENGTH, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_vc100  DESC_LENGTH", v, 100);

  v = 0;
  SQLColAttribute (stmt, 3, SQL_DESC_LENGTH, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_char   DESC_LENGTH", v, 10);

  SQLFreeStmt (stmt, SQL_CLOSE);
  SQLExecDirect (stmt, (SQLCHAR *) "DROP TABLE " TABLE, SQL_NTS);
  SQLFreeHandle (SQL_HANDLE_STMT, stmt);

  retcode = SQLDisconnect (dbc);
  AreNotEqual (retcode, SQL_ERROR);
  retcode = SQLFreeHandle (SQL_HANDLE_DBC, dbc);
  retcode = SQLFreeHandle (SQL_HANDLE_ENV, env);
  AreNotEqual (retcode, SQL_ERROR);

  return nok ? 1 : SQL_SUCCESS;
}
