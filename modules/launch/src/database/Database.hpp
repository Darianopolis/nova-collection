#pragma once

#include <nova/core/nova_Core.hpp>

#include <sqlite3.h>

using namespace nova::types;

class Database
{
public:
    Database(std::string path);
    ~Database();

    sqlite3* GetDB();
private:
    sqlite3* db{};
};

class Statement
{
public:
    Statement(Database &db, std::string sql);
    ~Statement();

    void ResetIfComplete();
    bool Step();
    i64 Insert();
    void SetNull(u32 index);
    void SetString(u32 index, std::string_view str);
    void SetInt(u32 index, i64 value);
    void SetReal(u32 index, f64 value);

    std::string_view GetString(u32 index);
    i64 GetInt(u32 index);
    f64 GetReal(u32 index);
private:
    sqlite3* db{};
    sqlite3_stmt* stmt{};
    bool complete = false;
};