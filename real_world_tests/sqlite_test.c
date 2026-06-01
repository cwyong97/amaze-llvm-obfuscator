#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"

static int callback(void *data, int argc, char **argv, char **azColName) {
    int *sum = (int*)data;
    if (argc > 0 && argv[0]) {
        *sum = atoi(argv[0]);
    }
    return 0;
}

int main() {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    // 1. Open an in-memory database
    rc = sqlite3_open(":memory:", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // 2. Create a table
    const char *sql_create = "CREATE TABLE users (id INT, name TEXT);";
    rc = sqlite3_exec(db, sql_create, NULL, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return 1;
    }

    // 3. Insert 500 records dynamically (using prepared statement for speed)
    const char *sql_insert = "INSERT INTO users VALUES (?, ?);";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare insert statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Begin transaction for bulk insert
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    for (int i = 0; i < 500; i++) {
        sqlite3_bind_int(stmt, 1, i);
        char name[32];
        sprintf(name, "User_%d", i);
        sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Failed to execute step: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return 1;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(stmt);

    // 4. Query the database and sum the IDs
    int sum = 0;
    const char *sql_select = "SELECT SUM(id) FROM users;";
    rc = sqlite3_exec(db, sql_select, callback, &sum, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return 1;
    }

    // 5. Verify the sum (Expected: 500 * 499 / 2 = 124750)
    int expected = 500 * 499 / 2;
    if (sum == expected) {
        printf("[✔] SQLite Test Passed! Expected Sum: %d, Query Sum: %d\n", expected, sum);
        sqlite3_close(db);
        return 0;
    } else {
        fprintf(stderr, "[✗] SQLite Test Failed! Expected Sum: %d, Query Sum: %d\n", expected, sum);
        sqlite3_close(db);
        return 1;
    }
}
