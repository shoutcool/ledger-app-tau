#include <stdbool.h>
#include <stdint.h>
#include <os.h>
#include "tau.h"

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
