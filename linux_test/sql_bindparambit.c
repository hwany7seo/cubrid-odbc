#include <stdio.h>
#include <wchar.h>
#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include "test_util.h"

/*
 * usage:
 *       sql_bindparambit dsn
 *
 * Reproduces (with raw ODBC calls, no Go/database-sql layer in the way) the
 * failure seen in cubrid-driverlink-test/go-odbc/to_odbc/bind.go's Test 2:
 * binding a BIT column parameter as []byte{0x80} (ODBC SQL_C_BINARY) fails
 * on SQLExecute against CUBRID_Unicode:
 *
 *   note: Test 2 (BIT []byte B'1', B'1') failed: SQLExecute: ...
 *
 * This testcase drives SQLPrepare/SQLBindParameter/SQLExecute directly
 * against a BIT column using a few different C-type/SQL-type pairings, to
 * isolate whether the failure is in cubrid-odbc's parameter binding for
 * SQL_BIT specifically, or in how the caller encodes the value. Each
 * variant is tried independently (a failure in one does not stop the
 * others) and prints SQLGetDiagRec output on failure so the exact
 * SQLSTATE/native error/message from the driver is visible.
 */

static void
print_diag (SQLSMALLINT handle_type, SQLHANDLE handle, const char *label)
{
  SQLCHAR sqlstate[6];
  SQLINTEGER native_error;
  SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
  SQLSMALLINT msg_len;
  SQLSMALLINT rec = 1;

  while (SQLGetDiagRec (handle_type, handle, rec, sqlstate, &native_error,
			 message, sizeof (message), &msg_len) == SQL_SUCCESS)
    {
      printf ("      [%s] SQLSTATE=%s native=%d msg=%s\n",
	      label, sqlstate, (int) native_error, message);
      rec++;
    }
}

/*
 * Inserts one row (id, a_bit) into tbl_bindparambit_test using the given
 * C-type/SQL-type pairing for a_bit. Returns 1 if SQLExecute succeeded
 * (SQL_SUCCESS or SQL_SUCCESS_WITH_INFO), 0 otherwise. Never returns early
 * on failure -- prints diagnostics and moves on so all variants get tried.
 */
static int
try_bind_variant (SQLHDBC hDbc, int id, const char *variant_name,
		   SQLSMALLINT c_type, SQLSMALLINT sql_type,
		   SQLPOINTER value, SQLLEN value_len)
{
  RETCODE retcode;
  SQLHSTMT hstmt = SQL_NULL_HSTMT;
  SQLCHAR *insert_sql = (SQLCHAR *) "INSERT INTO tbl_bindparambit_test (id, a_bit) VALUES (?, ?)";
  SQLLEN id_ind = 0;
  SQLLEN val_ind = value_len;
  int ok = 0;

  printf ("  variant [%s]:\n", variant_name);

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLAllocHandle(STMT) failed\n");
      return 0;
    }

  retcode = SQLPrepare (hstmt, insert_sql, SQL_NTS);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLPrepare failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, variant_name);
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLBindParameter (hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
			      0, 0, &id, 0, &id_ind);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(id) failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, variant_name);
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  /* ColumnSize=1: per the ODBC spec, SQL_BIT columns have a column size of 1. */
  retcode = SQLBindParameter (hstmt, 2, SQL_PARAM_INPUT, c_type, sql_type,
			      1, 0, value, value_len, &val_ind);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(a_bit) failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, variant_name);
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLExecute (hstmt);
  ok = (retcode != SQL_ERROR);
  printf ("      SQLExecute -> %s (retcode=%d)\n", ok ? "OK" : "FAILED", retcode);
  if (!ok)
    {
      print_diag (SQL_HANDLE_STMT, hstmt, variant_name);
    }

  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
  return ok;
}

