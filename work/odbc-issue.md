# CUBRID ODBC — BIT 바인딩 / SQLDescribeParam 이슈 정리

작성 대상: `cubrid-odbc` 드라이버 (`src/`) 및 `linux_test/sql_bindparambit*` 테스트.

이 문서는 (A) 세 드라이버(CUBRID / MySQL / Oracle)의 BIT 파라미터 바인딩 동작 공정 비교 결과,
(B) 진행 예정인 개선 항목 1·2·3의 상세 계획, (C) 현재 드라이버에 들어간 변환 전체 정리를 담는다.

---

## 0. 사전 주의 (재현 환경)

- **드라이버 재빌드/재배포 필수.** 배포본 `~/cubrid-odbc/lib/libcubrid-odbcw.so.11.4.1` 가
  최신 소스보다 오래되면 결과가 달라진다(실제로 stale 배포본에서는 `SQL_C_BIT→SQL_BIT` 가 실패했다).
  ```
  cd cubrid-odbc/build && make
  cp -f libcubrid-odbc*.so.11.4.1 ~/cubrid-odbc/lib/
  ```
- **Oracle 실행 시** Instant Client 경로를 `LD_LIBRARY_PATH` 에 추가해야 드라이버가 로드된다.
  ```
  export LD_LIBRARY_PATH=/home/hwanyseo/odbc/oracle/instantclient_23_8:/home/hwanyseo/cubrid-odbc/lib:$LD_LIBRARY_PATH
  ```
- DSN 정보는 `~/.odbc.ini` / `~/.odbcinst.ini` (CUBRID_Unicode, MYSQL_TESTDB, OracleODBC-11g).
- 현재 드라이버 버전: `11.4.1.0167`.

---

## A. 공정 비교 (항목 3 결과)

세 테스트 파일의 variant 세트를 **동일한 7개 조합**으로 맞춘 뒤 실행했다. 컬럼 타입만 DB별로
불가피하게 다르다(CUBRID `BIT(8)`, MySQL `BIT(8)`, Oracle `RAW` — Oracle에는 BIT 타입이 없음).

### A-1. 단일 파라미터 바인딩 매트릭스 (C타입 → SQL타입)

| # | C타입 → SQL타입 | CUBRID `BIT(8)` | MySQL `BIT(8)` | Oracle `RAW` |
|---|---|:---:|:---:|:---:|
| 1 | `SQL_C_BINARY` → `SQL_BIT` | ✅ OK | ❌ FAIL | ✅ OK |
| 2 | `SQL_C_BINARY` → `SQL_BINARY` | ✅ OK | ✅ OK | ✅ OK |
| 3 | `SQL_C_BINARY` → `SQL_VARBINARY` | ✅ OK | ✅ OK | ✅ OK |
| 4 | `SQL_C_BIT` → `SQL_BIT` | ✅ OK | ✅ OK | ❌ FAIL |
| 5 | `SQL_C_CHAR` → `SQL_BIT` | ✅ OK | ❌ FAIL | ✅ OK |
| 6 | `SQL_C_CHAR` → `SQL_CHAR` | ✅ OK | ✅ OK | ✅ OK |
| 7 | `SQL_C_BIT` → `SQL_BINARY` | ✅ OK | ✅ OK | ❌ FAIL |
| | **합계** | **7/7** | **5/7** | **5/7** |

### A-2. 값 왕복(round-trip) 검증 — INSERT 후 SELECT로 되읽어 비교

| DB | 결과 | 비고 |
|---|:---:|---|
| CUBRID | **10/10 VERIFIED** | `BIT(8)` 다양한 바이트 + `BIT(16)` 패딩(`0xaa`→`0xaa00`) + `BIT VARYING` |
| MySQL | 6/6 VERIFIED | `BIT(8)` 1바이트 + `VARBINARY` |
| Oracle | 6/6 VERIFIED | `RAW(1/2/4)` 바이트 그대로 |

### A-3. 해석 (드라이버별 특성)

