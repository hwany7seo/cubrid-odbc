#include "stdafx.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <odbcss.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestCPP
{
  /*
   * PHP_ODBC_00x - defects the PHP ext/odbc suite
   * (cubrid-driverlink-test/php-pdo-odbc/to_php_odbc) runs into.
   * Analysis: cubrid-driverlink-test/issue_odbc/php/
   * Linux counterparts: linux_test/sql_php_*.c
   */
  TEST_CLASS (UnitTest_Issue)
  {
public:
#define MAX_CUBRID_CHAR_LEN 1073741823

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
    TEST_METHOD (PHP_APIS_1117_ColAttribute_TypeCode)
    {
      RETCODE retcode;
      SQLHENV hEnv;
      SQLHDBC hDbc;
      SQLHSTMT hStmt;
      SQLSMALLINT numCols = 0;
      SQLLEN v;

      SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
      SQLSetEnvAttr (hEnv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
      SQLAllocHandle (SQL_HANDLE_DBC, hEnv, &hDbc);

      retcode = SQLDriverConnect (hDbc, NULL,
				  L"DRIVER=CUBRID Driver Unicode;DB_NAME=demodb;SERVER=test-db-server;PORT=33000;UID=dba;PWD=;CHARSET=utf-8;AUTOCOMMIT=ON",
				  SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);
      SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hStmt);

      SQLExecDirect (hStmt, L"DROP TABLE IF EXISTS t_php_coltype", SQL_NTS);
      retcode = SQLExecDirect (hStmt,
			       L"CREATE TABLE t_php_coltype (c_string STRING, c_vc20000 VARCHAR(20000),"
			       L" c_vc100 VARCHAR(100), c_char CHAR(10), c_bitvar BIT VARYING(64), c_int INTEGER,"
			       L" c_bit BIT(8), c_set SET(INTEGER), c_clob CLOB, c_blob BLOB)", SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      retcode = SQLExecDirect (hStmt, L"SELECT * FROM t_php_coltype", SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      SQLNumResultCols (hStmt, &numCols);
      Assert::AreEqual (10, (int)numCols, L"column count");

      v = 0;
      SQLColAttribute (hStmt, 1, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARCHAR, (long)v, L"c_string CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARCHAR, (long)v, L"c_string DESC_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 2, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARCHAR, (long)v, L"c_vc20000 CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 3, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_VARCHAR, (long)v, L"c_vc100 CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 4, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_CHAR, (long)v, L"c_char CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 5, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_VARBINARY, (long)v, L"c_bitvar CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 6, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_INTEGER, (long)v, L"c_int CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 7, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_BINARY, (long)v, L"c_bit CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 8, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARCHAR, (long)v, L"c_set CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 9, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARCHAR, (long)v, L"c_clob CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 10, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARBINARY, (long)v, L"c_blob CONCISE_TYPE");

      /* The combination that actually exhausts PHP's memory limit: a column
	 ext/odbc will bind, carrying a display size it has to allocate up front. */
      for (SQLUSMALLINT i = 1; i <= numCols; i++)
	{
	  SQLLEN type = 0, displaySize = 0;

	  SQLColAttribute (hStmt, i, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &type);
	  SQLColAttribute (hStmt, i, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &displaySize);

	  bool isLong = (type == SQL_LONGVARCHAR || type == SQL_WLONGVARCHAR
			 || type == SQL_BINARY || type == SQL_VARBINARY || type == SQL_LONGVARBINARY);

	  if (!isLong && displaySize > 8000)
	    {
	      WCHAR wmsg[128];
	      wsprintf (wmsg, L"column %d: bound type %ld with display size %ld",
			(int)i, (long)type, (long)displaySize);
	      Assert::Fail (wmsg);
	    }
	}

      SQLFreeStmt (hStmt, SQL_CLOSE);
      SQLExecDirect (hStmt, L"DROP TABLE t_php_coltype", SQL_NTS);
      SQLFreeHandle (SQL_HANDLE_STMT, hStmt);
      SQLDisconnect (hDbc);
      SQLFreeHandle (SQL_HANDLE_DBC, hDbc);
      SQLFreeHandle (SQL_HANDLE_ENV, hEnv);
    }

    
  };
}