/*
 * Reproduces the EXACT shape of go-odbc bind.go's Test 2 statement:
 *   INSERT INTO tbl_bind_test (id, a_bit, b_vbit, h_set, k_blob, l_clob)
 *     VALUES (2, ?, ?, NULL, NULL, NULL)
 * i.e. two BIT columns bound as parameters #1 and #2 in the SAME statement
 * (id is a literal, not a parameter, exactly like bind.go), both using
 * SQL_C_BINARY->SQL_BIT with the same 0x80 byte alexbrainman/odbc sends.
 * try_bind_variant() above only ever binds ONE BIT parameter per statement,
 * so if this fails where the single-param variant passed, the bug is
 * specific to binding more than one SQL_BIT parameter in one statement.
 */
static int
try_two_bit_params (SQLHDBC hDbc)
{
  RETCODE retcode;
  SQLHSTMT hstmt = SQL_NULL_HSTMT;
  SQLCHAR *insert_sql = (SQLCHAR *) "INSERT INTO tbl_bindparambit_test2 (id, a_bit, b_vbit) VALUES (5, ?, ?)";
  unsigned char byte_0x80_a = 0x80;
  unsigned char byte_0x80_b = 0x80;
  SQLLEN ind_a = 1;
  SQLLEN ind_b = 1;
  int ok = 0;

  printf ("  variant [two SQL_BIT params in one statement, both SQL_C_BINARY 0x80]:\n");

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLAllocHandle(STMT) failed\n");
      return 0;
    }

  retcode = SQLPrepare (hstmt, insert_sql, SQL_NTS);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLPrepare failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "two-bit-params");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLBindParameter (hstmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_BIT,
			      1, 0, &byte_0x80_a, 1, &ind_a);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(a_bit, param#1) failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "two-bit-params");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLBindParameter (hstmt, 2, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_BIT,
			      1, 0, &byte_0x80_b, 1, &ind_b);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(b_vbit, param#2) failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "two-bit-params");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLExecute (hstmt);
  ok = (retcode != SQL_ERROR);
  printf ("      SQLExecute -> %s (retcode=%d)\n", ok ? "OK" : "FAILED", retcode);
  if (!ok)
    {
      print_diag (SQL_HANDLE_STMT, hstmt, "two-bit-params");
    }

  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
  return ok;
}

/*
 * This is the real root-cause probe. go-odbc's param.go (ExtractParameters +
 * bindParam, see cubrid-driverlink-test/go-odbc/odbc/param.go:154 and 190)
 * calls SQLDescribeParam on every "?" placeholder BEFORE binding. If that
 * call succeeds (p.isDescribed = true), it uses *whatever SQL type
 * SQLDescribeParam reported* for the parameter -- NOT a hardcoded SQL_BIT --
 * when it then calls SQLBindParameter with ValueType=SQL_C_BINARY. So the
 * real question is not "does cubrid-odbc accept SQL_C_BINARY->SQL_BIT" (it
 * does, per the variants above) but "what does cubrid-odbc's
 * SQLDescribeParam report for a '?' bound to a BIT column, and does binding
 * SQL_C_BINARY against THAT reported type/size/decimal succeed?"
 */
