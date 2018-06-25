// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"
#include "coins.h"
#include "policy/policy.h"
#include "validation.h"
#include "wallet/crypter.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "txmempool.h"
#include "txdb.h"
//#include "init.h"
#include "chainparams.h"
#include "dbwrapper.h"

#include <vector>


#include <boost/thread.hpp>
#include "scheduler.h"
//#include <thread>
#include "core_io.h"


#include "consensus/consensus.h"
#include "crypto/sha256.h"
#include "fs.h"
#include "key.h"
#include "miner.h"
#include "net_processing.h"
#include "pubkey.h"
#include "random.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "script/sigcache.h"
#include <memory>



#include <chrono>
#include <algorithm>
#include <iostream>
//static boost::thread_group threadGroup;
//static CScheduler scheduler;
//
#include "perf.h"

//meminfo
#include "time.h"
#include "sys/types.h"
#include "sys/sysinfo.h"

struct sysinfo memInfo;



uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

static double gettimedouble(void) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_usec * 0.000001 + tv.tv_sec;
}


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


	//init from before!!!!!!!!!!!!!
	// Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
//	try {
//		SelectParams(ChainNameFromCommandLine());
//	} catch (const std::exception& e) {
//		std::cout<< "Error: %s\n"<< e.what();
//	}
//
	//	AppInitMain(threadGroup, scheduler);
//	!!!!!!!!!!!!!!!!!!!!!!

	//
    CCoinsViewDB *pcoinsdbview;
    fs::path pathTemp;
    boost::thread_group threadGroup;
    CConnman* connman;
    CScheduler scheduler;
    std::unique_ptr<PeerLogicValidation> peerLogic;

    SetupNetworking();
    InitSignatureCache();
    InitScriptExecutionCache();
    fCheckBlockIndex = true;
    SelectParams(ChainNameFromCommandLine());

    LogPrintf("Default data directory %s\n", GetDefaultDataDir().string());
	LogPrintf("Using data directory %s\n", GetDataDir().string());

    const CChainParams& chainparams = Params();

	// Note that because we don't bother running a scheduler thread here,
	// callbacks via CValidationInterface are unreliable, but that's OK,
	// our unit tests aren't testing multiple parts of the code at once.
	GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);


	int64_t nTotalCache = (gArgs.GetArg("-dbcache", nDefaultDbCache) << 20);
	nTotalCache = std::max(nTotalCache, nMinDbCache << 20); // total cache cannot be less than nMinDbCache
	nTotalCache = std::min(nTotalCache, nMaxDbCache << 20); // total cache cannot be greater than nMaxDbcache
	int64_t nBlockTreeDBCache = nTotalCache / 8;
	nBlockTreeDBCache = std::min(nBlockTreeDBCache, (gArgs.GetBoolArg("-txindex", DEFAULT_TXINDEX) ? nMaxBlockDBAndTxIndexCache : nMaxBlockDBCache) << 20);
	nTotalCache -= nBlockTreeDBCache;
	int64_t nCoinDBCache = std::min(nTotalCache / 2, (nTotalCache / 4) + (1 << 23)); // use 25%-50% of the remainder for disk cache
	nCoinDBCache = std::min(nCoinDBCache, nMaxCoinsDBCache << 20); // cap total coins db cache
	nTotalCache -= nCoinDBCache;
	nCoinCacheUsage = nTotalCache;


	bool fReset =false;
	mempool.setSanityCheck(1.0);

	pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReset);
	pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReset);

	if (!LoadGenesisBlock(chainparams)) {
		throw std::runtime_error("LoadGenesisBlock failed.");
	}

	std::cout << "After LoadGenesisBlock\n\n";


	if (!ReplayBlocks(chainparams, pcoinsdbview)) {
		std::cout << "Unable to replay blocks. You will need to rebuild the database using -reindex-chainstate.\n\n";
	}

	std::cout << "After ReplayBlocks\n\n";

	pcoinsTip = new CCoinsViewCache(pcoinsdbview);

//	bool is_coinsview_empty = fReset || pcoinsTip->GetBestBlock().IsNull();
//	if (!is_coinsview_empty) {
	// LoadChainTip sets chainActive based on pcoinsTip's best block
	if (!LoadChainTip(chainparams)) {
//					strLoadError = _("Error initializing block database");
//					break;
		std::cout << "Error initializing block database\n\n";
	}
	assert(chainActive.Tip() != nullptr);

	nScriptCheckThreads = 3;
	for (int i=0; i < nScriptCheckThreads-1; i++)
		threadGroup.create_thread(&ThreadScriptCheck);
	g_connman = std::unique_ptr<CConnman>(new CConnman(0x1337, 0x1337)); // Deterministic randomness for tests.
	connman = g_connman.get();
	peerLogic.reset(new PeerLogicValidation(connman, scheduler));


