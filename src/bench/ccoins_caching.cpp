// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"
#include "coins.h"
#include "policy/policy.h"
#include "validation.h"
#include "wallet/crypter.h"
#include "txdb.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "txmempool.h"
#include "txdb.h"
#include "init.h"
#include "chainparams.h"
#include "dbwrapper.h"

#include <vector>


#include <boost/thread.hpp>
#include "scheduler.h"
//#include <thread>
#include "core_io.h"



static boost::thread_group threadGroup;
static CScheduler scheduler;


class CCoinsViewErrorCatcher : public CCoinsViewBacked
{
public:
    CCoinsViewErrorCatcher(CCoinsView* view) : CCoinsViewBacked(view) {}
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override {
        try {
            return CCoinsViewBacked::GetCoin(outpoint, coin);
        } catch(const std::runtime_error& e) {
//            uiInterface.ThreadSafeMessageBox(_("Error reading from database, shutting down."), "", CClientUIInterface::MSG_ERROR);
            LogPrintf("Error reading from database: %s\n", e.what());
            // Starting the shutdown sequence and returning false to the caller would be
            // interpreted as 'entry not found' (as opposed to unable to read data), and
            // could lead to invalid interpretation. Just exit immediately, as we can't
            // continue anyway, and all writes should be atomic.
            abort();
        }
    }
    // Writes do not need similar protection, as failure to write is handled by the caller.
};




// FIXME: Dedup with SetupDummyInputs in test/transaction_tests.cpp.
//
// Helper: create two dummy transactions, each with
// two outputs.  The first has 11 and 50 CENT outputs
// paid to a TX_PUBKEY, the second 21 and 22 CENT outputs
// paid to a TX_PUBKEYHASH.
//
static std::vector<CMutableTransaction>
SetupDummyInputs(CBasicKeyStore& keystoreRet, CCoinsViewCache& coinsRet, int txNo, int outputsPerTx)
{
    std::vector<CMutableTransaction> dummyTransactions;


    // Add some keys to the keystore:
//    int outputsPerTx = 100;
//	int txNo = 10;
	int keyNo = txNo*outputsPerTx;

    dummyTransactions.resize(txNo);
    CKey key[keyNo];

    for (int i = 0; i < keyNo; i++) {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    int j = 0;
    //int k = 0;
    for (int i = 0; i < txNo; i++) {

    	dummyTransactions[i].vout.resize(outputsPerTx);
    	for (int k = 0; k < outputsPerTx; ++k) {

			dummyTransactions[i].vout[k].nValue = 11 * CENT;
			dummyTransactions[i].vout[k].scriptPubKey << ToByteVector(key[j].GetPubKey()) << OP_CHECKSIG;
			j++;
		}
    	AddCoins(coinsRet, dummyTransactions[i], 0);
    }
    return dummyTransactions;
}

// Microbenchmark for simple accesses to a CCoinsViewCache database. Note from
// laanwj, "replicating the actual usage patterns of the client is hard though,
// many times micro-benchmarks of the database showed completely different
// characteristics than e.g. reindex timings. But that's not a requirement of
// every benchmark."
// (https://github.com/bitcoin/bitcoin/issues/7883#issuecomment-224807484)
static void CCoinsCaching(benchmark::State& state)
{

	// Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
	try {
		SelectParams(ChainNameFromCommandLine());
	} catch (const std::exception& e) {
		std::cout<< "Error: %s\n"<< e.what();
//		return false;
	}

	AppInitMain(threadGroup, scheduler);

	if (pcoinsTip != nullptr) {
		std::cout << "db view size: " << pcoinsdbview->EstimateSize() << "\n\n\n\n\n";

		int cnt = 0;
		std::unique_ptr<CDBIterator> it(new CDBIterator(pcoinsdbview->db, pcoinsdbview->db.pdb->NewIterator(pcoinsdbview->db.iteroptions)));
		for (it->SeekToFirst(); it->Valid(); it->Next()) {
			cnt++;
		}

		std::cout << "number of entries in database: " << cnt << "\n\n\n\n\n";
		std::cout << "best block is null?: " << pcoinsdbview->GetBestBlock().IsNull() << "\n\n\n\n\n";
		std::cout << "cache size: " << pcoinsTip->GetCacheSize() << "\n\n\n\n\n";
	} else {
		std::cout << "SHIT\n\n\n\n";
	}

	CBasicKeyStore keystore;

	int txNo = 2;
	int maxUtxo = 500;

	//
	int utxoNo = 7;
	int maxTx = 100;

	std::cout << "number of transactions: " << txNo << "\n";

	for (int k = 20; k < maxUtxo; k+=10) {
		std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, *pcoinsTip, txNo, k);

		std::cout << "number of utxo per transaction: " << k << "\n";
		int dummySize = dummyTransactions.size();
		std::vector<CMutableTransaction> transactions;
		transactions.resize(dummySize);

		//for each dummy transaction create a transaction that takes the coins from the dummy tx i and i-1 and
		//puts them in 3 different outputs
		for (int i = 1; i < dummySize; i++) {

			//uncache the coins that are about to be used
//			for (int var = 0; var < dummyTransactions[i].vin.size(); var++) {
//				pcoinsTip->Uncache(dummyTransactions[i].vin[var].prevout);
//				std::cout << "uncached\n";;
////				pcoinsTip->Uncache(dummyTransactions[i-1].vin[var].prevout);
//			}
//
//			std::cout << "have coin for tx i: " << pcoinsTip->HaveCoin(dummyTransactions[i].vin[0].prevout) << "\n";
//			std::cout << "have coin for tx i-1: " << pcoinsTip->HaveCoin(dummyTransactions[i-1].vin[0].prevout) << "\n";


//			int i = 1;
			CMutableTransaction t1;
			t1.vin.resize(3);
			t1.vin[0].prevout.hash = dummyTransactions[i-1].GetHash();
			t1.vin[0].prevout.n = 1;
			t1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);

			t1.vin[1].prevout.hash = dummyTransactions[i].GetHash();
			t1.vin[1].prevout.n = 0;
			t1.vin[1].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);

			t1.vin[2].prevout.hash = dummyTransactions[i-1].GetHash();
			t1.vin[2].prevout.n = 1;
			t1.vin[2].scriptSig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);

			t1.vout.resize(2);
			t1.vout[0].nValue = 90 * CENT;
			t1.vout[0].scriptPubKey << OP_1;

			transactions[i] = t1;

//		}

		//write everything to database to bypass the cache
		//Uncomment this to benchmark the access to database
		pcoinsTip->Flush();

		// Benchmark.
//		int i = 1;
//		CMutableTransaction t1;
		bool isInCache = 1;

		while (state.KeepRunning()) {


//			for (int var = 0; var < t1.vin.size(); ++var) {
//				bool has = pcoinsTip->HaveCoinInCache(t1.vin[var].prevout);
//				assert(has == 1);
//			}

			CAmount value = pcoinsTip->GetValueIn(t1);

		}
		}
//		std::cout << " was in cache: " << isInCache << "\n";

		//need to clear cache
//			pcoinsTip->Flush();
	}
}

BENCHMARK(CCoinsCaching);
