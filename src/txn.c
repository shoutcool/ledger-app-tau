#include <stdbool.h>
#include <stdint.h>
#include <os.h>
#include "tau.h"

static void divWW10(uint64_t u1, uint64_t u0, uint64_t *q, uint64_t *r) {
	const uint64_t s = 60ULL;
	const uint64_t v = 11529215046068469760ULL;
	const uint64_t vn1 = 2684354560ULL;
	const uint64_t _B2 = 4294967296ULL;
	uint64_t un32 = u1<<s | u0>>(64-s);
	uint64_t un10 = u0 << s;
	uint64_t un1 = un10 >> 32;
	uint64_t un0 = un10 & (_B2-1);
	uint64_t q1 = un32 / vn1;
	uint64_t rhat = un32 - q1*vn1;

	while (q1 >= _B2) {
		q1--;
		rhat += vn1;
		if (rhat >= _B2) {
			break;
		}
	}

	uint64_t un21 = un32*_B2 + un1 - q1*v;
	uint64_t q0 = un21 / vn1;
	rhat = un21 - q0*vn1;

	while (q0 >= _B2) {
		q0--;
		rhat += vn1;
		if (rhat >= _B2) {
			break;
		}
	}

	*q = q1*_B2 + q0;
	*r = (un21*_B2 + un0 - q0*v) >> s;
}

static uint64_t quorem10(uint64_t nat[], int len) {
	uint64_t r = 0;
	for (int i = len - 1; i >= 0; i--) {
		divWW10(r, nat[i], &nat[i], &r);
	}
	return r;
}

static void need_at_least(txn_state_t *txn, uint64_t n) {
	if ((txn->buflen - txn->pos) < n) {
		THROW(TXN_STATE_PARTIAL);
	}
}

static void seek(txn_state_t *txn, uint64_t n) {
	need_at_least(txn, n);
	txn->pos += n;
}
/* 
static void advance(txn_state_t *txn) {
	// if elem is covered, add it to the hash
	if (txn->elemType != TXN_ELEM_TXN_SIG) {
		blake2b_update(&txn->blake, txn->buf, txn->pos);
	} else if (txn->sliceIndex == txn->sigIndex && txn->pos >= 48) {
		// add just the ParentID, Timelock, and PublicKeyIndex
		blake2b_update(&txn->blake, txn->buf, 48);
	}

	txn->buflen -= txn->pos;
	os_memmove(txn->buf, txn->buf+txn->pos, txn->buflen);
	txn->pos = 0;
} */



// throws txnDecoderState_e
/* static void __txn_next_elem(txn_state_t *txn) {
	// if we're on a slice boundary, read the next length prefix and bump the
	// element type
	while (txn->sliceIndex == txn->sliceLen) {
		txn->sliceLen = readInt(txn);
		txn->sliceIndex = 0;
		txn->elemType++;
		advance(txn);
	}

	switch (txn->elemType) {
	// these elements should be displayed
	case TXN_ELEM_SC_OUTPUT:
		readCurrency(txn, txn->outVal); // Value
		readHash(txn, txn->outAddr);    // UnlockHash
		advance(txn);
		txn->sliceIndex++;
		THROW(TXN_STATE_READY);

	case TXN_ELEM_SF_OUTPUT:
		readCurrency(txn, txn->outVal); // Value
		readHash(txn, txn->outAddr);    // UnlockHash
		readCurrency(txn, NULL);        // ClaimStart
		advance(txn);
		txn->sliceIndex++;
		THROW(TXN_STATE_READY);

	case TXN_ELEM_MINER_FEE:
		readCurrency(txn, txn->outVal); // Value
		os_memmove(txn->outAddr, "[Miner Fee]", 12);
		advance(txn);
		txn->sliceIndex++;
		THROW(TXN_STATE_READY);

	// these elements should be decoded, but not displayed
	case TXN_ELEM_SC_INPUT:
		readHash(txn, NULL);       // ParentID
		readUnlockConditions(txn); // UnlockConditions
		if (txn->asicChain) {
			addReplayProtection(&txn->blake);
		}
		advance(txn);
		txn->sliceIndex++;
		return;

	case TXN_ELEM_SF_INPUT:
		readHash(txn, NULL);       // ParentID
		readUnlockConditions(txn); // UnlockConditions
		readHash(txn, NULL);       // ClaimUnlockHash
		if (txn->asicChain) {
			addReplayProtection(&txn->blake);
		}
		advance(txn);
		txn->sliceIndex++;
		return;

	case TXN_ELEM_TXN_SIG:
		readHash(txn, NULL);    // ParentID
		readInt(txn);           // PublicKeyIndex
		readInt(txn);           // Timelock
		readCoveredFields(txn); // CoveredFields
		readPrefixedBytes(txn); // Signature
		advance(txn);
		txn->sliceIndex++;
		return;

	// these elements should not be present
	case TXN_ELEM_FC:
	case TXN_ELEM_FCR:
	case TXN_ELEM_SP:
	case TXN_ELEM_ARB_DATA:
		if (txn->sliceLen != 0) {
			PRINTF("6");
			THROW(TXN_STATE_ERR);
		}
		return;
	}
} */


/* txnDecoderState_e txn_next_elem(txn_state_t *txn) {
	// Like many transaction decoders, we use exceptions to jump out of deep
	// call stacks when we encounter an error. There are two important rules
	// for Ledger exceptions: declare modified variables as volatile, and do
	// not THROW(0). Presumably, 0 is the sentinel value for "no exception
	// thrown." So be very careful when throwing enums, since enums start at 0
	// by default.
	volatile txnDecoderState_e result;
	BEGIN_TRY {
		TRY {
			// read until we reach a displayable element or the end of the buffer
			for (;;) {
				__txn_next_elem(txn);
			}
		}
		CATCH_OTHER(e) {
			result = e;
		}
		FINALLY {
		}
	}
	END_TRY;
	if (txn->buflen + 255 > sizeof(txn->buf)) {
		// we filled the buffer to max capacity, but there still wasn't enough
		// to decode a full element. This generally means that the txn is
		// corrupt in some way, since elements shouldn't be very large.
		//PRINTF('\nBuflen %s max: %s', txn->buflen, sizeof(txn->buf));
		//PRINTF("\n77 %hu  max %lu", txn->buflen,  sizeof(txn->buf));
		PRINTF("7");
		return TXN_STATE_ERR;
	}
	return result;
} */

void txn_init(txn_state_t *txn, uint16_t sigIndex) {
	os_memset(txn, 0, sizeof(txn_state_t));
	txn->buflen = txn->pos = txn->sliceIndex = txn->sliceLen = txn->valLen = 0;
	txn->sigIndex = sigIndex;
	//PRINTF('\nBuflen: %s\n', txn->buflen);
}

void txn_update(txn_state_t *txn, uint8_t *in, uint8_t inlen) {
	// the buffer should never overflow; any elements should always be drained
	// before the next read.
	if (txn->buflen + inlen > sizeof(txn->buf)) {
		THROW(SW_TXLENGTH_ERR);
	}

	// append to the buffer
	os_memmove(txn->buf + txn->buflen, in, inlen);
	txn->buflen += inlen;

	//PRINTF('\nBuflen New: %s\n', txn->buflen);


	// reset the seek position; if we previously threw TXN_STATE_PARTIAL, now
	// we can try decoding again from the beginning.
	txn->pos = 0;
}
