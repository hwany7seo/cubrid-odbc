#include "stdafx.h"
#include "CppUnitTest.h"

#include <stdio.h>  
#include <string.h>  
#include <windows.h>  
#include <sql.h>  
#include <sqlext.h>  
#include <odbcss.h>  

#define MAXBUFLEN	640
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTestCPP
{		
	TEST_CLASS(UnitTest)
	{
	public:
		TEST_METHOD(TestClassInit)
		{
		}

		TEST_METHOD(QueryPlan)
		{
			RETCODE retcode;

			SQLWCHAR query_plan[32768] = { 0, };

			SQLHENV env;
			SQLHDBC dbc;
			SQLHSTMT hStmt = SQL_NULL_HSTMT;
			SWORD plm_pcbErrorMsg = 0;
			SQLINTEGER diag_rec;

			SQLLEN len;

			/* Allocate an environment handle */
			SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
			/* We want ODBC 3 support */
			SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
			/* Allocate a connection handle */
			SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

			retcode = SQLConnect(dbc, L"CUBRID Driver Unicode", SQL_NTS, L"dba", SQL_NTS, NULL, SQL_NTS);

			if (retcode == SQL_ERROR) {
				SQLGetDiagField(SQL_HANDLE_DBC, dbc, 0, SQL_DIAG_NUMBER, &diag_rec, 0, &plm_pcbErrorMsg);
			}

			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			// Allocate statement handle and execute a query
			retcode = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hStmt);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			retcode = SQLExecDirect(hStmt, L"SELECT name as \"user\", db_user, groups FROM db_user", SQL_NTS);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			retcode = SQLFetch(hStmt);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			getchar();

			// Get Query Plan
			retcode = SQLGetData(hStmt, 1, SQL_C_DEFAULT, (SQLPOINTER)query_plan, 0, &len);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			if (retcode == SQL_SUCCESS_WITH_INFO) {
				retcode = SQLGetData(hStmt, 1, SQL_C_DEFAULT, (SQLPOINTER)query_plan, len, &len);
			}

			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			if (strlen((const char *)query_plan) > 0) {
				int c = strncmp((const char *)query_plan, (const char *)L"Join graph", 10);
				Assert::AreEqual(c, 0);
			}
			else {
				Assert::Fail(L"Query Plan is null");
			}
			// Clean up.   
			SQLDisconnect(dbc);
			SQLFreeHandle(SQL_HANDLE_DBC, dbc);
			SQLFreeHandle(SQL_HANDLE_ENV, env);
		}

		TEST_METHOD(QueryPlanMultiByte)
		{
			RETCODE retcode;

			SQLWCHAR query_plan[32768] = { 0, };

			SQLHENV env;
			SQLHDBC dbc;
			SQLHSTMT hStmt = SQL_NULL_HSTMT;
			SWORD plm_pcbErrorMsg = 0;
			SQLINTEGER diag_rec;

			SQLLEN len;

			/* Allocate an environment handle */
			SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
			/* We want ODBC 3 support */
			SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
			/* Allocate a connection handle */
			SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

			retcode = SQLConnect(dbc, L"CUBRID Driver Unicode2", SQL_NTS, L"dba", SQL_NTS, NULL, SQL_NTS);

			if (retcode == SQL_ERROR) {
				SQLGetDiagField(SQL_HANDLE_DBC, dbc, 0, SQL_DIAG_NUMBER, &diag_rec, 0, &plm_pcbErrorMsg);
			}

			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			// Allocate statement handle and execute a query
			retcode = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hStmt);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			retcode = SQLExecDirect(hStmt, L"DROP TABLE IF EXISTS [���̺�] ", SQL_NTS);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);
			retcode = SQLExecDirect(hStmt, L"CREATE TABLE [���̺�] ([�̸�] varchar(16), [����] integer)", SQL_NTS);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);
			retcode = SQLExecDirect(hStmt, L"INSERT INTO [���̺�] VALUES ('ȫ�浿', 25)", SQL_NTS);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);
			retcode = SQLExecDirect(hStmt, L"SELECT [�̸�], [����] FROM [���̺�] WHERE [����] > 19", SQL_NTS);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			retcode = SQLFetch(hStmt);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			//MessageBox(NULL, L"Contents", L"Title", MB_OK);

			// Get Query Plan
			retcode = SQLGetData(hStmt, 1, SQL_C_DEFAULT, (SQLPOINTER)query_plan, 0, &len);
			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			if (retcode == SQL_SUCCESS_WITH_INFO) {
				retcode = SQLGetData(hStmt, 1, SQL_C_DEFAULT, (SQLPOINTER)query_plan, len, &len);
			}

			Assert::AreNotEqual((int)retcode, SQL_ERROR);

			if (strlen((const char *)query_plan) > 0) {
				wchar_t expected[200] = L"Join graph segments (f indicates final):\r\nseg[0]: [0]\r\nseg[1]: �̸�[0] (f)\r\nseg[2]: ����[0] (f)\r\nJoin graph nodes:\r\nnode[0]: ���̺� ���̺�(1/1) (sargs 0) (loc 0)\r\nJoin graph terms:\r\nterm[0]: [���̺�].[����] range";
				int c = wcsncmp(query_plan, expected, wcslen(expected));
				Assert::AreEqual(c, 0);
			}
			else {
				Assert::Fail(L"Query Plan is null");
			}
			// Clean up.   
			SQLDisconnect(dbc);
			SQLFreeHandle(SQL_HANDLE_DBC, dbc);
			SQLFreeHandle(SQL_HANDLE_ENV, env);
		}
	};
}