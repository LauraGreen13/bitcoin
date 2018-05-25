// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbwrapper.h"

#include "fs.h"
#include "util.h"
#include "random.h"

#include <memenv.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>


#include <chrono>


bool CDBWrapper::WriteBatch(CDBBatch& batch, bool fSync)
{
//	auto start = std::chrono::high_resolution_clock::now();

//	int bytes_written = 0;
	// begin DB transaction
	int src = sqlite3_step(stmts[DBW_BEGIN]);
	int rrc = sqlite3_reset(stmts[DBW_BEGIN]);
//
//	auto stop = std::chrono::high_resolution_clock::now();
//	//duration_cast<microseconds>
//	auto duration = (stop - start);
//
//	std::cout << "1.Time taken by step&rest of DBW_BEGIN: " << duration.count() << " microseconds\n";
//	start = stop;


	if ((src != SQLITE_DONE) || (rrc != SQLITE_OK))
		throw dbwrapper_error("DB cannot begin transaction, err " + itostr(src) + "," + itostr(rrc));

	sqlite3_stmt *stmt = NULL;
//
//	std::cout << "number entries: " << batch.entries.size() << "\n\n";
//

	for (unsigned int wr = 0; wr < batch.entries.size(); wr++) {
		BatchEntry& be = batch.entries[wr];
//		std::cout << be.isErase << " key: " << be.key << ", value: " << be.value;
//		bytes_written += sizeof(be.value);

		// get the SQL stmt to exec
		if (be.isErase) {
			stmt = stmts[DBW_DELETE];
//			std::cout << "is delete\n";
		} else {
			stmt = stmts[DBW_PUT];
		}


		// bind column 0: key
		if (sqlite3_bind_blob(stmt, 1, be.key.c_str(), be.key.size(),
							  SQLITE_TRANSIENT) != SQLITE_OK) {
			throw dbwrapper_error("DB cannot bind blob key");
		}



		// if inserting, bind column 1: value
		if (!be.isErase) {
			if (sqlite3_bind_blob(stmt, 2, be.value.c_str(), be.value.size(),
											  SQLITE_TRANSIENT) != SQLITE_OK) {
				throw dbwrapper_error("DB cannot bind blob value");
			}
		}
//
//
//		stop = std::chrono::high_resolution_clock::now();
//		//duration_cast<microseconds>
//		duration = (stop - start);
//
//		std::cout << "2.Time taken by function bind_blob: " << duration.count() << " microseconds\n";
//		start = stop;

		// exec INSERT/DELETE
		sqlite3_step(stmt);
//		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);

//
//		stop = std::chrono::high_resolution_clock::now();
//		//duration_cast<microseconds>
//		duration = (stop - start);
//
//		std::cout << "3.Time taken by insert/delete in transaction: " << duration.count() << " microseconds\n\n";
//		start = stop;

		if ((src != SQLITE_DONE) || (rrc != SQLITE_OK)) {
			std::cout << itostr(src) + "," + itostr(rrc);
			throw dbwrapper_error("DB failed update, err " + itostr(src) + "," + itostr(rrc));
		}

	}
//	start = stop;
	src = sqlite3_step(stmts[DBW_COMMIT]);
	rrc = sqlite3_reset(stmts[DBW_COMMIT]);
//
//	stop = std::chrono::high_resolution_clock::now();
//	//duration_cast<microseconds>
//	duration = (stop - start);
//
//	std::cout << "4.Time taken by step&reset of DBW_COMMIT: " << duration.count() << " microseconds\n";
//

	if ((src != SQLITE_DONE) || (rrc != SQLITE_OK)) {
		std::cout << itostr(src) + "," + itostr(rrc);
		throw dbwrapper_error("DB cannot commit transaction, err" + itostr(src) + "," + itostr(rrc));
	}

//	std::cout << "size of write batch is: " << bytes_written << "\n";
	batch.Clear();

	return true;
}




CDBIterator *CDBWrapper::NewIterator()
{
//        return new CDBIterator(*this, pdb->NewIterator(iteroptions));
	unsigned int stmt_idx = DBW_SEEK_SORTED;
	return new CDBIterator(*this, stmts[stmt_idx]);
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
