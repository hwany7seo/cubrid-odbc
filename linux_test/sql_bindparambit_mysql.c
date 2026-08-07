#include <stdio.h>
#include <wchar.h>
#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include "test_util.h"

/*
 * usage:
 *       sql_bindparambit_mysql            (ignores dsn.txt; connects to MySQL)
 *
 * MySQL variant of sql_bindparambit.c. Drives the SAME BIT-column parameter
 * binding variants (SQL_C_BINARY / SQL_C_BIT / SQL_C_CHAR against SQL_BIT /
 * SQL_BINARY) against a MySQL 8.0 server via the MySQL Connector/ODBC driver,
 * so its behavior can be compared with CUBRID's.
 *
 * Connection: uses the MYSQL_TESTDB DSN from ~/.odbc.ini for driver/server/db,
 * plus UID/PWD and NO_SSPS=1 supplied here. NO_SSPS=1 disables server-side
 * prepared statements; without it MySQL Connector/ODBC 9.6 rejects every
 * parameter-bound execute with "Incorrect arguments to COM_STMT_EXECUTE".
 *
 * MySQL has a native BIT(M) type, so the CUBRID BIT(8) DDL is kept as-is.
 */

#define MYSQL_CONN_STR "DSN=MYSQL_TESTDB;UID=hwanyseo;PWD=1111;NO_SSPS=1;"

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

static int
try_two_bit_params (SQLHDBC hDbc)
{
  RETCODE retcode;
  SQLHSTMT hstmt = SQL_NULL_HSTMT;
  SQLCHAR *insert_sql = (SQLCHAR *) "INSERT INTO tbl_bindparambit_test2 (id, a_bit, b_vbit) VALUES (5, ?, ?)";
  unsigned char byte_a = 0x80;
  unsigned char byte_b = 0x80;
  SQLLEN ind_a = 1;
  SQLLEN ind_b = 1;
  int ok = 0;

  printf ("  variant [two BIT params in one statement, both SQL_C_BINARY]:\n");

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
			      1, 0, &byte_a, 1, &ind_a);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(a_bit, param#1) failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "two-bit-params");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLBindParameter (hstmt, 2, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_BIT,
			      1, 0, &byte_b, 1, &ind_b);
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
  unsigned char byte_a = 0x80;
  unsigned char byte_b = 0x80;
  SQLLEN ind_a = 1;
  SQLLEN ind_b = 1;
  int ok = 0;

  printf ("  variant [SQLDescribeParam-driven bind, mirroring go-odbc's code path]:\n");

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
      printf ("      SQLDescribeParam(param#1) FAILED / not supported\n");
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

  retcode = SQLBindParameter (hstmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY, p1_sqltype,
			      1, p1_decimal, &byte_a, 1, &ind_a);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(param#1, described sqltype=%d) FAILED\n", (int) p1_sqltype);
      print_diag (SQL_HANDLE_STMT, hstmt, "describe-then-bind");
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  retcode = SQLBindParameter (hstmt, 2, SQL_PARAM_INPUT, SQL_C_BINARY, p2_sqltype,
			      1, p2_decimal, &byte_b, 1, &ind_b);
  if (retcode == SQL_ERROR)
    {
      printf ("      SQLBindParameter(param#2, described sqltype=%d) FAILED\n", (int) p2_sqltype);
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

static void
print_hex (const unsigned char *p, SQLLEN n)
{
  SQLLEN i;
  if (n < 0)
    {
      printf ("(null)");
      return;
    }
  for (i = 0; i < n; i++)
    {
      printf ("%02x", p[i]);
    }
}

/*
 * Round-trip verification: INSERT `in` (in_len bytes, SQL_C_BINARY -> sql_type)
 * into <table>.<col> at row `id`, SELECT it back and compare against `expect`.
 * For MySQL, BIT(8) holds exactly one byte (round-trips verbatim) and
 * VARBINARY(n) stores bytes verbatim. Returns 1 on match, 0 otherwise.
 */
static int
verify_bit_value (SQLHDBC hDbc, const char *table, const char *col, int id,
		  const char *label, SQLSMALLINT sql_type,
		  const unsigned char *in, SQLLEN in_len,
		  const unsigned char *expect, SQLLEN expect_len)
{
  RETCODE rc;
  SQLHSTMT hstmt = SQL_NULL_HSTMT;
  char sql[256];
  SQLLEN id_ind = 0;
  SQLLEN val_ind = in_len;
  SQLLEN got_ind = 0;
  unsigned char got[256];
  int ok = 0;

  rc = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  if (rc == SQL_ERROR)
    {
      printf ("  [%s] SQLAllocHandle(insert) failed\n", label);
      return 0;
    }
  snprintf (sql, sizeof (sql), "INSERT INTO %s (id, %s) VALUES (?, ?)", table, col);
  rc = SQLPrepare (hstmt, (SQLCHAR *) sql, SQL_NTS);
  if (SQL_SUCCEEDED (rc))
    rc = SQLBindParameter (hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, &id_ind);
  if (SQL_SUCCEEDED (rc))
    rc = SQLBindParameter (hstmt, 2, SQL_PARAM_INPUT, SQL_C_BINARY, sql_type, in_len, 0, (SQLPOINTER) in, in_len, &val_ind);
  if (SQL_SUCCEEDED (rc))
    rc = SQLExecute (hstmt);
  if (!SQL_SUCCEEDED (rc))
    {
      printf ("  [%s] in=", label);
      print_hex (in, in_len);
      printf (" -> INSERT FAILED\n");
      print_diag (SQL_HANDLE_STMT, hstmt, label);
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }
  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);

  rc = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  if (rc == SQL_ERROR)
    {
      printf ("  [%s] SQLAllocHandle(select) failed\n", label);
      return 0;
    }
  snprintf (sql, sizeof (sql), "SELECT %s FROM %s WHERE id = ?", col, table);
  rc = SQLPrepare (hstmt, (SQLCHAR *) sql, SQL_NTS);
  if (SQL_SUCCEEDED (rc))
    rc = SQLBindParameter (hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, &id_ind);
  if (SQL_SUCCEEDED (rc))
    rc = SQLExecute (hstmt);
  if (SQL_SUCCEEDED (rc))
    rc = SQLFetch (hstmt);
  if (!SQL_SUCCEEDED (rc))
    {
      printf ("  [%s] SELECT/FETCH FAILED\n", label);
      print_diag (SQL_HANDLE_STMT, hstmt, label);
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }
  rc = SQLGetData (hstmt, 1, SQL_C_BINARY, got, sizeof (got), &got_ind);
  if (!SQL_SUCCEEDED (rc))
    {
      printf ("  [%s] SQLGetData FAILED\n", label);
      print_diag (SQL_HANDLE_STMT, hstmt, label);
      SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
      return 0;
    }

  ok = (got_ind == expect_len && memcmp (got, expect, (size_t) expect_len) == 0);
  printf ("  [%s] in=", label);
  print_hex (in, in_len);
  printf (" got=");
  print_hex (got, got_ind);
  printf (" expect=");
  print_hex (expect, expect_len);
  printf (" -> %s\n", ok ? "VERIFIED" : "MISMATCH");

  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);
  return ok;
}

