// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbwrapper.h"

#include "fs.h"
#include "util.h"
#include "random.h"

//#include <leveldb/cache.h>
//#include <leveldb/env.h>
//#include <leveldb/filter_policy.h>
#include <memenv.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>

//class CBitcoinLevelDBLogger : public leveldb::Logger {
//public:
//    // This code is adapted from posix_logger.h, which is why it is using vsprintf.
//    // Please do not do this in normal code
//    virtual void Logv(const char * format, va_list ap) override {
//            if (!LogAcceptCategory(BCLog::LEVELDB)) {
//                return;
//            }
//            char buffer[500];
//            for (int iter = 0; iter < 2; iter++) {
//                char* base;
//                int bufsize;
//                if (iter == 0) {
//                    bufsize = sizeof(buffer);
//                    base = buffer;
//                }
//                else {
//                    bufsize = 30000;
//                    base = new char[bufsize];
//                }
//                char* p = base;
//                char* limit = base + bufsize;
//
//                // Print the message
//                if (p < limit) {
//                    va_list backup_ap;
//                    va_copy(backup_ap, ap);
//                    // Do not use vsnprintf elsewhere in bitcoin source code, see above.
//                    p += vsnprintf(p, limit - p, format, backup_ap);
//                    va_end(backup_ap);
//                }
//
//                // Truncate to available space if necessary
//                if (p >= limit) {
//                    if (iter == 0) {
//                        continue;       // Try again with larger buffer
//                    }
//                    else {
//                        p = limit - 1;
//                    }
//                }
//
//                // Add newline if necessary
//                if (p == base || p[-1] != '\n') {
//                    *p++ = '\n';
//                }
//
//                assert(p <= limit);
//                base[std::min(bufsize - 1, (int)(p - base))] = '\0';
//                LogPrintStr(base);
//                if (base != buffer) {
//                    delete[] base;
//                }
//                break;
//            }
//    }
//};


//sqlite github version
static const char *sql_db_init[] = {
    "CREATE TABLE tab(k BLOB PRIMARY KEY, v BLOB) WITHOUT ROWID",
};





static const char *sql_db_configure[] = {
//    "PRAGMA page_size=4096",
//	"PRAGMA cache_size=-4000",    // max cache size; negative = # of kibibytes
//	"PRAGMA journal_mode=WAL",
//	"PRAGMA wal_autocheckpoint=10000",
//
	"PRAGMA page_size = 4096",
	"PRAGMA cache_size=10000",
//	"PRAGMA locking_mode=EXCLUSIVE",
	"PRAGMA journal_mode=WAL"
	"PRAGMA SQLITE_DEFAULT_CACHE_SIZE=5000",
	"PRAGMA schema.journal_size_limit = 1048576"

};

static const char *sql_stmt_text[] = {
    "REPLACE INTO tab(k, v) VALUES(:k, :v)",            // DBW_PUT
    "SELECT v FROM tab WHERE k = :k",                   // DBW_GET
    "DELETE FROM tab WHERE k = :k",                     // DBW_DELETE
    "BEGIN TRANSACTION",                                // DBW_BEGIN
    "COMMIT TRANSACTION",                               // DBW_COMMIT
    "SELECT COUNT(*) FROM tab",                         // DBW_GET_ALL_COUNT
    "SELECT k, v FROM tab WHERE k >= :k ORDER BY k",    // DBW_SEEK_SORTED
};


//static leveldb::Options GetOptions(size_t nCacheSize)
//{
//    leveldb::Options options;
//    options.block_cache = leveldb::NewLRUCache(nCacheSize / 2);
//    options.write_buffer_size = nCacheSize / 4; // up to two write buffers may be held in memory simultaneously
//    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
//    options.compression = leveldb::kNoCompression;
//    options.max_open_files = 64;
//    options.info_log = new CBitcoinLevelDBLogger();
//    if (leveldb::kMajorVersion > 1 || (leveldb::kMajorVersion == 1 && leveldb::kMinorVersion >= 16)) {
//        // LevelDB versions before 1.16 consider short writes to be corruption. Only trigger error
//        // on corruption in later versions.
//        options.paranoid_checks = true;
//    }
//    return options;
//}

