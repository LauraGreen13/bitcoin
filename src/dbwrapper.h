// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_DBWRAPPER_H
#define BITCOIN_DBWRAPPER_H

#include "clientversion.h"
#include "fs.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utilstrencodings.h"
#include "version.h"

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include <sqlite3.h>


enum {
        DBW_PUT,
        DBW_GET,
        DBW_DELETE,
        DBW_BEGIN,
        DBW_COMMIT,
        DBW_GET_ALL_COUNT,
        DBW_SEEK_SORTED,
};

static const size_t DBWRAPPER_PREALLOC_KEY_SIZE = 64;
static const size_t DBWRAPPER_PREALLOC_VALUE_SIZE = 1024;

class dbwrapper_error : public std::runtime_error
{
public:
    dbwrapper_error(const std::string& msg) : std::runtime_error(msg) {
    	//TODO: handle errors somehow
    	std::cout << msg + "\n\n";

    }
};

class CDBWrapper;


static const char *sql_db_init[] = {
    "CREATE TABLE tab(k BLOB PRIMARY KEY, v BLOB) WITHOUT ROWID",
};

static const char *sql_db_configure[] = {
//    "PRAGMA page_size=4096",
//	"PRAGMA cache_size=-4000",    // max cache size; negative = # of kibibytes
//	"PRAGMA journal_mode=WAL",
//	"PRAGMA wal_autocheckpoint=10000",
//
	"PRAGMA page_size = 8192",
//	"PRAGMA cache_size= 10000",
//	"PRAGMA auto_vacuum=FULL",
	"PRAGMA journal_mode=WAL",
//	"PRAGMA journal_size_limit = 1048576"
//	"PRAGMA cache_size=400000",

//	"PRAGMA cache_size=102400",
//	"PRAGMA cache_size=204800",
//	"PRAGMA locking_mode=EXCLUSIVE",

//	"PRAGMA SQLITE_DEFAULT_CACHE_SIZE=102400",

//	"PRAGMA synchronous=OFF"
};

static const char *sql_stmt_text[] = {
    "REPLACE INTO tab(k, v) VALUES(:k, :v)",            // DBW_PUT
    "SELECT v FROM tab WHERE k = :k",                   // DBW_GET
    "DELETE FROM tab WHERE k = :k",                     // DBW_DELETE
    "BEGIN TRANSACTION",                                // DBW_BEGIN
    "COMMIT TRANSACTION",                               // DBW_COMMIT
    "SELECT COUNT(*) FROM tab",                         // DBW_GET_ALL_COUNT
//    "SELECT k, v FROM tab WHERE k >= :k ORDER BY k",    // DBW_SEEK_SORTED
};


/** These should be considered an implementation detail of the specific database.
 */
namespace dbwrapper_private {

/** Handle database error by throwing dbwrapper_error exception.
 */
//void HandleError(const leveldb::Status& status);

/** Work around circular dependency, as well as for testing in dbwrapper_tests.
 * Database obfuscation should be considered an implementation detail of the
 * specific database.
 */
const std::vector<unsigned char>& GetObfuscateKey(const CDBWrapper &w);

};


class BatchEntry {
public:
    bool           isErase;
    std::string    key;
    std::string    value;

    BatchEntry(bool isErase_, const std::string& key_,
               const std::string& value_) : isErase(isErase_), key(key_), value(value_) { }
    BatchEntry(bool isErase_, const std::string& key_) : isErase(isErase_), key(key_) { }
};

/** Batch of changes queued to be written to a CDBWrapper */
class CDBBatch
{
    friend class CDBWrapper;

private:
    const CDBWrapper &parent;
//    leveldb::WriteBatch batch;

    std::vector<BatchEntry> entries;

    CDataStream ssKey;
    CDataStream ssValue;

    size_t size_estimate;

public:
    /**
     * @param[in] _parent   CDBWrapper that this batch is to be submitted to
     */
    CDBBatch(const CDBWrapper &_parent) : parent(_parent), ssKey(SER_DISK, CLIENT_VERSION), ssValue(SER_DISK, CLIENT_VERSION), size_estimate(0) { };

    void Clear()
    {
//        batch.Clear();
//        size_estimate = 0;
    }

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
//        leveldb::Slice slKey(ssKey.data(), ssKey.size());