//++++++++++++++++++++++++++++++INIT WAS DONE



	CBasicKeyStore keystore;

	int testCache = gArgs.GetArg("-test_cache", 0);

	int txNo = gArgs.GetArg("-tx_no", 2);
	int maxUtxo = gArgs.GetArg("-utxo_no", 2);

	std::cout << "number of transactions: " << txNo << "\n";

	int k = maxUtxo;
	std::vector<CMutableTransaction> dummyTransactions = SetupDummyInputs(keystore, *pcoinsTip, txNo, k);

	std::cout << "utxo_no per transaction: " << k << "\n";
	std::cout << "tx_no: " << txNo << "\n";

	int dummySize = dummyTransactions.size();
//	std::cout << "tx_no: " << dummySize << "\n";
	std::vector<CMutableTransaction> transactions;
	transactions.resize(dummySize);

	//for each dummy transaction create a transaction that takes the coins from the dummy tx i and i-1 and
	//puts them in 3 different outputs
	for (int i = 1; i < dummySize; i++) {
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
	}

//	============================= ACTUAL TEST===========================================================


	if (testCache == 0) {
		pcoinsTip->Flush();

	}

	//check everything is NOT flushed to database
	for (int j = 0; j < dummySize; j++) {
//		std::cout<< "j = " << j;
		CMutableTransaction temp = transactions[j];

//		std::cout<< "temp vin size: " << temp.vin.size() << "\n";
		for (int var = 0; var < temp.vin.size(); var++) {
//			std::cout<< "var = " << var << "\n";
			bool has = pcoinsdbview->HaveCoin(temp.vin[var].prevout);

			if (testCache == 0) {
				//if we don't test the cache, the coin should be in the database
				assert(has == 1);
			} else {
				assert(has == 0);
			}

		}
	}




	// Benchmark.
//	auto start = std::chrono::high_resolution_clock::now();
	while (state.KeepRunning()) {


	if (testCache == 0) {

		//access coins in db
		for (int j = 1; j < dummySize; j++) {
			CMutableTransaction temp = transactions[j];
			for (int var = 0; var < temp.vin.size(); var++) {
//				bool has = pcoinsdbview->HaveCoin(temp.vin[var].prevout);
				bool has = pcoinsTip->HaveCoin(temp.vin[var].prevout);

				assert(has == 1);

			}


//
//			CMutableTransaction tx = transactions[j];
//
//
//
//					for (const CTxIn& txin : tx.vin) {
//			//			indexed_transaction_set::const_iterator it2 = mapTx.find(txin.prevout.hash);
//			//			if (it2 != mapTx.end())
//			//				continue;
//						Coin coin;// = pcoinsTip->AccessCoin(txin.prevout);
//
//						bool has = pcoinsdbview->GetCoin(txin.prevout, coin);
//
//						assert(has == 1);
//
//
//					}


		}

	} else {

		//access coins in cache
		for (int j = 1; j < dummySize; j++) {
			CMutableTransaction temp = transactions[j];
			for (int var = 0; var < temp.vin.size(); var++) {
				bool has = pcoinsTip->HaveCoinInCache(temp.vin[var].prevout);

				assert(has == 1);

			}
		}

	}



	}

//	auto stop = std::chrono::high_resolution_clock::now();
	//duration_cast<microseconds>
//	auto duration = (stop - start);

//	std::cout << "0.Time taken by function: " << duration.count() << " microseconds\n\n";

	//check everything is flushed to database
	for (int j = 1; j < dummySize; j++) {
		CMutableTransaction temp = transactions[j];
		for (int var = 0; var < temp.vin.size(); var++) {
			bool has = pcoinsdbview->HaveCoin(temp.vin[var].prevout);

			if (testCache == 1) {
				assert(has == 0);
			} else {
				assert(has == 1);
			}

		}
	}



    threadGroup.interrupt_all();
    threadGroup.join_all();
    GetMainSignals().FlushBackgroundCallbacks();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
    g_connman.reset();
    peerLogic.reset();
    UnloadBlockIndex();
    delete pcoinsTip;
    delete pcoinsdbview;
    delete pblocktree;
//    fs::remove_all(pathTemp);

}

BENCHMARK(CCoinsCaching);