CDBWrapper::CDBWrapper(const fs::path& path, size_t nCacheSize, bool fMemory, bool fWipe, bool obfuscate)
{
//    penv = nullptr;
//    readoptions.verify_checksums = true;
//    iteroptions.verify_checksums = true;
//    iteroptions.fill_cache = false;
//    syncoptions.sync = true;
//    options = GetOptions(nCacheSize);
//    options.create_if_missing = true;

	std::string fp;
	const char *filename = NULL;
	bool need_init = false;

    if (fMemory) {
//        penv = leveldb::NewMemEnv(leveldb::Env::Default());
//        options.env = penv;
    	filename = ":memory:";
		need_init = true;
    } else {
    	fp = path.string();
		fp = fp + "/db";
        if (fWipe) {
//            LogPrintf("Wiping LevelDB in %s\n", path.string());
//            leveldb::Status result = leveldb::DestroyDB(path.string(), options);
//            dbwrapper_private::HandleError(result);
            LogPrintf("Wiping DB %s\n", fp);

            std::cout << "Wiping DB %s\n" + fp + "\n";
            fs::remove(fp);
        }
//        TryCreateDirectories(path);
//        LogPrintf("Opening LevelDB in %s\n", path.string());
        TryCreateDirectories(path);
		LogPrintf("Opening DB in ", path.string());
//		std::cout << "Opening DB in " + path.string() + "\n";

		filename = fp.c_str();
    }

//    std::cout << "Opening DB in " + path.string() + "\n";

//    leveldb::Status status = leveldb::DB::Open(options, path.string(), &pdb);
//    dbwrapper_private::HandleError(status);
//    LogPrintf("Opened LevelDB successfully\n");
//
    // open existing sqlite db
	int orc = sqlite3_open_v2(filename, &psql, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL);

	// if open-existing failed, attempt to create new db
	if (orc != SQLITE_OK) {
		orc = sqlite3_open_v2(filename, &psql, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL);
		if (orc != SQLITE_OK)
			throw(dbwrapper_error("DB open failed, err " + itostr(orc)));

		need_init = true;
	}

    if (gArgs.GetBoolArg("-forcecompactdb", false)) {
//        LogPrintf("Starting database compaction of %s\n", path.string());
//        pdb->CompactRange(nullptr, nullptr);
//        LogPrintf("Finished database compaction of %s\n", path.string());

    	//TODO: do stuff here
    }

    // configure database params
	if (need_init) {
		for (unsigned int i = 0; i < ARRAYLEN(sql_db_init); i++)
			if (sqlite3_exec(psql, sql_db_configure[i], NULL, NULL, NULL) != SQLITE_OK)
				throw(dbwrapper_error("DB configuration failed, stmt " + itostr(i)));
	}

	// first time: initialize new db schema
	if (need_init) {
		for (unsigned int i = 0; i < ARRAYLEN(sql_db_init); i++)
			if (sqlite3_exec(psql, sql_db_init[i], NULL, NULL, NULL) != SQLITE_OK)
				throw(dbwrapper_error("DB one-time setup failed, stmt " + itostr(i)));
	}

	// pre-compile SQL statements
	for (unsigned sqlidx = 0; sqlidx < ARRAYLEN(sql_stmt_text); sqlidx++) {
		sqlite3_stmt *pstmt = NULL;

		int src = sqlite3_prepare_v2(psql,
							   sql_stmt_text[sqlidx],
							   strlen(sql_stmt_text[sqlidx]),
							   &pstmt, NULL);
		if (src != SQLITE_OK)
			throw(dbwrapper_error("DB compile failed, err " + itostr(src) + ": " + std::string(sql_stmt_text[sqlidx])));

		stmts.push_back(pstmt);
	}


	LogPrintf("Opened DB successfully\n");
//	std::cout << "Opened DB successfully\n";



    // The base-case obfuscation key, which is a noop.
    obfuscate_key = std::vector<unsigned char>(OBFUSCATE_KEY_NUM_BYTES, '\000');

    bool key_exists = Read(OBFUSCATE_KEY_KEY, obfuscate_key);

    if (!key_exists && obfuscate && IsEmpty()) {
        // Initialize non-degenerate obfuscation if it won't upset
        // existing, non-obfuscated data.
        std::vector<unsigned char> new_key = CreateObfuscateKey();

        // Write `new_key` so we don't obfuscate the key with itself
        Write(OBFUSCATE_KEY_KEY, new_key);
        obfuscate_key = new_key;

        LogPrintf("Wrote new obfuscate key for %s: %s\n", path.string(), HexStr(obfuscate_key));
    }

    LogPrintf("Using obfuscation key for %s: %s\n", path.string(), HexStr(obfuscate_key));
}

CDBWrapper::~CDBWrapper()
{
//    delete pdb;
//    pdb = nullptr;
//    delete options.filter_policy;
//    options.filter_policy = nullptr;
//    delete options.info_log;
//    options.info_log = nullptr;
//    delete options.block_cache;
//    options.block_cache = nullptr;
//    delete penv;
//    options.env = nullptr;



	for (unsigned int i = 0; i < stmts.size(); i++) {
		sqlite3_finalize(stmts[i]);
	}

	stmts.clear();

	sqlite3_close(psql);
	psql = NULL;
}

