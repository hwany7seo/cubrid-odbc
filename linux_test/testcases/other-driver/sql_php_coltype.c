#include <stdio.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include "test_util.h"

/*
 * PHP-ODBC-001 - SQLColAttribute must return negative SQL type codes intact.
 *
 * Before the fix odbc_col_attribute() read every numeric IRD field through a
 * long *, while SQL_DESC_CONCISE_TYPE/SQL_DESC_TYPE are stored as short.  The
 * upper bytes of the caller's buffer stayed zero, so every negative SQL type
 * code came back as a large positive short instead - e.g. SQL_LONGVARCHAR
 * (-1) as 65535, SQL_BINARY (-2) as 65534, SQL_VARBINARY (-3) as 65533,
 * SQL_LONGVARBINARY (-4) as 65532.  ext/odbc then missed its long-type cases
 * and sized a bound buffer from SQL_DESC_DISPLAY_SIZE, which is
 * MAX_CUBRID_CHAR_LEN for those columns - a 1 GiB emalloc() per column.
 *
 * Columns below cover every CUBRID type that maps onto one of these four
 * negative SQL type codes (src/odbc_type.c odbc_type_by_cci()):
 *   STRING/VARCHAR(>8000)        -> SQL_LONGVARCHAR   (c_string, c_vc20000)
 *   SET/MULTISET/SEQUENCE(LIST)  -> SQL_LONGVARCHAR   (c_set)
 *   CLOB                         -> SQL_LONGVARCHAR   (c_clob)
 *   BIT (fixed length)           -> SQL_BINARY        (c_bit)
 *   BIT VARYING                  -> SQL_VARBINARY     (c_bitvar)
 *   BLOB                         -> SQL_LONGVARBINARY (c_blob)
 */

#define TABLE "t_php_coltype"

/* ext/odbc stops binding a column and streams it instead for these types. */
#define IS_LONG_TYPE(t)							\
  ((t) == SQL_LONGVARCHAR || (t) == SQL_WLONGVARCHAR			\
   || (t) == SQL_BINARY || (t) == SQL_VARBINARY || (t) == SQL_LONGVARBINARY)

int
sql_php_coltype (int case_num, char *dsn)
{
  RETCODE retcode;
  SQLHENV env;
  SQLHDBC dbc;
  SQLHSTMT stmt;
  SQLSMALLINT ncols = 0, i;
  SQLLEN v;
  int nok = 0;
  int oversized_bound = 0;

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
			   "c_string STRING, c_vc20000 VARCHAR(20000), c_vc100 VARCHAR(100),"
			   "c_char CHAR(10), c_bitvar BIT VARYING(64), c_int INTEGER,"
			   "c_bit BIT(8), c_set SET(INTEGER), c_clob CLOB, c_blob BLOB)", SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);
  SQLFreeStmt (stmt, SQL_CLOSE);

  retcode = SQLExecDirect (stmt, (SQLCHAR *) "SELECT * FROM " TABLE, SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);

  SQLNumResultCols (stmt, &ncols);
  ReportEqual (nok, "column count", ncols, 10);

  v = 0;
  SQLColAttribute (stmt, 1, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_string  CONCISE_TYPE", v, SQL_LONGVARCHAR);

  v = 0;
  SQLColAttribute (stmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_string  DESC_TYPE", v, SQL_LONGVARCHAR);

  v = 0;
  SQLColAttribute (stmt, 2, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_vc20000 CONCISE_TYPE", v, SQL_LONGVARCHAR);

  v = 0;
  SQLColAttribute (stmt, 3, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_vc100   CONCISE_TYPE", v, SQL_VARCHAR);

  v = 0;
  SQLColAttribute (stmt, 4, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_char    CONCISE_TYPE", v, SQL_CHAR);

  v = 0;
  SQLColAttribute (stmt, 5, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_bitvar  CONCISE_TYPE", v, SQL_VARBINARY);

  v = 0;
  SQLColAttribute (stmt, 6, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_int     CONCISE_TYPE", v, SQL_INTEGER);

  v = 0;
  SQLColAttribute (stmt, 7, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_bit     CONCISE_TYPE", v, SQL_BINARY);

  v = 0;
  SQLColAttribute (stmt, 8, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_set     CONCISE_TYPE", v, SQL_LONGVARCHAR);

  v = 0;
  SQLColAttribute (stmt, 9, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_clob    CONCISE_TYPE", v, SQL_LONGVARCHAR);

  v = 0;
  SQLColAttribute (stmt, 10, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_blob    CONCISE_TYPE", v, SQL_LONGVARBINARY);

  /* The combination that actually exhausts PHP's memory limit: a column that
     ext/odbc will bind, carrying a display size it has to allocate up front. */
  for (i = 1; i <= ncols; i++)
    {
      SQLLEN type = 0, display_size = 0;

      SQLColAttribute (stmt, i, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &type);
      SQLColAttribute (stmt, i, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &display_size);

      if (!IS_LONG_TYPE (type) && display_size > 8000)
	{
	  printf ("  column %d: bound type %ld with display size %ld\n",
		  i, (long) type, (long) display_size);
	  oversized_bound++;
	}
    }
  ReportEqual (nok, "columns needing an oversized bound buffer", oversized_bound, 0);

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