        std::string rawKey(&ssKey[0], ssKey.size());

        ssValue.reserve(DBWRAPPER_PREALLOC_VALUE_SIZE);
        ssValue << value;
        ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));

        std::string rawValue(&ssValue[0], ssValue.size());
//
        BatchEntry be(false, rawKey, rawValue);
        //

        for (unsigned int wr = 0; wr < entries.size(); wr++) {
        		BatchEntry& tempBE = entries[wr];
        		if (rawKey == tempBE.key) {
//        			std::cout << "It's true!\n\n\n\n";
        			//delete other with same key
        			entries.erase(entries.begin() + wr);
        		}
        }


        entries.push_back(be);
        ssKey.clear();
        ssValue.clear();
//        leveldb::Slice slValue(ssValue.data(), ssValue.size());
//
//        batch.Put(slKey, slValue);

        // LevelDB serializes writes as:
        // - byte: header
        // - varint: key length (1 byte up to 127B, 2 bytes up to 16383B, ...)
        // - byte[]: key
        // - varint: value length
        // - byte[]: value
        // The formula below assumes the key and value are both less than 16k.
//        size_estimate += 3 + (slKey.size() > 127) + slKey.size() + (slValue.size() > 127) + slValue.size();

    }

    template <typename K>
    void Erase(const K& key)
    {
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;
        std::string rawKey(&ssKey[0], ssKey.size());

        BatchEntry be(true, rawKey);

        //
        for (unsigned int wr = 0; wr < entries.size(); wr++) {
        		BatchEntry& tempBE = entries[wr];
        		if (rawKey == tempBE.key) {
//        			std::cout << "It's true!\n\n\n\n";
        			//delete other with same key
        			entries.erase(entries.begin() + wr);
        		}
        }
        //

        entries.push_back(be);

        ssKey.clear();
//        leveldb::Slice slKey(ssKey.data(), ssKey.size());

//        batch.Delete(slKey);
        // LevelDB serializes erases as:
        // - byte: header
        // - varint: key length
        // - byte[]: key
        // The formula below assumes the key is less than 16kB.
//        size_estimate += 2 + (slKey.size() > 127) + slKey.size();

    }

    size_t SizeEstimate() const { return 0; }
};


class CDBIterator;

class CDBWrapper
{
    friend const std::vector<unsigned char>& dbwrapper_private::GetObfuscateKey(const CDBWrapper &w);
public:
    //! custom environment this database is using (may be nullptr in case of default environment)
//    leveldb::Env* penv;
//
//    //! database options used
//    leveldb::Options options;
//
//    //! options used when reading from the database
//    leveldb::ReadOptions readoptions;
//
//    //! options used when iterating over values of the database
//    leveldb::ReadOptions iteroptions;
//
//    //! options used when writing to the database
//    leveldb::WriteOptions writeoptions;
//
//    //! options used when sync writing to the database
//    leveldb::WriteOptions syncoptions;
//
//    //! the database itself
//    leveldb::DB* pdb;

    //sqlite version
    //! the database itself
	sqlite3* psql;

	std::vector<sqlite3_stmt*> stmts;



    //! a key used for optional XOR-obfuscation of the database
    std::vector<unsigned char> obfuscate_key;

    //! the key under which the obfuscation key is stored
    static const std::string OBFUSCATE_KEY_KEY;

    //! the length of the obfuscate key in number of bytes
    static const unsigned int OBFUSCATE_KEY_NUM_BYTES;

    std::vector<unsigned char> CreateObfuscateKey() const;

public:
    /**
     * @param[in] path        Location in the filesystem where leveldb data will be stored.
     * @param[in] nCacheSize  Configures various leveldb cache settings.
     * @param[in] fMemory     If true, use leveldb's memory environment.
     * @param[in] fWipe       If true, remove all existing data.
     * @param[in] obfuscate   If true, store data obfuscated via simple XOR. If false, XOR
     *                        with a zero'd byte array.
     */