static int
try_describe_then_bind (SQLHDBC hDbc)
{
  RETCODE retcode;
  SQLHSTMT hstmt = SQL_NULL_HSTMT;
  SQLCHAR *insert_sql = (SQLCHAR *) "INSERT INTO tbl_bindparambit_test2 (id, a_bit, b_vbit) VALUES (6, ?, ?)";
  SQLSMALLINT num_params = 0;
  SQLSMALLINT p1_sqltype = 0, p2_sqltype = 0;
  SQLULEN p1_size = 0, p2_size = 0;
  SQLSMALLINT p1_decimal = 0, p2_decimal = 0;
  SQLSMALLINT p1_nullable = 0, p2_nullable = 0;
  unsigned char byte_0x80_a = 0x80;
  unsigned char byte_0x80_b = 0x80;
  SQLLEN ind_a = 1;
  SQLLEN ind_b = 1;
  int ok = 0;

  printf ("  variant [SQLDescribeParam-driven bind, mirroring go-odbc's actual code path]:\n");

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLAllocHandle(STMT) failed\n");
      return 0;
    }

  retcode = SQLPrepare (hstmt, insert_sql, SQL_NTS);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLPrepare failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "describe-then-bind");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLNumParams (hstmt, &num_params);
  printf ("      SQLNumParams -> retcode=%d num_params=%d\n", retcode, num_params);

  retcode = SQLDescribeParam (hstmt, 1, &p1_sqltype, &p1_size, &p1_decimal, &p1_nullable);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLDescribeParam(param#1) FAILED -- go-odbc would fall back to its\n");
      printf ("      size-based guess (SQL_BINARY/SQL_LONGVARBINARY) in this case, not this bug.\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "describe-then-bind");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }
  printf ("      SQLDescribeParam(param#1) -> sqltype=%d size=%lu decimal=%d nullable=%d\n",
	  (int) p1_sqltype, (unsigned long) p1_size, (int) p1_decimal, (int) p1_nullable);

  retcode = SQLDescribeParam (hstmt, 2, &p2_sqltype, &p2_size, &p2_decimal, &p2_nullable);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLDescribeParam(param#2) FAILED\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "describe-then-bind");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }
  printf ("      SQLDescribeParam(param#2) -> sqltype=%d size=%lu decimal=%d nullable=%d\n",
	  (int) p2_sqltype, (unsigned long) p2_size, (int) p2_decimal, (int) p2_nullable);

  /* Mirror go-odbc exactly: ValueType=SQL_C_BINARY always for []byte,
   * ParameterType=whatever SQLDescribeParam reported, ColumnSize=len(data)=1. */
  retcode = SQLBindParameter (hstmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY, p1_sqltype,
			      1, p1_decimal, &byte_0x80_a, 1, &ind_a);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(param#1, using described sqltype=%d) FAILED\n", (int) p1_sqltype);
      print_diag (SQL_HANDLE_STMT, hstmt, "describe-then-bind");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLBindParameter (hstmt, 2, SQL_PARAM_INPUT, SQL_C_BINARY, p2_sqltype,
			      1, p2_decimal, &byte_0x80_b, 1, &ind_b);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(param#2, using described sqltype=%d) FAILED\n", (int) p2_sqltype);
      print_diag (SQL_HANDLE_STMT, hstmt, "describe-then-bind");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLExecute (hstmt);
  ok = (retcode != SQL_ERROR);
  printf ("      SQLExecute -> %s (retcode=%d)\n", ok ? "OK" : "FAILED", retcode);
  if (!ok)
    {
      print_diag (SQL_HANDLE_STMT, hstmt, "describe-then-bind");
    }

  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
  return ok;
}

int
sql_bindparambit (int case_num, char *dsn)
{
  RETCODE retcode;
  SQLHENV hEnv;
  SQLHDBC hDbc;
  SQLHSTMT hstmt;
  wchar_t *dsn_buf;
  unsigned char byte_0x80 = 0x11;	/* what go-odbc's bind.go sends today: []byte{0x80} == B'1' */
  unsigned char sql_c_bit_one = 1;	/* SQL_C_BIT expects a plain 0/1 byte, not ASCII */
  char ascii_digit_1 = '1';
  int pass_count = 0;
  int variant_count = 4;
  SQLCHAR bit_val[3] = "0xF";  // 8비트 (1111)
  SQLCHAR var_val[5] = "0xAF"; // 가변 비트

  retcode = SQLAllocEnv (&hEnv);
  retcode = SQLSetEnvAttr (hEnv, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
  retcode = SQLAllocConnect (hEnv, &hDbc);
  AreNotEqual (retcode, SQL_ERROR);

  bytes_to_wide_char (dsn, strlen (dsn), &dsn_buf, 0, NULL, "UCS2");
  retcode = SQLConnectW (hDbc, (SQLWCHAR *) dsn_buf, SQL_NTS, NULL, SQL_NTS, NULL, SQL_NTS);
  AreNotEqual (retcode, SQL_ERROR);

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS tbl_bindparambit_test", SQL_NTS);
  retcode = SQLExecDirect (hstmt, (SQLCHAR *) "CREATE TABLE tbl_bindparambit_test (id INT, a_bit BIT(16))", SQL_NTS);
  if (retcode == SQL_ERROR)
    {
      printf ("  setup: CREATE TABLE tbl_bindparambit_test failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "create");
    }
  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS tbl_bindparambit_test2", SQL_NTS);
  retcode = SQLExecDirect (hstmt, (SQLCHAR *) "CREATE TABLE tbl_bindparambit_test2 (id INT, a_bit BIT(8), b_vbit BIT(8))", SQL_NTS);
  if (retcode == SQL_ERROR)
    {
      printf ("  setup: CREATE TABLE tbl_bindparambit_test2 failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "create2");
    }
  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);

  printf ("  -- reproducing go-odbc bind.go Test 2 (BIT column parameter binding) --\n");

  /* Variant 1: exactly what alexbrainman/odbc sends today for a Go []byte
   * parameter bound to a BIT column -- this is the one known to fail. */
  pass_count += try_bind_variant (hDbc, 1, "SQL_C_BINARY value=0x80 -> SQL_BIT",
				  SQL_C_BINARY, SQL_BIT, &byte_0x80, 1);

  /* Variant 2: same C-side bytes, but declare the target SQL type as
   * SQL_BINARY instead of SQL_BIT (in case cubrid-odbc's SQLBindParameter
   * path only accepts SQL_BIT params through the SQL_C_BIT/SQL_C_CHAR
   * branches and mishandles SQL_C_BINARY). */
  pass_count += try_bind_variant (hDbc, 2, "SQL_C_BINARY value=0x80 -> SQL_BINARY",
				  SQL_C_BINARY, SQL_BINARY, &bit_val, 1);

  pass_count += try_bind_variant (hDbc, 3, "SQL_C_BINARY value=0x80 -> SQL_BINARY",
				  SQL_C_BINARY, SQL_BINARY, &var_val, 1);


  /* Variant 3: SQL_C_BIT is the ODBC-spec C type for bit values (a single
   * byte containing numeric 0 or 1, not a packed bitmask). */
  pass_count += try_bind_variant (hDbc, 4, "SQL_C_BIT value=1 -> SQL_BIT",
				  SQL_C_BIT, SQL_BIT, &sql_c_bit_one, 1);

  /* Variant 4: some drivers accept bit values as an ASCII '0'/'1' digit via
   * SQL_C_CHAR instead of a binary/bit C type. */
  pass_count += try_bind_variant (hDbc, 5, "SQL_C_CHAR value='1' -> SQL_BIT",
				  SQL_C_CHAR, SQL_BIT, &ascii_digit_1, 1);

  printf ("  bind-variant summary (single BIT param per statement): %d/%d succeeded\n",
	  pass_count, variant_count);

  /* Now the exact multi-param shape bind.go's Test 2 actually uses. */
  pass_count += try_two_bit_params (hDbc);
  variant_count++;

  /* And finally, the actual go-odbc code path: describe params first, then
   * bind SQL_C_BINARY against whatever SQLDescribeParam reported. */
  pass_count += try_describe_then_bind (hDbc);
  variant_count++;

  printf ("  bind-variant summary (all variants): %d/%d succeeded\n",
	  pass_count, variant_count);

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
//   SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE tbl_bindparambit_test", SQL_NTS);
//   SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE tbl_bindparambit_test2", SQL_NTS);
  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);

  retcode = SQLDisconnect (hDbc);
  AreNotEqual (retcode, SQL_ERROR);
  retcode = SQLFreeHandle (SQL_HANDLE_DBC, hDbc);
  retcode = SQLFreeHandle (SQL_HANDLE_ENV, hEnv);
  AreNotEqual (retcode, SQL_ERROR);

  /* This testcase's job is to surface driver behavior for each variant
   * above (see printed diagnostics), not to assert a single expected
   * outcome -- so it reports overall SQL_SUCCESS as long as connect/
   * disconnect worked, independent of how many bind variants succeeded. */
  return SQL_SUCCESS;
}