bool CDBWrapper::WriteBatch(CDBBatch& batch, bool fSync)
{
//    leveldb::Status status = pdb->Write(fSync ? syncoptions : writeoptions, &batch.batch);
//    dbwrapper_private::HandleError(status);
//    return true;

//	std::cout<< "batch size:" << batch.entries.size();
	// begin DB transaction
	int src = sqlite3_step(stmts[DBW_BEGIN]);
	int rrc = sqlite3_reset(stmts[DBW_BEGIN]);

	if ((src != SQLITE_DONE) || (rrc != SQLITE_OK))
		throw dbwrapper_error("DB cannot begin transaction, err " + itostr(src) + "," + itostr(rrc));

	int bytes_written = 0;
	for (unsigned int wr = 0; wr < batch.entries.size(); wr++) {
		BatchEntry& be = batch.entries[wr];
		bytes_written += sizeof(be.key);
		bytes_written += sizeof(be.value);

		sqlite3_stmt *stmt = NULL;

		// get the SQL stmt to exec
		if (be.isErase)
			stmt = stmts[DBW_DELETE];
		else
			stmt = stmts[DBW_PUT];

		// bind column 0: key
		if (sqlite3_bind_blob(stmt, 1, be.key.c_str(), be.key.size(),
							  SQLITE_TRANSIENT) != SQLITE_OK)
			throw dbwrapper_error("DB cannot bind blob key");

		// if inserting, bind column 1: value
		if (!be.isErase)
			if (sqlite3_bind_blob(stmt, 2, be.value.c_str(), be.value.size(),
								  SQLITE_TRANSIENT) != SQLITE_OK)
				throw dbwrapper_error("DB cannot bind blob value");

		// exec INSERT/DELETE
		src = sqlite3_step(stmt);
		rrc = sqlite3_reset(stmt);

		if ((src != SQLITE_DONE) || (rrc != SQLITE_OK)) {
			std::cout << itostr(src) + "," + itostr(rrc);
			throw dbwrapper_error("DB failed update, err " + itostr(src) + "," + itostr(rrc));
		}

	}

	src = sqlite3_step(stmts[DBW_COMMIT]);
	rrc = sqlite3_reset(stmts[DBW_COMMIT]);


	std::cout << "size of write batch is: " << bytes_written << "\n";
	if ((src != SQLITE_DONE) || (rrc != SQLITE_OK)) {
		std::cout << itostr(src) + "," + itostr(rrc);
		throw dbwrapper_error("DB cannot commit transaction, err" + itostr(src) + "," + itostr(rrc));
	}

	batch.Clear();

	return true;
}

// Prefixed with null character to avoid collisions with other keys
//
// We must use a string constructor which specifies length so that we copy
// past the null-terminator.
const std::string CDBWrapper::OBFUSCATE_KEY_KEY("\000obfuscate_key", 14);

const unsigned int CDBWrapper::OBFUSCATE_KEY_NUM_BYTES = 8;

/**
 * Returns a string (consisting of 8 random bytes) suitable for use as an
 * obfuscating XOR key.
 */
std::vector<unsigned char> CDBWrapper::CreateObfuscateKey() const
{
    unsigned char buff[OBFUSCATE_KEY_NUM_BYTES];
    GetRandBytes(buff, OBFUSCATE_KEY_NUM_BYTES);
    return std::vector<unsigned char>(&buff[0], &buff[OBFUSCATE_KEY_NUM_BYTES]);

}

bool CDBWrapper::IsEmpty()
{
//    std::unique_ptr<CDBIterator> it(NewIterator());
//    it->SeekToFirst();
//    return !(it->Valid());

	int src = sqlite3_step(stmts[DBW_GET_ALL_COUNT]);
	if (src != SQLITE_ROW) {
		sqlite3_reset(stmts[DBW_GET_ALL_COUNT]);
		throw dbwrapper_error("DB read count failure");
	}

	int64_t count = sqlite3_column_int64(stmts[DBW_GET_ALL_COUNT], 0);
	sqlite3_reset(stmts[DBW_GET_ALL_COUNT]);

	return (count == 0);

}


CDBIterator::~CDBIterator() {
	sqlite3_reset(pstmt);
}

//TODO: empty impl to compile: this won't be needed
bool CDBIterator::Valid() { return valid; }
void CDBIterator::SeekToFirst() {}

namespace dbwrapper_private {

//void HandleError(const leveldb::Status& status)
//{
//    if (status.ok())
//        return;
//    LogPrintf("%s\n", status.ToString());
//    if (status.IsCorruption())
//        throw dbwrapper_error("Database corrupted");
//    if (status.IsIOError())
//        throw dbwrapper_error("Database I/O error");
//    if (status.IsNotFound())
//        throw dbwrapper_error("Database entry missing");
//    throw dbwrapper_error("Unknown database error");
//}

const std::vector<unsigned char>& GetObfuscateKey(const CDBWrapper &w)
{
    return w.obfuscate_key;
}

} // namespace dbwrapper_private