    CDBWrapper(const fs::path& path, size_t nCacheSize, bool fMemory = false, bool fWipe = false, bool obfuscate = false) {

    		std::string fp;
    		const char *filename = NULL;
    		bool need_init = false;

    	    if (fMemory) {
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

    	            std::cout << "Wiping DB: " + fp + "\n";
    	            fs::remove(fp);
    	        }
    	//        TryCreateDirectories(path);
    	//        LogPrintf("Opening LevelDB in %s\n", path.string());
    	        TryCreateDirectories(path);
    			LogPrintf("Opening DB in ", path.string());

    			filename = fp.c_str();
    	    }

    	//    std::cout << "Opening DB in " << filename << "\n";

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
    			for (unsigned int i = 0; i < ARRAYLEN(sql_db_configure); i++) {
    				if (sqlite3_exec(psql, sql_db_configure[i], NULL, NULL, NULL) != SQLITE_OK) {
    					throw(dbwrapper_error("DB configuration failed, stmt " + itostr(i)));
    				} else {
    					std::cout << "yey " << sql_db_configure[i] << "\n";
    				}
    			}

    		}

    		// first time: initialize new db schema
    		if (need_init) {
    			for (unsigned int i = 0; i < ARRAYLEN(sql_db_init); i++) {
    				if (sqlite3_exec(psql, sql_db_init[i], NULL, NULL, NULL) != SQLITE_OK) {
    					throw(dbwrapper_error("DB one-time setup failed, stmt " + itostr(i)));
    				}
    			}
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

    	~CDBWrapper()
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





    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;

//        std::string rawKey(&ssKey[0], ssKey.size());
//        if (sqlite3_bind_blob(pstmt, 0, rawKey.c_str(), rawKey.size(), SQLITE_TRANSIENT) != SQLITE_OK) {

        int brc = sqlite3_bind_blob(stmts[DBW_GET], 1, &ssKey[0], ssKey.size(), SQLITE_TRANSIENT);
        if (brc != SQLITE_OK) {
//        	LogPrintf("LevelDB read failure: %s\n",  itostr(brc));
        	throw dbwrapper_error("DB find-read failure, err " + itostr(brc));
//			dbwrapper_private::HandleError(status);
        }

        int src = sqlite3_step(stmts[DBW_GET]);
        if (src != SQLITE_ROW) {
			sqlite3_reset(stmts[DBW_GET]);

			if (src == SQLITE_DONE)                // success; 0 results
				return false;

			// exceptional error
			throw dbwrapper_error("DB read failure, err " + itostr(src));
		}

		// SQLITE_ROW - we have a result
		int col_size = sqlite3_column_bytes(stmts[DBW_GET], 0);
		std::string strValue((const char *) sqlite3_column_blob(stmts[DBW_GET], 0), col_size);

		sqlite3_reset(stmts[DBW_GET]);

		try {
			CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
			ssValue.Xor(obfuscate_key);
			ssValue >> value;
		} catch (const std::exception&) {
			return false;
		}
		return true;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        CDBBatch batch(*this);
        batch.Write(key, value);
        return WriteBatch(batch, fSync);
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;

        if (sqlite3_bind_blob(stmts[DBW_GET], 1, &ssKey[0], ssKey.size(),
                                      SQLITE_TRANSIENT) != SQLITE_OK)
			throw dbwrapper_error("DB find-exists failure");

		int src = sqlite3_step(stmts[DBW_GET]);
		sqlite3_reset(stmts[DBW_GET]);
		if (src == SQLITE_DONE)                // zero results; not found
			return false;
		else if (src == SQLITE_ROW)
			return true;
		else
			throw dbwrapper_error("DB read failure, err " + itostr(src));
//        leveldb::Slice slKey(ssKey.data(), ssKey.size());
//
//        std::string strValue;
//        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
//        if (!status.ok()) {
//            if (status.IsNotFound())
//                return false;
//            LogPrintf("LevelDB read failure: %s\n", status.ToString());
//            dbwrapper_private::HandleError(status);
//        }
//        return true;
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
//        CDBBatch batch(*this);
//        batch.Erase(key);
//
//        THIS IS SOOO WEIRD
//        return WriteBatch(batch, fSync);
//
//


        CDataStream ssKey(SER_DISK, CLIENT_VERSION);

    	ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
		ssKey << key;
//		std::string rawKey(&ssKey[0], ssKey.size());

        sqlite3_stmt *stmt = stmts[DBW_DELETE];
    	int src = sqlite3_step(stmt);
    	if (src != SQLITE_ROW) {
    		sqlite3_reset(stmt);
    		throw dbwrapper_error("DB  failure");
    	}


		if (sqlite3_bind_blob(stmt, 1, &ssKey[0], ssKey.size(),
							  SQLITE_TRANSIENT) != SQLITE_OK) {
			throw dbwrapper_error("DB cannot bind blob key");
		}

		sqlite3_step(stmt);
//		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);


		return true;
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false);

    // not available for LevelDB; provide for compatibility with BDB
