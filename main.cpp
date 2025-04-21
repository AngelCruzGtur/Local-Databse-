#include <iostream>
#include <fstream>
#include <vector>
#include <sqlite3.h>
#include <string>
#include <filesystem>
#include <optional>

using namespace std;
namespace fs = std::filesystem;

// Function to safely read a file into a binary vector
optional<vector<char>> readFile(const string& filePath) 
{
    ifstream file(filePath, ios::binary | ios::ate);
    if (!file) 
    {
        cerr << "Error opening file: " << filePath << endl;
        return nullopt;
    }
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) 
    {
        cerr << "Error reading file: " << filePath << endl;
        return nullopt;
    }
    return buffer;
}

// Function to check if a file is a PDF
bool isPDF(const string& filePath) 
{
    return fs::path(filePath).extension() == ".pdf";
}

int main() 
{
    cout << "Enter File Path or Directory Path: ";
    string path;
    cin >> path;

    // Initialize SQLite database
    sqlite3* db;
    char* errorMessage = 0;

    // Open database 
    // Create a new database to a name that isnt example for both main.cpp and server.py
    // Im using example.db for testing purposes
    if (sqlite3_open("example.db", &db) != SQLITE_OK) 
    {
        cerr << "Error opening database: " << sqlite3_errmsg(db) << endl;
        return -1;
    }
    cout << "Connected to database successfully!" << endl;

    // Create table if it doesn't exist
    string createTableSQL = "CREATE TABLE IF NOT EXISTS Files ("
                            "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                            "FileName TEXT NOT NULL, "
                            "FilePath TEXT NOT NULL, "
                            "ParentDirectory TEXT NOT NULL, "  // New column for folder structure
                            "FileType TEXT NOT NULL, "
                            "FileData BLOB NOT NULL);";

    if (sqlite3_exec(db, createTableSQL.c_str(), 0, 0, &errorMessage) != SQLITE_OK) 
    {
        cerr << "Error creating table: " << errorMessage << endl;
        sqlite3_free(errorMessage);
        sqlite3_close(db);
        return -1;
    }
    cout << "Table verified successfully!" << endl;

    try 
    {
        auto processFile = [&](const string& filePath) 
        {
            auto fileDataOpt = readFile(filePath);
            if (!fileDataOpt) return;  // Skip if file read failed

            vector<char> fileData = move(fileDataOpt.value());
            string fileName = fs::path(filePath).filename().string();
            string parentDirectory = fs::path(filePath).parent_path().string(); // Get the parent directory of the file
            string fileType = isPDF(filePath) ? "PDF" : "Other"; // Identify if it's a PDF

            string insertSQL = "INSERT INTO Files (FileName, FilePath, ParentDirectory, FileType, FileData) VALUES (?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt;

            if (sqlite3_prepare_v2(db, insertSQL.c_str(), -1, &stmt, 0) != SQLITE_OK) 
            {
                cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
                return;
            }

            if (sqlite3_bind_text(stmt, 1, fileName.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
                sqlite3_bind_text(stmt, 2, filePath.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
                sqlite3_bind_text(stmt, 3, parentDirectory.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||  // Bind parent directory
                sqlite3_bind_text(stmt, 4, fileType.c_str(), -1, SQLITE_STATIC) != SQLITE_OK ||
                sqlite3_bind_blob(stmt, 5, fileData.data(), fileData.size(), SQLITE_TRANSIENT) != SQLITE_OK) 
            {
                cerr << "Error binding values: " << sqlite3_errmsg(db) << endl;
                sqlite3_finalize(stmt);
                return;
            }

            if (sqlite3_step(stmt) != SQLITE_DONE) 
            {
                cerr << "Error inserting file: " << sqlite3_errmsg(db) << endl;
            }
            else 
            {
                cout << "File '" << fileName << "' (" << filePath << ") [" << fileType << "] inserted successfully!" << endl;
            }

            sqlite3_finalize(stmt);
        };

        if (fs::is_directory(path)) 
        {
            // Process all files in the directory
            for (const auto& entry : fs::recursive_directory_iterator(path)) 
            {
                if (fs::is_regular_file(entry)) 
                {
                    processFile(entry.path().string());
                }
            }
        } 
        else if (fs::is_regular_file(path)) 
        {
            processFile(path);
        } 
        else 
        {
            cerr << "Invalid path: " << path << endl;
        }
    } 
    catch (const exception& e) 
    {
        cerr << "Exception: " << e.what() << endl;
    }

    // Close the database connection
    sqlite3_close(db);
    cout << "Database connection closed." << endl;

    return 0;
}
