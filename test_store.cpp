// test_store.cpp - console tests for the file-handling layer (no Qt needed).
// Build:  g++ -std=c++17 -Isrc tests/test_store.cpp src/RecordStore.cpp -o test_store
#include "RecordStore.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

static int g_failed = 0;
static int g_total  = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        ++g_total;                                                          \
        if (!(cond)) {                                                      \
            ++g_failed;                                                     \
            std::cout << "  FAIL line " << __LINE__ << ": " #cond << "\n";  \
        }                                                                   \
    } while (0)

static StudentRecord make(const std::string &roll, const std::string &name, double gpa = 3.5)
{
    StudentRecord r;
    r.name = name; r.rollNo = roll; r.fatherName = "Father of " + name;
    r.caste = "Memon"; r.district = "Khairpur"; r.gpa = gpa;
    return r;
}

static std::string slurp(const std::string &path)
{
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main()
{
    const std::string path = "test_students.txt";
    std::remove(path.c_str());

    std::cout << "Add / persistence\n";
    {
        RecordStore s(path);
        s.load();
        CHECK(s.size() == 0);
        CHECK(s.add(make("26SW001", "Ali")).ok);
        CHECK(s.add(make("26SW002", "Sara", 3.91)).ok);
        CHECK(s.size() == 2);
    }
    {
        RecordStore s(path);                 // fresh object = reload from disk
        s.load();
        CHECK(s.size() == 2);
        CHECK(s.at(1).name == "Sara");
        CHECK(s.at(1).gpa > 3.909 && s.at(1).gpa < 3.911);
    }
    CHECK(slurp(path) == "Ali,26SW001,Father of Ali,Memon,Khairpur,3.50\n"
                         "Sara,26SW002,Father of Sara,Memon,Khairpur,3.91\n");

    std::cout << "Validation\n";
    {
        RecordStore s(path); s.load();
        CHECK(!s.add(make("26SW001", "Dup")).ok);            // duplicate roll
        CHECK(!s.add(make("", "NoRoll")).ok);                // empty roll
        CHECK(!s.add(make("26SW010", "")).ok);               // empty name
        CHECK(!s.add(make("26SW011", "Hi", 4.5)).ok);        // GPA too high
        CHECK(!s.add(make("26SW012", "Lo", -0.1)).ok);       // GPA negative
        CHECK(s.add(make("  26SW013  ", "  Trim  ")).ok);   // whitespace trimmed
        CHECK(s.indexOfRoll("26SW013") >= 0);
        CHECK(s.at(static_cast<std::size_t>(s.indexOfRoll("26SW013"))).name == "Trim");
        CHECK(s.size() == 3);
    }

    std::cout << "Update\n";
    {
        RecordStore s(path); s.load();
        StudentRecord changed = make("IGNORED", "Ali Raza", 3.75);
        CHECK(s.update("26SW001", changed).ok);
        CHECK(s.at(0).name == "Ali Raza");
        CHECK(s.at(0).rollNo == "26SW001");                   // key never changes
        CHECK(!s.update("NOPE", changed).ok);                 // unknown roll
        CHECK(!s.update("26SW001", make("x", "Bad", 9.0)).ok);// invalid data rejected
        CHECK(s.at(0).gpa > 3.749 && s.at(0).gpa < 3.751);    // unchanged by failed update
        RecordStore again(path); again.load();
        CHECK(again.at(0).name == "Ali Raza");                // update reached the file
    }

    std::cout << "Delete\n";
    {
        RecordStore s(path); s.load();
        CHECK(s.remove("26SW002").ok);
        CHECK(!s.remove("26SW002").ok);                       // already gone
        CHECK(s.size() == 2);
        CHECK(s.indexOfRoll("26SW002") == -1);
        RecordStore again(path); again.load();
        CHECK(again.size() == 2);
        CHECK(again.remove("26SW001").ok);
        CHECK(again.remove("26SW013").ok);
        CHECK(again.size() == 0);
        CHECK(slurp(path).empty());
    }

    std::cout << "Special characters and bad lines\n";
    {
        StudentRecord tricky = make("26SW050", "Khan, Ahmed");
        tricky.fatherName = "He said \"hi\"";
        RecordStore s(path); s.load();
        CHECK(s.add(tricky).ok);
        RecordStore again(path); again.load();
        CHECK(again.size() == 1);
        CHECK(again.at(0).name == "Khan, Ahmed");             // comma survived
        CHECK(again.at(0).fatherName == "He said \"hi\"");    // quotes survived
    }
    {
        std::ofstream out(path, std::ios::trunc);
        out << "good,R1,F,C,D,3.00\r\n"                       // Windows line ending
            << "too,few,fields\n"
            << "bad,R2,F,C,D,notanumber\n"
            << "\n"
            << "good2,R3,F,C,D,2.50\n";
    }
    {
        RecordStore s(path); s.load();
        CHECK(s.size() == 2);
        CHECK(s.skippedLines() == 2);
        CHECK(s.at(0).name == "good");
    }
    {
        RecordStore s("does_not_exist_dir/x.txt");            // missing file: empty, no crash
        CHECK(s.load());
        CHECK(s.size() == 0);
    }

    std::remove(path.c_str());
    std::cout << "\n" << (g_total - g_failed) << "/" << g_total << " checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