//    bool Flush()
//    {
//        return true;
//    }


    CDBIterator *NewIterator();
//    {
////        return new CDBIterator(*this, pdb->NewIterator(iteroptions));
//    	unsigned int stmt_idx = DBW_SEEK_SORTED;
//		return new CDBIterator(*this, stmts[stmt_idx]);
//    }

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty();

    template<typename K>
    size_t EstimateSize(const K& key_begin, const K& key_end) const
    {

    }

    /**
     * Compact a certain range of keys in the database.
     */
    template<typename K>
    void CompactRange(const K& key_begin, const K& key_end) const
    {

    }

};


class CDBIterator
{
private:
    const CDBWrapper &parent;
//    leveldb::Iterator *piter;

	sqlite3_stmt *pstmt;
	bool valid;

public:

    /**
     * @param[in] _parent          Parent CDBWrapper instance.
     * @param[in] _piter           The original leveldb iterator.
     */
    CDBIterator(const CDBWrapper &_parent, sqlite3_stmt *pstmt_) :
    	parent(_parent), pstmt(pstmt_), valid(false) { };
    ~CDBIterator();

    bool Valid();

    void SeekToFirst();

    template<typename K> void Seek(const K& key) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(DBWRAPPER_PREALLOC_KEY_SIZE);
        ssKey << key;

        valid = false;

        pstmt = parent.stmts[DBW_GET];

        int rrc = sqlite3_reset(pstmt);
        if (rrc != SQLITE_OK) {
        	throw dbwrapper_error("DB reset failure");
        }

        std::string rawKey(&ssKey[0], ssKey.size());

        //        sqlite3_bind_blob(stmts[DBW_GET], 1, &ssKey[0], ssKey.size(), SQLITE_TRANSIENT);
        if (sqlite3_bind_blob(pstmt, 1, rawKey.c_str(), rawKey.size(), SQLITE_TRANSIENT) != SQLITE_OK) {
        	throw dbwrapper_error("DB find failure");
        }

         int src = sqlite3_step(pstmt);
         if (src == SQLITE_ROW)
         	valid = true;

    }

    void Next() {
            if (!valid)
                return;

            int src = sqlite3_step(pstmt);
            if (src == SQLITE_ROW)
                valid = true;
            else
                valid = false;
        }

    template<typename K> bool GetKey(K& key) {
//        leveldb::Slice slKey = piter->key();
//        try {
//            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
//            ssKey >> key;
//        } catch (const std::exception&) {
//            return false;
//        }
//        return true;

    	if (!valid) {
    		return false;
    	}

		std::string strCol((const char *) sqlite3_column_blob(pstmt, 0),
						   (size_t) sqlite3_column_bytes(pstmt, 0));
		try {
			CDataStream ssKey(&strCol[0], &strCol[0] + strCol.size(), SER_DISK, CLIENT_VERSION);
			ssKey >> key;
		} catch(std::exception &e) {
			return false;
		}
		return true;

    }

    template<typename V> bool GetValue(V& value) {
//        leveldb::Slice slValue = piter->value();

    	if (!valid) {
			return false;
		}

    	std::string strCol((const char *) sqlite3_column_blob(pstmt, 1),
    							   (size_t) sqlite3_column_bytes(pstmt, 1));

		try {
			CDataStream ssValue(&strCol[0], &strCol[0] + strCol.size(), SER_DISK, CLIENT_VERSION);
			ssValue.Xor(dbwrapper_private::GetObfuscateKey(parent));
			ssValue >> value;
		} catch (const std::exception&) {
			return false;
		}




        return true;
    }

    unsigned int GetValueSize() {
    	if (!valid) {
    		return 0;
    	}

		return sqlite3_column_bytes(pstmt, 1);
//        return piter->value().size();
    }

};






#endif // BITCOIN_DBWRAPPER_H