- **CUBRID (수정본): 7/7 전부 OK, 값 왕복 10/10.** 이번 드라이버 수정으로 세 DB 중 가장 관대하고
  완전하다. 관대함이 값 손상 없이(왕복 검증 통과) 보장된다.
- **MySQL Connector/ODBC 9.6**: `ParameterType=SQL_BIT` 를 `SQL_C_BINARY`/`SQL_C_CHAR`에서 받으면
  **잘못된 SQL을 생성**한다 (`... near ')'` 구문오류, native 1064). 즉 SQL_BIT 파라미터 타입 처리에
  드라이버 버그가 있다. `SQL_C_BIT→SQL_BIT`(#4)만 통과. 2-파라미터 케이스도 같은 이유로 실패.
  `SQLDescribeParam`은 전부 `SQL_VARCHAR(12)/255` 로 반환(파라미터 서술 미구현).
- **Oracle 11g (Instant Client)**: `SQL_C_BIT` 을 `NUMBER`로 취급해 `RAW` 컬럼과 불일치
  (`ORA-00932: expected BINARY got NUMBER`). `SQL_C_BINARY`/`SQL_C_CHAR` 는 모두 통과.
  `SQLDescribeParam`은 `RAW` 를 `SQL_VARBINARY(-3)` 로 정확히 서술.

> 참고: `SQL_C_CHAR→SQL_BIT`(#5)가 CUBRID에서 OK인 것은 아래 **항목 2**에서 다루는
> "char→BIT 불리언 해석" 변환 때문이다. 이 서브케이스를 제거하면 CUBRID도 MySQL처럼 #5가 FAIL이 된다.

### A-4. 테스트 파일 정렬 내역 (항목 3에서 수행)

- 세 파일 `main()` 의 variant 7개를 **동일한 (C타입, SQL타입, 값) 조합/순서**로 통일.
- 실험 중 남았던 미사용 지역변수(`bit_val`/`var_val`/`bitData`, 라벨과 값이 어긋나던 호출) 제거.
- 세 파일 모두 **INSERT→SELECT 값 왕복 검증** 섹션 보유(DB별 저장 의미에 맞춰 기대값 설정).
- 실행:
  ```
  ./odbc_test sql_bindparambit          # CUBRID
  ./odbc_test sql_bindparambit_mysql    # MySQL
  ./odbc_test sql_bindparambit_oracle   # Oracle
  ```

---

## B. 진행 예정 항목 1·2·3 상세 계획

### 항목 1 — `SQLDescribeParam` 파라미터 정보 캐싱 최적화 (성능)

**문제**
- `odbc_describe_param()`(`src/odbc_statement.c`)는 `SQLDescribeParam` 호출마다 `cci_get_param_info()`를
  호출한다.
- `cci_get_param_info()`는 **전체 파라미터 배열**을 CAS 서버에서 받아오는 **네트워크 왕복 1회**다.
- 그런데 파라미터 N개를 서술하면(go-odbc 는 파라미터마다 `SQLDescribeParam` 호출) **같은 배열을 N번
  네트워크로 재조회**한다 → prepare 1건당 O(N) 왕복.

**개선안 (lazy 캐시)**
1. `ODBC_STATEMENT` 구조체에 캐시 필드 추가:
   `T_CCI_PARAM_INFO *param_info; int param_info_count; char param_info_loaded;`
2. `odbc_describe_param()`:
   - `param_info_loaded==0` 이면 `cci_get_param_info()` 1회 호출 → `param_info`/`param_info_count` 저장,
     `param_info_loaded=1`.
   - 이후에는 캐시에서 `cci_slot` 으로 해당 슬롯만 읽음 (네트워크 없음).
   - 실패 시 기존처럼 `VARCHAR(255)` fallback(캐시는 "시도했으나 없음" 상태로 표기해 반복 호출 방지).
3. **캐시 무효화/해제** (누수·정합성 핵심):
   - `odbc_prepare()`(재-prepare), `reset_result_set()`/statement 재사용, `SQLFreeStmt`/`SQLFreeHandle(STMT)`
     경로에서 `cci_param_info_free(param_info)` 후 `param_info=NULL; param_info_loaded=0;`.
4. lazy 로 두는 이유: `SQLDescribeParam` 을 쓰지 않는 앱은 비용 0.

**영향 파일**: `src/odbc_statement.h`(구조체 필드), `src/odbc_statement.c`(describe/prepare/reset/free).
**리스크**: 캐시 수명 관리(재-prepare·close 시 반드시 무효화). 스레드 안전성은 statement 단위이므로 기존과 동일.
**기대 효과**: prepare당 파라미터 서술 네트워크 왕복 **N회 → 1회**.

### 항목 2 — `char → BIT` 불리언 해석 서브케이스 제거 (정합성)

**현재 동작**
- `odbc_execute()`(`src/odbc_statement.c`)의 char→binary 블록:
  `a_type==CCI_A_TYPE_STR && (u_type==CCI_U_TYPE_BIT || u_type==CCI_U_TYPE_VARBIT)` 일 때 문자열을
  `T_CCI_BIT`로 재포장.
  - `u_type==CCI_U_TYPE_BIT`(고정 BIT): 문자를 **불리언으로 해석**(`'0'`/빈 → false=0x00, 그 외 → true=0x80).
  - `u_type==CCI_U_TYPE_VARBIT`(BLOB/BIT VARYING): 원시 바이트 그대로.

**제거 근거**
- 문자 `"1"`을 자동으로 불리언 `0x80`으로 바꾸는 것은 **가장 자의적(opinionated)** 인 변환이다.
- 실사용에서 "문자 데이터를 BIT 컬럼에" 바인딩하는 경우는 드물다. 불리언은 `SQL_C_BIT`로 바인딩하는 것이 정석.
- MySQL도 이 조합(`SQL_C_CHAR→SQL_BIT`)은 지원하지 않는다(A-1 #5 FAIL). 제거하면 동작이 더 예측 가능해진다.

**개선안**
- char→binary 블록의 조건을 `u_type==CCI_U_TYPE_VARBIT` **로만** 축소(=BLOB/BIT VARYING의 문자→원시바이트
  관대함은 유지, 고정 BIT의 문자→불리언 매직만 제거).
- 결과: `SQL_C_CHAR→SQL_BIT`(a_type=STR, u_type=BIT)는 `cci_bind_param(STR,BIT)` → `CCI_ER_TYPE_CONVERSION`
  로 **명확히 실패**(앱은 불리언에 `SQL_C_BIT` 사용).
- **불변**: `SQL_C_BIT→SQL_BIT/`(값 변환 경로, 항목과 무관) 계속 동작 → go-odbc `bool` 바인딩 정상.
  `SQL_C_CHAR→BLOB`(VARBIT) 문자→바이트 저장도 유지 → go-odbc 문자열→BLOB 정상.

**영향 파일**: `src/odbc_statement.c` (char→binary 블록의 조건과 BIT 분기 제거).
`odbc_make_cci_bit_from_bool()` 는 `SQL_C_BIT` 경로가 계속 쓰므로 유지.
**영향 결과**: A-1 매트릭스에서 CUBRID #5 가 `OK → FAIL` 로 바뀌어 MySQL과 동일해진다(의도된 변화).
**트레이드오프**: Oracle 은 `SQL_C_CHAR→SQL_BIT` 를 허용(RAW로 저장)하므로 그보다는 덜 관대해진다.
다만 "텍스트에서 불리언을 추측하지 않는다"는 원칙이 더 안전하다는 판단.

### 항목 3 — 세 테스트 파일 variant 대칭화 (완료)

- 위 **A-4** 참고. 세 파일이 동일 7-조합 + 값 왕복 검증을 갖도록 정렬 완료, 실행·비교까지 수행.
- 남은 후속(선택): variant 라벨 문구를 세 파일에서 완전히 동일 문자열로 통일(현재 조합은 동일, 문구만 상이).

---

## C. 현재 드라이버에 들어간 변환/변경 전체 정리

(기준 커밋 `455bb6e` 이후, `src/` diff 기반)

| # | 위치 | 분류 | 트리거 | 하는 일 | 런타임 비용 |
|---|---|---|---|---|---|
| 1 | `SQLDescribeParam`→`odbc_describe_param` | 메타데이터 | 앱이 `SQLDescribeParam` 호출 | `cci_get_param_info`로 파라미터 타입 조회·보고 | **네트워크 왕복(항목1 대상)** |
| 2 | `odbc_type.c` 테이블: DECIMAL/NUMERIC `decimal_digits` `0→-1` | 메타데이터 | describe/describecol | scale을 실제값으로 반환(기존 버그: 항상 0) | 0 (테이블 상수) |
| 3 | `odbc_column_size`/`odbc_decimal_digits` 사용 | 메타데이터 | describe | col_size/dec_digits 테이블 기반 산출 | 0 (테이블 조회) |
| 4 | LOB 의사타입→표준(`SQL_BLOB→SQL_LONGVARBINARY`, `SQL_CLOB→SQL_LONGVARCHAR`) | 메타데이터 | describe | 비표준 코드 정규화 | 0 (비교 2회) |
| 5 | `odbc_type_to_cci_u_type`에 `SQL_BIT→CCI_U_TYPE_BIT` | 타입 매핑 | bind마다 | ODBC 불리언 비트→CUBRID BIT | 0 (switch) |
| 6 | `odbc_value_to_cci`에 `SQL_C_BIT` 케이스 (+`odbc_make_cci_bit_from_bool`) | 값 변환 | `SQL_C_BIT` 바인딩 시 | 0/1 → `T_CCI_BIT`(0x80/0x00) | malloc + 1byte |
| 7 | `odbc_execute` char→binary 블록 | 값 변환 | **문자데이터를 BIT/VARBIT에 바인딩 시에만** | STR→`T_CCI_BIT` (BIT:불리언 0x80 / VARBIT:원시바이트) | 조건비교 2회 + (해당 시) malloc+memcpy |
| 8 | `odbc_execute` bind 후 `CCI_A_TYPE_BIT` buf 해제 | 메모리 | BIT 바인딩 후 | `T_CCI_BIT.buf` 누수 정리(기존 `[]byte` 누수 포함) | UT_FREE 1회 |

### 성능 판단 요약
- **값 변환(#6, #7)은 hot path 밖.** 일반 바인딩(int→int, string→varchar, []byte→blob)은 진입하지 않으며,
  진입 시 비용도 기존 `SQL_C_BINARY` 경로와 동일 수준(작은 malloc+copy). → **성능 영향 사실상 없음.**
- **진짜 개선 포인트는 #1(`SQLDescribeParam`).** `cci_get_param_info` 네트워크 왕복을 파라미터마다 반복 →
  **항목 1(캐싱)** 으로 prepare당 1회로 감소.

### "드라이버가 변환하는 게 맞나" 판단
- 메타데이터/타입매핑(#1~5)과 `SQL_C_BIT`(#6)은 ODBC 드라이버의 정당한 책임(위치 적절).
- Oracle/MySQL ODBC 드라이버도 permissive(문자→BLOB 수용)하므로 **드라이버가 변환하는 것 자체는 표준**.
- 단 **#7의 char→BIT 불리언 해석**이 가장 자의적 → **항목 2에서 제거** 예정.
- 계층 정합성의 정석은 CCI `bind_value_conversion`에 `STR→BIT/VARBIT` 를 추가하는 것이나, 공유 vendored
  라이브러리라 영향범위가 커 별도 검토 대상(현 시점 보류).

---

## D. 관련 파일

- 드라이버: `src/odbc_interface.c`, `src/odbc_statement.c`, `src/odbc_statement.h`,
  `src/odbc_type.c`, `src/odbc_type.h`
- 테스트: `linux_test/sql_bindparambit.c` (CUBRID), `linux_test/sql_bindparambit_mysql.c`,
  `linux_test/sql_bindparambit_oracle.c`, `linux_test/CMakeLists.txt`