int
sql_bindparambit_mysql (int case_num, char *dsn)
{
  RETCODE retcode;
  SQLHENV hEnv;
  SQLHDBC hDbc;
  SQLHSTMT hstmt;
  SQLCHAR conn_out[1024];
  SQLSMALLINT conn_out_len;
  unsigned char byte_val = 0xaa;
  unsigned char sql_c_bit_one = 1;
  char ascii_digit_1 = '1';
  int pass_count = 0;
  int variant_count = 7;
  int verify_fail = 0;

  (void) dsn;			/* ignore dsn.txt; this test targets MySQL explicitly */

  retcode = SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
  retcode = SQLSetEnvAttr (hEnv, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
  retcode = SQLAllocHandle (SQL_HANDLE_DBC, hEnv, &hDbc);
  AreNotEqual (retcode, SQL_ERROR);

  retcode = SQLDriverConnect (hDbc, NULL, (SQLCHAR *) MYSQL_CONN_STR, SQL_NTS,
			      conn_out, sizeof (conn_out), &conn_out_len, SQL_DRIVER_NOPROMPT);
  if (retcode == SQL_ERROR)
    {
      printf ("  connect to MySQL (%s) FAILED\n", MYSQL_CONN_STR);
      print_diag (SQL_HANDLE_DBC, hDbc, "connect");
    }
  AreNotEqual (retcode, SQL_ERROR);

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS tbl_bindparambit_test", SQL_NTS);
  retcode = SQLExecDirect (hstmt, (SQLCHAR *) "CREATE TABLE tbl_bindparambit_test (id INT, a_bit BIT(8))", SQL_NTS);
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

  printf ("  -- MySQL BIT(8) column parameter binding --\n");

  pass_count += try_bind_variant (hDbc, 1, "SQL_C_BINARY value=0xaa -> SQL_BIT",
				  SQL_C_BINARY, SQL_BIT, &byte_val, 1);
  pass_count += try_bind_variant (hDbc, 2, "SQL_C_BINARY value=0xaa -> SQL_BINARY",
				  SQL_C_BINARY, SQL_BINARY, &byte_val, 1);
  pass_count += try_bind_variant (hDbc, 3, "SQL_C_BINARY value=0xaa -> SQL_VARBINARY",
				  SQL_C_BINARY, SQL_VARBINARY, &byte_val, 1);
  pass_count += try_bind_variant (hDbc, 4, "SQL_C_BIT value=1 -> SQL_BIT",
				  SQL_C_BIT, SQL_BIT, &sql_c_bit_one, 1);
  pass_count += try_bind_variant (hDbc, 5, "SQL_C_CHAR value='1' -> SQL_BIT",
				  SQL_C_CHAR, SQL_BIT, &ascii_digit_1, 1);
  pass_count += try_bind_variant (hDbc, 6, "SQL_C_CHAR value='1' -> SQL_CHAR",
				  SQL_C_CHAR, SQL_CHAR, &ascii_digit_1, 1);
  pass_count += try_bind_variant (hDbc, 7, "SQL_C_BIT value=1 -> SQL_BINARY",
				  SQL_C_BIT, SQL_BINARY, &sql_c_bit_one, 1);

  printf ("  bind-variant summary (single BIT param per statement): %d/%d succeeded\n",
	  pass_count, variant_count);

  pass_count += try_two_bit_params (hDbc);
  variant_count++;

  pass_count += try_describe_then_bind (hDbc);
  variant_count++;

  printf ("  bind-variant summary (all variants): %d/%d succeeded\n",
	  pass_count, variant_count);

  /* ------------------------------------------------------------------
   * Round-trip VALUE verification. BIT(8) holds exactly one byte, so 1-byte
   * values round-trip verbatim regardless of MySQL's (right-aligned) BIT
   * semantics; VARBINARY(n) stores bytes verbatim.
   * ------------------------------------------------------------------ */
  printf ("  -- round-trip value verification (MySQL BIT(8) / VARBINARY) --\n");

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
  SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS tbl_bit_verify", SQL_NTS);
  retcode = SQLExecDirect (hstmt,
			   (SQLCHAR *) "CREATE TABLE tbl_bit_verify "
			   "(id INT, b8 BIT(8), vb VARBINARY(16))", SQL_NTS);
  if (retcode == SQL_ERROR)
    {
      printf ("  setup: CREATE TABLE tbl_bit_verify failed\n");
      print_diag (SQL_HANDLE_STMT, hstmt, "create-verify");
    }
  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);

  {
    static const unsigned char v00[] = { 0x00 };
    static const unsigned char v55[] = { 0x55 };
    static const unsigned char vaa[] = { 0xaa };
    static const unsigned char vff[] = { 0xff };
    static const unsigned char vaabb[] = { 0xaa, 0xbb };
    static const unsigned char vdeadbeef[] = { 0xde, 0xad, 0xbe, 0xef };
    int n = 0;

    n += verify_bit_value (hDbc, "tbl_bit_verify", "b8", 1, "BIT(8) 0x00", SQL_BINARY, v00, 1, v00, 1);
    n += verify_bit_value (hDbc, "tbl_bit_verify", "b8", 2, "BIT(8) 0x55", SQL_BINARY, v55, 1, v55, 1);
    n += verify_bit_value (hDbc, "tbl_bit_verify", "b8", 3, "BIT(8) 0xaa", SQL_BINARY, vaa, 1, vaa, 1);
    n += verify_bit_value (hDbc, "tbl_bit_verify", "b8", 4, "BIT(8) 0xff", SQL_BINARY, vff, 1, vff, 1);
    n += verify_bit_value (hDbc, "tbl_bit_verify", "vb", 10, "VARBINARY 0xaabb", SQL_VARBINARY, vaabb, 2, vaabb, 2);
    n += verify_bit_value (hDbc, "tbl_bit_verify", "vb", 11, "VARBINARY 0xdeadbeef", SQL_VARBINARY, vdeadbeef, 4, vdeadbeef, 4);

    printf ("  round-trip verification: %d/6 values verified\n", n);
    verify_fail = 6 - n;
  }

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, hDbc, &hstmt);
//   SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS tbl_bindparambit_test", SQL_NTS);
//   SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS tbl_bindparambit_test2", SQL_NTS);
  SQLExecDirect (hstmt, (SQLCHAR *) "DROP TABLE IF EXISTS tbl_bit_verify", SQL_NTS);
  SQLFreeHandle (SQL_HANDLE_STMT, hstmt);

  retcode = SQLDisconnect (hDbc);
  AreNotEqual (retcode, SQL_ERROR);
  retcode = SQLFreeHandle (SQL_HANDLE_DBC, hDbc);
  retcode = SQLFreeHandle (SQL_HANDLE_ENV, hEnv);
  AreNotEqual (retcode, SQL_ERROR);

  return (verify_fail == 0) ? SQL_SUCCESS : 1;
}
