#define SQL_ODBC3
#include <iostream>
#include <sql.h>
#include <sqlext.h>
#include <chrono>
#include <cstring>
#include <stdexcept>

#define DSN "CUBRID_UNICODE"
#define USER "dba"
#define PASS ""
#define TEST_COUNT 10000
#define BATCH_SIZE 10000

class OdbcError : public std::runtime_error {
public:
    OdbcError(const std::string& msg, SQLHANDLE handle, SQLSMALLINT type) 
        : std::runtime_error(getErrorMessage(msg, handle, type)) {}

private:
    static std::string getErrorMessage(const std::string& msg, SQLHANDLE handle, SQLSMALLINT type) {
        SQLWCHAR sqlState[6] = {0};
        SQLWCHAR msgText[SQL_MAX_MESSAGE_LENGTH] = {0};
        SQLINTEGER nativeError = 0;
        SQLSMALLINT msgLen = 0;
        SQLRETURN ret = SQLGetDiagRecW(type, handle, 1, sqlState, &nativeError, msgText, sizeof(msgText), &msgLen);
        
        std::string errorMsg = "[ERROR] " + msg;
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            errorMsg += " SQLSTATE: " + std::string((char*)sqlState) + 
                       " MESSAGE: " + std::string((char*)msgText);
        }
        return errorMsg;
    }
};

void checkError(SQLRETURN ret, SQLHANDLE handle, SQLSMALLINT type, const std::string& msg) {
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        throw OdbcError(msg, handle, type);
    }
}

class OdbcConnection {
public:
    OdbcConnection() {
        std::cout << "OdbcConnection" << std::endl;
        // 환경 핸들 할당
        ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
        checkError(ret, hEnv, SQL_HANDLE_ENV, "Allocating Environment Handle");
        
        ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
        checkError(ret, hEnv, SQL_HANDLE_ENV, "Setting ODBC Version");
        
        ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
        checkError(ret, hDbc, SQL_HANDLE_DBC, "Allocating Connection Handle");
        
        // 연결 문자열 수정
        SQLWCHAR connStr[256];
        swprintf((wchar_t*)connStr, sizeof(connStr)/sizeof(SQLWCHAR), 
                L"DRIVER=%s;SERVER=localhost;PORT=33000;DB_NAME=demodb;USER=%s;PASSWORD=%s;CHARSET=utf-8;AUTOCOMMIT=ON",
                DSN, USER, PASS);

        std::wcout << L"connStr: " << connStr << std::endl;
        
        // 연결 시도
        SQLWCHAR outConnStr[256];
        SQLSMALLINT outConnStrLen;
        ret = SQLDriverConnectW(hDbc, NULL, connStr, SQL_NTS, 
                              outConnStr, sizeof(outConnStr)/sizeof(SQLWCHAR), 
                              &outConnStrLen, SQL_DRIVER_NOPROMPT);
        
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            SQLWCHAR sqlState[6] = {0};
            SQLWCHAR msgText[SQL_MAX_MESSAGE_LENGTH] = {0};
            SQLINTEGER nativeError = 0;
            SQLSMALLINT msgLen = 0;
            SQLGetDiagRecW(SQL_HANDLE_DBC, hDbc, 1, sqlState, &nativeError, msgText, sizeof(msgText)/sizeof(SQLWCHAR), &msgLen);
            std::wcerr << L"Connection failed: " << msgText << std::endl;
            throw OdbcError("Connecting to Database", hDbc, SQL_HANDLE_DBC);
        }
        
        // 드라이버 이름 가져오기
        SQLWCHAR driverName[256] = {0};
        SQLSMALLINT driverNameLen = 0;
        ret = SQLGetInfoW(hDbc, SQL_DRIVER_NAME, driverName, sizeof(driverName)/sizeof(SQLWCHAR), &driverNameLen);
        checkError(ret, hDbc, SQL_HANDLE_DBC, "Getting Driver Name");
        std::wcout << L"Driver Name: " << driverName << std::endl;

        std::cout << "Connected successfully" << std::endl;
    }
    
    ~OdbcConnection() {
        if (hDbc) {
            SQLDisconnect(hDbc);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        }
        if (hEnv) {
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        }
    }
    
    SQLHDBC getConnection() { return hDbc; }
    SQLHENV getEnvironment() { return hEnv; }

private:
    SQLHENV hEnv = SQL_NULL_HANDLE;
    SQLHDBC hDbc = SQL_NULL_HANDLE;
    SQLRETURN ret;
};

int main() {
    try {
        OdbcConnection conn;
        SQLHSTMT hStmt = SQL_NULL_HANDLE;
        SQLRETURN ret;

        // 테이블 생성
        ret = SQLAllocHandle(SQL_HANDLE_STMT, conn.getConnection(), &hStmt);
        checkError(ret, hStmt, SQL_HANDLE_STMT, "Allocating Statement Handle");

        const char* dropTableSQL = "DROP TABLE IF EXISTS test_table";
        ret = SQLExecDirect(hStmt, (SQLCHAR*)dropTableSQL, SQL_NTS);
        checkError(ret, hStmt, SQL_HANDLE_STMT, "Dropping Table");
        std::cout << "Table dropped successfully" << std::endl;

        const char* createTableSQL = "CREATE TABLE test_table (id INT, name VARCHAR(255))";
        ret = SQLExecDirect(hStmt, (SQLCHAR*)createTableSQL, SQL_NTS);
        checkError(ret, hStmt, SQL_HANDLE_STMT, "Creating Table");
        std::cout << "Table created successfully" << std::endl;

        // 데이터 삽입
        auto start = std::chrono::high_resolution_clock::now();
        
        const char* insertSQL = "INSERT INTO test_table (id, name) VALUES (?, ?)";
        ret = SQLPrepare(hStmt, (SQLCHAR*)insertSQL, SQL_NTS);
        checkError(ret, hStmt, SQL_HANDLE_STMT, "Preparing Insert Statement");

        SQLINTEGER id;
        SQLCHAR name[256];

        // 트랜잭션 시작
        ret = SQLSetConnectAttr(conn.getConnection(), SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
        checkError(ret, conn.getConnection(), SQL_HANDLE_DBC, "Setting AutoCommit Off");

        for (id = 1; id <= TEST_COUNT; id++) {
            snprintf((char*)name, sizeof(name), "oriodb%d", id);
            ret = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, 0, NULL);
            ret = SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 255, 0, name, 0, NULL);
            ret = SQLExecute(hStmt);
            checkError(ret, hStmt, SQL_HANDLE_STMT, "Executing Insert Statement");
        }

        // 트랜잭션 커밋
        ret = SQLEndTran(SQL_HANDLE_DBC, conn.getConnection(), SQL_COMMIT);
        checkError(ret, conn.getConnection(), SQL_HANDLE_DBC, "Committing Transaction");

        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Data inserted in " << std::chrono::duration<double>(end - start).count() << "s" << std::endl;

        // 데이터 검증
        SQLCHAR countSQL[] = "SELECT COUNT(*) FROM test_table";
        ret = SQLExecDirect(hStmt, countSQL, SQL_NTS);
        checkError(ret, hStmt, SQL_HANDLE_STMT, "Executing Count Query");

        SQLINTEGER count;
        ret = SQLBindCol(hStmt, 1, SQL_C_LONG, &count, 0, NULL);
        checkError(ret, hStmt, SQL_HANDLE_STMT, "Binding Count Column");

        ret = SQLFetch(hStmt);
        checkError(ret, hStmt, SQL_HANDLE_STMT, "Fetching Count Result");

        std::cout << "Data count after insert: " << count << std::endl;

        // 리소스 정리
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
