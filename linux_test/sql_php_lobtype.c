#include <stdio.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include "test_util.h"

/*
 * PHP-ODBC-003 - CLOB/BLOB/ENUM/JSON must describe themselves.
 *
 * odbc_data_type_info_set[] has no BLOB/CLOB row, so odbc_type_name() and
 * odbc_display_size() both miss and the columns report an empty type name with
 * size 0.  odbc_set_desc_field() also rejects SQL_BLOB/SQL_CLOB as a verbose
 * type, leaving SQL_DESC_TYPE at its SQL_C_DEFAULT (99) initial value.
 *
 * ENUM and JSON are not in odbc_type_by_cci() at all; its unsupported-type
 * sentinel is -1, which collides with SQL_LONGVARCHAR, so they silently look
 * like long strings of MAX_CUBRID_CHAR_LEN.
 *
 * Expected to fail until the type table and odbc_type_by_cci() are extended.
 */

#define TABLE "t_php_lobtype"
#define MAX_CUBRID_CHAR_LEN 1073741823

int
sql_php_lobtype (int case_num, char *dsn)
{
  RETCODE retcode;
  SQLHENV env;
  SQLHDBC dbc;
  SQLHSTMT stmt;
  char name[128];
  SQLSMALLINT namelen;
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
			   "c_clob CLOB, c_blob BLOB, c_enum ENUM('a','b'), c_json JSON)", SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);
  SQLFreeStmt (stmt, SQL_CLOSE);

  retcode = SQLExecDirect (stmt, (SQLCHAR *) "SELECT * FROM " TABLE, SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);

  name[0] = '\0';
  SQLColAttribute (stmt, 1, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
  ReportEqualStr (nok, "c_clob TYPE_NAME", name, "CLOB");

  name[0] = '\0';
  SQLColAttribute (stmt, 2, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
  ReportEqualStr (nok, "c_blob TYPE_NAME", name, "BLOB");

  name[0] = '\0';
  SQLColAttribute (stmt, 3, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
  ReportEqualStr (nok, "c_enum TYPE_NAME", name, "ENUM");

  name[0] = '\0';
  SQLColAttribute (stmt, 4, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
  ReportEqualStr (nok, "c_json TYPE_NAME", name, "JSON");

  v = 0;
  SQLColAttribute (stmt, 1, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_clob CONCISE_TYPE", v, SQL_LONGVARCHAR);

  v = 0;
  SQLColAttribute (stmt, 2, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_blob CONCISE_TYPE", v, SQL_LONGVARBINARY);

  v = 0;
  SQLColAttribute (stmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_clob DESC_TYPE", v, SQL_LONGVARCHAR);

  v = 0;
  SQLColAttribute (stmt, 2, SQL_DESC_TYPE, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_blob DESC_TYPE", v, SQL_LONGVARBINARY);

  v = 0;
  SQLColAttribute (stmt, 1, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_clob OCTET_LENGTH", v, MAX_CUBRID_CHAR_LEN);

  v = 0;
  SQLColAttribute (stmt, 2, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL, &v);
  ReportEqual (nok, "c_blob OCTET_LENGTH", v, MAX_CUBRID_CHAR_LEN);

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
