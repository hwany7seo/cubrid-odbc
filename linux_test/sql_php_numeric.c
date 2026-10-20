#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include "test_util.h"

/*
 * PHP-ODBC-004 - NUMERIC/DECIMAL must report a usable display size.
 *
 * odbc_display_size() branches on octet_length == -1 instead of
 * display_size == -1.  The NUMERIC and DECIMAL rows are the only ones with a
 * constant octet_length and a delegated display size, so they fall through to
 * "return display_size" and hand out -1.  ext/odbc sizes its bound buffer as
 * display_size + 1, so it binds a zero-length buffer and later reads back
 * uninitialised heap - in practice a leaked pointer value.
 *
 * Expected to fail until odbc_display_size() checks its own field.
 */

#define TABLE "t_php_numeric"

int
sql_php_numeric (int case_num, char *dsn)
{
  RETCODE retcode;
  SQLHENV env;
  SQLHDBC dbc;
  SQLHSTMT stmt;
  SQLLEN v;
  int nok = 0;
  int col;
  static const char *const expected[3] = { "1234.56", "42.125", "7" };

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

  retcode = SQLExecDirect (stmt, (SQLCHAR *)
			   "CREATE TABLE " TABLE " (n NUMERIC(10,2), d DECIMAL(8,3), i INTEGER)", SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);
  SQLFreeStmt (stmt, SQL_CLOSE);

  retcode = SQLExecDirect (stmt, (SQLCHAR *) "INSERT INTO " TABLE " VALUES (1234.56, 42.125, 7)", SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);
  SQLFreeStmt (stmt, SQL_CLOSE);

  retcode = SQLExecDirect (stmt, (SQLCHAR *) "SELECT * FROM " TABLE, SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);

  /* display_size_decimal() is precision + 2, for the sign and the point. */
  v = 0;
  SQLColAttribute (stmt, 1, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &v);
  ReportEqual (nok, "n NUMERIC(10,2) DISPLAY_SIZE", v, 12);

  v = 0;
  SQLColAttribute (stmt, 2, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &v);
  ReportEqual (nok, "d DECIMAL(8,3)  DISPLAY_SIZE", v, 10);

  v = 0;
  SQLColAttribute (stmt, 3, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &v);
  ReportEqual (nok, "i INTEGER       DISPLAY_SIZE", v, 11);

  retcode = SQLFetch (stmt);
  AreNotEqual (retcode, SQL_ERROR);

  /* ext/odbc allocates its column buffer as display_size + 1 and binds it with
     SQLBindCol; SQLGetData with that same buffer length reproduces the read. */
  for (col = 1; col <= 3; col++)
    {
      char value[64];
      SQLLEN display_size = 0, bind_len, ind = 0;

      SQLColAttribute (stmt, col, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &display_size);

      /* ext/odbc computes emalloc(display_size + 1) with no lower bound, so a
	 negative display_size (the actual bug) yields a 0-byte allocation.
	 A 0-length SQLGetData call is spec-legal and safe here: the driver
	 reports the true length through ind but writes nothing, leaving
	 value as the empty string we pre-set below - exactly the truncation
	 PHP hits. Only clamp the upper bound, to stay inside our buffer. */
      bind_len = display_size + 1;
      if (bind_len < 0)
	{
	  bind_len = 0;
	}
      else if (bind_len > (SQLLEN) sizeof (value))
	{
	  bind_len = sizeof (value);
	}

      value[0] = '\0';
      SQLGetData (stmt, col, SQL_C_CHAR, value, bind_len, &ind);
      value[sizeof (value) - 1] = '\0';

      ReportEqualStr (nok, "value through a display-size buffer", value, expected[col - 1]);
    }

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
