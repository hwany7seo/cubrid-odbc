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
    TEST_METHOD (PHP_ODBC_001_ColAttribute_TypeCode)
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

    /*
     * PHP-ODBC-002 - SQL_COLUMN_PRECISION must report the column size.
     *
     * odbc_field_len() maps onto the ODBC 2.x identifier SQL_COLUMN_PRECISION,
     * which for character, binary and datetime types has to come from
     * SQL_DESC_LENGTH.  The driver forwards it to SQL_DESC_PRECISION, which
     * odbc_set_ird() deliberately sets to 0 for those types, so every string
     * column reports length 0.
     */
    TEST_METHOD (PHP_ODBC_002_ColumnPrecision)
    {
      RETCODE retcode;
      SQLHENV hEnv;
      SQLHDBC hDbc;
      SQLHSTMT hStmt;
      SQLLEN v;

      SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
      SQLSetEnvAttr (hEnv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
      SQLAllocHandle (SQL_HANDLE_DBC, hEnv, &hDbc);

      retcode = SQLDriverConnect (hDbc, NULL,
				  L"DRIVER=CUBRID Driver Unicode;DB_NAME=demodb;SERVER=test-db-server;PORT=33000;UID=dba;PWD=;CHARSET=utf-8;AUTOCOMMIT=ON",
				  SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);
      SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hStmt);

      SQLExecDirect (hStmt, L"DROP TABLE IF EXISTS t_php_collen", SQL_NTS);
      retcode = SQLExecDirect (hStmt,
			       L"CREATE TABLE t_php_collen (c_string STRING, c_vc100 VARCHAR(100),"
			       L" c_char CHAR(10), c_date DATE, c_int INTEGER, c_num NUMERIC(10,2))",
			       SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      retcode = SQLExecDirect (hStmt, L"SELECT * FROM t_php_collen", SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      v = 0;
      SQLColAttribute (hStmt, 1, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)MAX_CUBRID_CHAR_LEN, (long)v, L"c_string COLUMN_PRECISION");

      v = 0;
      SQLColAttribute (hStmt, 2, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
      Assert::AreEqual (100L, (long)v, L"c_vc100 COLUMN_PRECISION");

      v = 0;
      SQLColAttribute (hStmt, 3, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
      Assert::AreEqual (10L, (long)v, L"c_char COLUMN_PRECISION");

      v = 0;
      SQLColAttribute (hStmt, 4, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
      Assert::AreEqual (10L, (long)v, L"c_date COLUMN_PRECISION");

      v = 0;
      SQLColAttribute (hStmt, 5, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
      Assert::AreEqual (10L, (long)v, L"c_int COLUMN_PRECISION");

      v = 0;
      SQLColAttribute (hStmt, 6, SQL_COLUMN_PRECISION, NULL, 0, NULL, &v);
      Assert::AreEqual (10L, (long)v, L"c_num COLUMN_PRECISION");

      /* SQL_DESC_LENGTH already carries the right value; only the 2.x alias is wrong. */
      v = 0;
      SQLColAttribute (hStmt, 2, SQL_DESC_LENGTH, NULL, 0, NULL, &v);
      Assert::AreEqual (100L, (long)v, L"c_vc100 DESC_LENGTH");

      v = 0;
      SQLColAttribute (hStmt, 3, SQL_DESC_LENGTH, NULL, 0, NULL, &v);
      Assert::AreEqual (10L, (long)v, L"c_char DESC_LENGTH");

      SQLFreeStmt (hStmt, SQL_CLOSE);
      SQLExecDirect (hStmt, L"DROP TABLE t_php_collen", SQL_NTS);
      SQLFreeHandle (SQL_HANDLE_STMT, hStmt);
      SQLDisconnect (hDbc);
      SQLFreeHandle (SQL_HANDLE_DBC, hDbc);
      SQLFreeHandle (SQL_HANDLE_ENV, hEnv);
    }

    /*
     * PHP-ODBC-003 - CLOB/BLOB/ENUM/JSON must describe themselves.
     *
     * odbc_data_type_info_set[] has no BLOB/CLOB row, so odbc_type_name() and
     * odbc_display_size() both miss and the columns report an empty type name
     * with size 0.  odbc_set_desc_field() also rejects SQL_BLOB/SQL_CLOB as a
     * verbose type, leaving SQL_DESC_TYPE at its SQL_C_DEFAULT (99) initial
     * value.  ENUM and JSON are not in odbc_type_by_cci() at all; its
     * unsupported-type sentinel is -1, which collides with SQL_LONGVARCHAR.
     */
    TEST_METHOD (PHP_ODBC_003_Lob_Enum_Json_Metadata)
    {
      RETCODE retcode;
      SQLHENV hEnv;
      SQLHDBC hDbc;
      SQLHSTMT hStmt;
      SQLWCHAR name[128];
      SQLSMALLINT namelen;
      SQLLEN v;

      SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
      SQLSetEnvAttr (hEnv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
      SQLAllocHandle (SQL_HANDLE_DBC, hEnv, &hDbc);

      retcode = SQLDriverConnect (hDbc, NULL,
				  L"DRIVER=CUBRID Driver Unicode;DB_NAME=demodb;SERVER=test-db-server;PORT=33000;UID=dba;PWD=;CHARSET=utf-8;AUTOCOMMIT=ON",
				  SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);
      SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hStmt);

      SQLExecDirect (hStmt, L"DROP TABLE IF EXISTS t_php_lobtype", SQL_NTS);
      retcode = SQLExecDirect (hStmt,
			       L"CREATE TABLE t_php_lobtype (c_clob CLOB, c_blob BLOB,"
			       L" c_enum ENUM('a','b'), c_json JSON)", SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      retcode = SQLExecDirect (hStmt, L"SELECT * FROM t_php_lobtype", SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      name[0] = 0;
      SQLColAttribute (hStmt, 1, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
      Assert::AreEqual (L"CLOB", (const wchar_t *)name, false, L"c_clob TYPE_NAME");

      name[0] = 0;
      SQLColAttribute (hStmt, 2, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
      Assert::AreEqual (L"BLOB", (const wchar_t *)name, false, L"c_blob TYPE_NAME");

      name[0] = 0;
      SQLColAttribute (hStmt, 3, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
      Assert::AreEqual (L"ENUM", (const wchar_t *)name, false, L"c_enum TYPE_NAME");

      name[0] = 0;
      SQLColAttribute (hStmt, 4, SQL_DESC_TYPE_NAME, name, sizeof (name), &namelen, NULL);
      Assert::AreEqual (L"JSON", (const wchar_t *)name, false, L"c_json TYPE_NAME");

      v = 0;
      SQLColAttribute (hStmt, 1, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARCHAR, (long)v, L"c_clob CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 2, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARBINARY, (long)v, L"c_blob CONCISE_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARCHAR, (long)v, L"c_clob DESC_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 2, SQL_DESC_TYPE, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)SQL_LONGVARBINARY, (long)v, L"c_blob DESC_TYPE");

      v = 0;
      SQLColAttribute (hStmt, 1, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)MAX_CUBRID_CHAR_LEN, (long)v, L"c_clob OCTET_LENGTH");

      v = 0;
      SQLColAttribute (hStmt, 2, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL, &v);
      Assert::AreEqual ((long)MAX_CUBRID_CHAR_LEN, (long)v, L"c_blob OCTET_LENGTH");

      SQLFreeStmt (hStmt, SQL_CLOSE);
      SQLExecDirect (hStmt, L"DROP TABLE t_php_lobtype", SQL_NTS);
      SQLFreeHandle (SQL_HANDLE_STMT, hStmt);
      SQLDisconnect (hDbc);
      SQLFreeHandle (SQL_HANDLE_DBC, hDbc);
      SQLFreeHandle (SQL_HANDLE_ENV, hEnv);
    }

    /*
     * PHP-ODBC-004 - NUMERIC/DECIMAL must report a usable display size.
     *
     * odbc_display_size() branches on octet_length == -1 instead of
     * display_size == -1.  NUMERIC/DECIMAL are the only rows with a constant
     * octet_length and a delegated display size, so they fall through to
     * "return display_size" and hand out -1.  ext/odbc sizes its bound
     * buffer as display_size + 1, binding a 0-byte buffer and later reading
     * back uninitialised heap - in practice a leaked pointer value.
     */
    TEST_METHOD (PHP_ODBC_004_Numeric_DisplaySize)
    {
      RETCODE retcode;
      SQLHENV hEnv;
      SQLHDBC hDbc;
      SQLHSTMT hStmt;
      SQLCHAR value[64];
      SQLLEN ind;

      SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
      SQLSetEnvAttr (hEnv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
      SQLAllocHandle (SQL_HANDLE_DBC, hEnv, &hDbc);

      retcode = SQLDriverConnect (hDbc, NULL,
				  L"DRIVER=CUBRID Driver Unicode;DB_NAME=demodb;SERVER=test-db-server;PORT=33000;UID=dba;PWD=;CHARSET=utf-8;AUTOCOMMIT=ON",
				  SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);
      SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hStmt);

      SQLExecDirect (hStmt, L"DROP TABLE IF EXISTS t_php_numeric", SQL_NTS);
      retcode = SQLExecDirect (hStmt,
			       L"CREATE TABLE t_php_numeric (n NUMERIC(10,2), d DECIMAL(8,3), i INTEGER)",
			       SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      retcode = SQLExecDirect (hStmt, L"INSERT INTO t_php_numeric VALUES (1234.56, 42.125, 7)", SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);
      SQLFreeStmt (hStmt, SQL_CLOSE);

      retcode = SQLExecDirect (hStmt, L"SELECT * FROM t_php_numeric", SQL_NTS);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      /* display_size_decimal() is precision + 2, for the sign and the point. */
      SQLLEN v = 0;

      SQLColAttribute (hStmt, 1, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &v);
      Assert::AreEqual (12L, (long)v, L"n DISPLAY_SIZE");

      v = 0;
      SQLColAttribute (hStmt, 2, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &v);
      Assert::AreEqual (10L, (long)v, L"d DISPLAY_SIZE");

      v = 0;
      SQLColAttribute (hStmt, 3, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &v);
      Assert::AreEqual (11L, (long)v, L"i DISPLAY_SIZE");

      retcode = SQLFetch (hStmt);
      Assert::AreNotEqual ((int)retcode, SQL_ERROR);

      /* ext/odbc sizes its bound buffer as display size + 1.  A 0-length
	 SQLGetData call is spec-legal and safe: the driver reports the true
	 length through ind but writes nothing, leaving value as the empty
	 string pre-set below - exactly the truncation PHP hits. */
      static const char *const expected[3] = { "1234.56", "42.125", "7" };

      for (SQLUSMALLINT i = 1; i <= 3; i++)
	{
	  SQLLEN displaySize = 0, bindLen;

	  SQLColAttribute (hStmt, i, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL, &displaySize);

	  bindLen = displaySize + 1;
	  if (bindLen < 0)
	    {
	      bindLen = 0;
	    }
	  else if (bindLen > (SQLLEN)sizeof (value))
	    {
	      bindLen = sizeof (value);
	    }

	  value[0] = '\0';
	  SQLGetData (hStmt, i, SQL_C_CHAR, value, bindLen, &ind);
	  value[sizeof (value) - 1] = '\0';

	  WCHAR wmsg[64];
	  wsprintf (wmsg, L"value[%d] through a display-size buffer", (int)i);
	  Assert::AreEqual (expected[i - 1], (const char *)value, false, wmsg);
	}

      SQLFreeStmt (hStmt, SQL_CLOSE);
      SQLExecDirect (hStmt, L"DROP TABLE t_php_numeric", SQL_NTS);
      SQLFreeHandle (SQL_HANDLE_STMT, hStmt);
      SQLDisconnect (hDbc);
      SQLFreeHandle (SQL_HANDLE_DBC, hDbc);
      SQLFreeHandle (SQL_HANDLE_ENV, hEnv);
    }
  };
}
