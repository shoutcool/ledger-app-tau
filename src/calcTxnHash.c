// This file contains the implementation of the calcTxnHash command. It is
// significantly more complicated than the other commands, mostly due to the
// transaction parsing.
//
// A high-level description of calcTxnHash is as follows. The user initiates
// the command on their computer by requesting the hash of a specific
// transaction. A flag in the request controls whether the resulting hash
// should be signed. The command handler then begins reading transaction data
// from the computer, in packets of up to 255 bytes at a time. The handler
// buffers this data until a full "element" in parsed. Depending on the type
// of the element, it may then be displayed to the user for comparison. Once
// all elements have been received and parsed, the final screen differs
// depending on whether a signature was requested. If so, the user is prompted
// to approve the signature; if they do, the signature is sent to the
// computer, and the app returns to the main menu. If no signature was
// requested, the transaction hash is immediately sent to the computer and
// displayed on a comparison screen. Pressing both buttons returns the user to
// the main menu.
//
// Keep this description in mind as you read through the implementation.

#include <stdint.h>
#include <stdbool.h>
#include <os.h>
#include <os_io_seproxyhal.h>
#include "blake2b.h"
#include "sia.h"
#include "sia_ux.h"
#include "tiny-json.h"
#include "string.h"

static calcTxnHashContext_t *ctx = &global.calcTxnHashContext;

// This is a helper function that prepares an element of the transaction for
// display. It stores the type of the element in labelStr, and a human-
// readable representation of the element in fullStr. As in previous screens,
// partialStr holds the visible portion of fullStr.
static void fmtTxnElem(calcTxnHashContext_t *ctx) {
	txn_state_t *txn = &ctx->txn;

	switch (txn->elemType) {

	case TXN_ELEM_CONTRACT:
		// Miner fees only have one part.
		os_memmove(ctx->labelStr, "Contract:\0", 10);
		os_memmove(ctx->fullStr, txn->contractName, sizeof(txn->contractName));
		ctx->elemLen = strlen(txn->contractName);
		os_memmove(ctx->partialStr, ctx->fullStr, 12);
		ctx->elemPart = 0;
		break;

	case TXN_ELEM_AMOUNT:
		// Miner fees only have one part.
		os_memmove(ctx->labelStr, "Amount:\0", 8);
		os_memmove(ctx->fullStr, txn->amount, sizeof(txn->amount));
		os_memmove(ctx->fullStr + 2, " TAU", 5);
		ctx->elemLen = strlen(txn->amount);
		os_memmove(ctx->partialStr, ctx->fullStr, 12);

		ctx->elemPart = 0;
		break;

	case TXN_ELEM_TO:
		// Miner fees only have one part.
		os_memmove(ctx->labelStr, "To:\0", 4);
		os_memmove(ctx->fullStr, txn->to, sizeof(txn->to));
		ctx->elemLen = strlen(txn->to);
		os_memmove(ctx->partialStr, ctx->fullStr, 12);
		ctx->elemPart = 0;
		break;
	

	default:
		// This should never happen.
		io_exchange_with_code(SW_DEVELOPER_ERR, 0);
		ui_idle();
		break;
	}

	// Regardless of what we're displaying, the displayIndex should always be
	// reset to 0, because we're displaying the beginning of fullStr.
	ctx->displayIndex = 0;
}

//*********************************************************************
// SCREEN: SiGN TRANSACTION
//*********************************************************************


static const bagl_element_t ui_calcTxnHash_sign[] = {
	UI_BACKGROUND(),
	UI_ICON_LEFT(0x01, BAGL_GLYPH_ICON_CROSS),
	UI_ICON_RIGHT(0x02, BAGL_GLYPH_ICON_CHECK),
	UI_TEXT(0x00, 0, 12, 128, "Sign this Txn"),
	UI_TEXT(0x00, 0, 26, 128, global.calcTxnHashContext.fullStr), // "with Key #123?"
};

static unsigned int ui_calcTxnHash_sign_button(unsigned int button_mask, unsigned int button_mask_counter) {
	switch (button_mask) {
	case BUTTON_EVT_RELEASED | BUTTON_LEFT: // REJECT
		io_exchange_with_code(SW_USER_REJECTED, 0);
		ui_idle();
		break;

	case BUTTON_EVT_RELEASED | BUTTON_RIGHT: // APPROVE
		/* for(int x=0; x<1020; x++)
		{
			PRINTF("%d", ctx->txn.buf[x]);
		} */

		//PRINTF("What a lovely buffer:\n %.*H \n\n", ctx->txn.buflen, ctx->txn.buf);
		//0000000000007B22636F6E7472616374223A2263757272656E6379222C2266756E6374696F6E223A227472616E73666572222C226B7761726773223A7B22616D6F756E74223A31302C22746F223A2237323863343134396136316166323031613031356238383364353732643465623263326537616533366135326332383536626565323630363130363765393863227D2C226E6F6E6365223A302C2270726F636573736F72223A2238396636376262383731333531613136323964363636373665346264393262626163623233626430363439623839303534326566393866316236363461343937222C2273656E646572223A2262383837306439313339
		
		deriveAndSign(G_io_apdu_buffer, ctx->keyIndex, ctx->txn.buf, ctx->txn.buflen);
		io_exchange_with_code(SW_OK, 64);
		ui_idle();
		break;
	}
	return 0;
}

//*********************************************************************
// SCREEN: TO
//*********************************************************************

static const bagl_element_t ui_to_compare[] = {
	UI_BACKGROUND(),
	UI_ICON_LEFT(0x01, BAGL_GLYPH_ICON_LEFT),
	UI_ICON_RIGHT(0x02, BAGL_GLYPH_ICON_RIGHT),
	UI_TEXT(0x00, 0, 12, 128, global.calcTxnHashContext.labelStr),
	UI_TEXT(0x00, 0, 26, 128, global.calcTxnHashContext.partialStr),
};


static const bagl_element_t* ui_prepro_to_compare(const bagl_element_t *element) {
	if ((element->component.userid == 1 && ctx->displayIndex == 0) ||
	    (element->component.userid == 2 && ((ctx->elemLen < 12) || (ctx->displayIndex == ctx->elemLen-12)))) {
		return NULL;
	}
	return element;
}

// This is the button handler for the comparison screen. Unlike the approval
// button handler, this handler doesn't send any data to the computer.
static unsigned int ui_to_compare_button(unsigned int button_mask, unsigned int button_mask_counter) {
	switch (button_mask) {
	case BUTTON_LEFT:
	case BUTTON_EVT_FAST | BUTTON_LEFT: // SEEK LEFT
		if (ctx->displayIndex > 0) {
			ctx->displayIndex--;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_RIGHT:
	case BUTTON_EVT_FAST | BUTTON_RIGHT: // SEEK RIGHT
		if (ctx->displayIndex < ctx->elemLen-12) {
			ctx->displayIndex++;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // PROCEED
		//ui_idle();

		os_memmove(ctx->fullStr, "with Key #", 10);
		bin2dec(ctx->fullStr+10, ctx->keyIndex);
		os_memmove(ctx->fullStr+10+(bin2dec(ctx->fullStr+10, ctx->keyIndex)), "?", 2);
		UX_DISPLAY(ui_calcTxnHash_sign, NULL);
		
		break;
	}
	return 0;
}


//*********************************************************************
// SCREEN: AMOUNT
//*********************************************************************

static const bagl_element_t ui_amount_compare[] = {
	UI_BACKGROUND(),
	UI_ICON_LEFT(0x01, BAGL_GLYPH_ICON_LEFT),
	UI_ICON_RIGHT(0x02, BAGL_GLYPH_ICON_RIGHT),
	UI_TEXT(0x00, 0, 12, 128, global.calcTxnHashContext.labelStr),
	UI_TEXT(0x00, 0, 26, 128, global.calcTxnHashContext.partialStr),
};


static const bagl_element_t* ui_prepro_amount_compare(const bagl_element_t *element) {
	if ((element->component.userid == 1 && ctx->displayIndex == 0) ||
	    (element->component.userid == 2 && ((ctx->elemLen < 12) || (ctx->displayIndex == ctx->elemLen-12)))) {
		return NULL;
	}
	return element;
}

// This is the button handler for the comparison screen. Unlike the approval
// button handler, this handler doesn't send any data to the computer.
static unsigned int ui_amount_compare_button(unsigned int button_mask, unsigned int button_mask_counter) {
	switch (button_mask) {
	case BUTTON_LEFT:
	case BUTTON_EVT_FAST | BUTTON_LEFT: // SEEK LEFT
		if (ctx->displayIndex > 0) {
			ctx->displayIndex--;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_RIGHT:
	case BUTTON_EVT_FAST | BUTTON_RIGHT: // SEEK RIGHT
		if (ctx->displayIndex < ctx->elemLen-12) {
			ctx->displayIndex++;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // PROCEED
		ctx->txn.elemType=TXN_ELEM_TO;
		fmtTxnElem(ctx);

		UX_DISPLAY(ui_to_compare, ui_prepro_to_compare);
		break;
	}
	return 0;
}


//*********************************************************************
// SCREEN: CONTRACT
//*********************************************************************


static const bagl_element_t ui_contract_compare[] = {
	UI_BACKGROUND(),
	UI_ICON_LEFT(0x01, BAGL_GLYPH_ICON_LEFT),
	UI_ICON_RIGHT(0x02, BAGL_GLYPH_ICON_RIGHT),
	UI_TEXT(0x00, 0, 12, 128, global.calcTxnHashContext.labelStr),
	UI_TEXT(0x00, 0, 26, 128, global.calcTxnHashContext.partialStr),
};


static const bagl_element_t* ui_prepro_contract_compare(const bagl_element_t *element) {
	if ((element->component.userid == 1 && ctx->displayIndex == 0) ||
	    (element->component.userid == 2 && ((ctx->elemLen < 12) || (ctx->displayIndex == ctx->elemLen-12)))) {
		return NULL;
	}
	return element;
}

// This is the button handler for the comparison screen. Unlike the approval
// button handler, this handler doesn't send any data to the computer.
static unsigned int ui_contract_compare_button(unsigned int button_mask, unsigned int button_mask_counter) {
	switch (button_mask) {
	case BUTTON_LEFT:
	case BUTTON_EVT_FAST | BUTTON_LEFT: // SEEK LEFT
		if (ctx->displayIndex > 0) {
			ctx->displayIndex--;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_RIGHT:
	case BUTTON_EVT_FAST | BUTTON_RIGHT: // SEEK RIGHT
		if (ctx->displayIndex < ctx->elemLen-12) {
			ctx->displayIndex++;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // PROCEED
		
		ctx->txn.elemType=TXN_ELEM_AMOUNT;
		fmtTxnElem(ctx);

		UX_DISPLAY(ui_amount_compare, ui_prepro_amount_compare);
		break;
	}
	return 0;
}



static const bagl_element_t ui_calcTxnHash_compare[] = {
	UI_BACKGROUND(),
	UI_ICON_LEFT(0x01, BAGL_GLYPH_ICON_LEFT),
	UI_ICON_RIGHT(0x02, BAGL_GLYPH_ICON_RIGHT),
	UI_TEXT(0x00, 0, 12, 128, "Compare Hash:"),
	UI_TEXT(0x00, 0, 26, 128, global.calcTxnHashContext.partialStr),
};

static const bagl_element_t* ui_prepro_calcTxnHash_compare(const bagl_element_t *element) {
	if ((element->component.userid == 1 && ctx->displayIndex == 0) ||
	    (element->component.userid == 2 && ctx->displayIndex == ctx->elemLen-12)) {
		return NULL;
	}
	return element;
}

static unsigned int ui_calcTxnHash_compare_button(unsigned int button_mask, unsigned int button_mask_counter) {
	switch (button_mask) {
	case BUTTON_LEFT:
	case BUTTON_EVT_FAST | BUTTON_LEFT: // SEEK LEFT
		if (ctx->displayIndex > 0) {
			ctx->displayIndex--;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_RIGHT:
	case BUTTON_EVT_FAST | BUTTON_RIGHT: // SEEK RIGHT
		if (ctx->displayIndex < ctx->elemLen-12) {
			ctx->displayIndex++;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // PROCEED
		ui_idle();
		break;
	}
	return 0;
}



static const bagl_element_t ui_calcTxnHash_elem[] = {
	UI_BACKGROUND(),
	UI_ICON_LEFT(0x01, BAGL_GLYPH_ICON_LEFT),
	UI_ICON_RIGHT(0x02, BAGL_GLYPH_ICON_RIGHT),
	UI_TEXT(0x00, 0, 12, 128, global.calcTxnHashContext.labelStr),
	UI_TEXT(0x00, 0, 26, 128, global.calcTxnHashContext.partialStr),
};

static const bagl_element_t* ui_prepro_calcTxnHash_elem(const bagl_element_t *element) {
	PRINTF("1: %d", ctx->displayIndex);
	PRINTF("2: %d", ctx->elemLen);
	if ((element->component.userid == 1 && ctx->displayIndex == 0) ||
	    (element->component.userid == 2 && ((ctx->elemLen < 12) || (ctx->displayIndex == ctx->elemLen-12)))) {
		return NULL;
	}
	return element;
	
}



static unsigned int ui_calcTxnHash_elem_button(unsigned int button_mask, unsigned int button_mask_counter) {
	//PRINTF("Button pressed");
	switch (button_mask) {
	case BUTTON_LEFT:
	case BUTTON_EVT_FAST | BUTTON_LEFT: // SEEK LEFT
		PRINTF("LEFT");
		if (ctx->displayIndex > 0) {
			ctx->displayIndex--;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_RIGHT:
	case BUTTON_EVT_FAST | BUTTON_RIGHT: // SEEK RIGHT
		PRINTF("RIGHT");
		if (ctx->displayIndex < ctx->elemLen-12) {
			ctx->displayIndex++;
		}
		os_memmove(ctx->partialStr, ctx->fullStr+ctx->displayIndex, 12);
		UX_REDISPLAY();
		break;

	case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // PROCEED
		PRINTF("CONFIRM");
		
		ui_idle();
		
		/*if (ctx->elemPart > 0) {
			// We're in the middle of displaying a multi-part element; display
			// the next part.
			fmtTxnElem(ctx);
			UX_REDISPLAY();
			break;
		}
		// Attempt to decode the next element in the transaction.
		switch (ctx->txn.decoderState) {
		case TXN_STATE_ERR:
			// The transaction is invalid.
			io_exchange_with_code(SW_INVALID_PARAM, 0);
			ui_idle();
			break;
		case TXN_STATE_PARTIAL:
			// We don't have enough data to decode the next element; send an
			// OK code to request more.
			io_exchange_with_code(SW_OK, 0);
			break;
		case TXN_STATE_READY:
			// We successively decoded one or more elements; display the first
			// part of the first element.
			ctx->elemPart = 0;
			fmtTxnElem(ctx);
			UX_REDISPLAY();
			break;
		case TXN_STATE_FINISHED:
			// We've finished decoding the transaction, and all elements have
			// been displayed.
			if (ctx->sign) {
				// If we're signing the transaction, prepare and display the
				// approval screen.
				os_memmove(ctx->fullStr, "with Key #", 10);
				os_memmove(ctx->fullStr+10+(bin2dec(ctx->fullStr+10, ctx->keyIndex)), "?", 2);
				UX_DISPLAY(ui_calcTxnHash_sign, NULL);
			} else {
				// If we're just computing the hash, send it immediately and
				// display the comparison screen
				os_memmove(G_io_apdu_buffer, ctx->txn.sigHash, 32);
				io_exchange_with_code(SW_OK, 32);
				bin2hex(ctx->fullStr, ctx->txn.sigHash, sizeof(ctx->txn.sigHash));
				os_memmove(ctx->partialStr, ctx->fullStr, 12);
				ctx->elemLen = 64;
				ctx->displayIndex = 0;
				UX_DISPLAY(ui_calcTxnHash_compare, ui_prepro_calcTxnHash_compare);
			}
			// Reset the initialization state.
			ctx->initialized = false;
			break;
		}*/
		break;
	}
	return 0;
}

// APDU parameters
#define P1_FIRST        0x00 // 1st packet of multi-packet transfer
#define P1_MORE         0x80 // nth packet of multi-packet transfer
#define P2_DISPLAY_HASH 0x00 // display transaction hash
#define P2_SIGN_HASH    0x01 // sign transaction hash

// handleCalcTxnHash reads a signature index and a transaction, calculates the
// SigHash of the transaction, and optionally signs the hash using a specified
// key. The transaction is processed in a streaming fashion and displayed
// piece-wise to the user.
void handleCalcTxnHash(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
	if ((p1 != P1_FIRST && p1 != P1_MORE) || (p2 != P2_DISPLAY_HASH && p2 != P2_SIGN_HASH)) {
		THROW(SW_INVALID_PARAM);
	}

	if (p1 == P1_FIRST) {
		// If this is the first packet of a transaction, the transaction
		// context must not already be initialized. (Otherwise, an attacker
		// could fool the user by concatenating two transactions.)
		//
		// NOTE: ctx->initialized is set to false when the Sia app loads.
		if (ctx->initialized) {
			THROW(SW_IMPROPER_INIT);
		}
		ctx->initialized = true;

		ctx->keyIndex = 0x00000000; // aktuell wird nur Key 0 unterstützt
		uint16_t sigIndex = 0x0000; // SigIndex ist auch immer 0 (was auch immer das ist)


		// If this is the first packet, it will include the key index and sig
		// index in addition to the transaction data. Use these to initialize
		// the ctx and the transaction decoder.
		////ctx->keyIndex = U4LE(dataBuffer, 0); // NOTE: ignored if !ctx->sign
		////dataBuffer += 4; dataLength -= 4;
		////uint16_t sigIndex = U2LE(dataBuffer, 0);
		//// dataBuffer += 2; dataLength -= 2;
		// The official Sia Nano S app only signs transactions on the forked
		// chain. To use the app on dissenting chains, pass false instead.
		txn_init(&ctx->txn, sigIndex);

		// Set ctx->sign according to P2.
		ctx->sign = (p2 & P2_SIGN_HASH);

		// Initialize some display-related variables.
		ctx->partialStr[12] = '\0';
		ctx->elemPart = ctx->elemLen = ctx->displayIndex = 0;
	} else {
		// If this is not P1_FIRST, the transaction must have been
		// initialized previously.
		if (!ctx->initialized) {
			THROW(SW_IMPROPER_INIT);
		}
	}

	

	// Add the new data to transaction decoder.
	PRINTF("%u\n", dataLength);
	txn_update(&ctx->txn, dataBuffer, dataLength);

	


	if (dataLength >= 255) { //TODO: Fixe grenze hier entfernen
		//Wenn noch nicht alle Daten gelesen wurde, die restlichen Daten noch anfordern
		io_exchange_with_code(SW_OK, 0);
	}else {

		if (ctx->sign) {

			//Validate
			uint8_t json[ctx->txn.buflen];

			//This line breaks the PRINTFs
			os_memmove(&json, &ctx->txn.buf, ctx->txn.buflen);

			PRINTF("mannomanno3");


			enum { MAX_FIELDS = 20 };
			json_t pool[ MAX_FIELDS ];

			char str[] = "{ \"name\": \"peter\", \"age\": 32 }";	

			json_t const* parent = json_create( &json, pool, MAX_FIELDS );

			json_t const* contractField = json_getProperty( parent, "contract" );
			if ( contractField == NULL ) {
				THROW(SW_INVALID_PARAM);
			}

			if ( json_getType( contractField ) != JSON_TEXT ) {
				THROW(SW_INVALID_PARAM);
			}


			json_t const* kwargs = json_getProperty( parent, "kwargs" );
			if ( !kwargs || JSON_OBJ != json_getType( kwargs ) ) {
				THROW(SW_INVALID_PARAM);
			}

			json_t const* to = json_getProperty( kwargs, "to" );
			if ( !to ) {
				THROW(SW_INVALID_PARAM);
			}

			json_t const* amount = json_getProperty( kwargs, "amount" );
			if ( !amount ) {
				THROW(SW_INVALID_PARAM);
			}
			


			char const* contactValue = json_getValue( contractField );
			char const* toValue = json_getValue( to );
			char const* amountValue = json_getValue( amount );


			os_memmove(ctx->txn.contractName, contactValue, strlen(contactValue));
			os_memmove(ctx->txn.amount, amountValue, strlen(amountValue));
			os_memmove(ctx->txn.to, toValue, strlen(toValue));

			

			//Show on Screen
			ctx->txn.decoderState=TXN_STATE_READY;
			//ctx->txn.valLen=strlen(nameValue);
			//os_memmove(ctx->txn.outVal, nameValue, strlen(nameValue));
			ctx->txn.sliceIndex++;
			
			ctx->elemPart = 0;
			fmtTxnElem(ctx);
			
			//UX_DISPLAY(ui_calcTxnHash_elem, ui_prepro_calcTxnHash_elem);
			UX_DISPLAY(ui_contract_compare, ui_prepro_contract_compare);
			*flags |= IO_ASYNCH_REPLY;
			


			/*


			os_memmove(ctx->fullStr, "with Key #", 10);
			bin2dec(ctx->fullStr+10, ctx->keyIndex);
			os_memmove(ctx->fullStr+10+(bin2dec(ctx->fullStr+10, ctx->keyIndex)), "?", 2);
			UX_DISPLAY(ui_calcTxnHash_sign, NULL);
			*flags |= IO_ASYNCH_REPLY;

			*/
			
			
		} else {
			os_memmove(G_io_apdu_buffer, ctx->txn.sigHash, 32);
			io_exchange_with_code(SW_OK, 32);
			bin2hex(ctx->fullStr, ctx->txn.sigHash, sizeof(ctx->txn.sigHash));
			os_memmove(ctx->partialStr, ctx->fullStr, 12);
			ctx->elemLen = 64;
			ctx->displayIndex = 0;
			UX_DISPLAY(ui_calcTxnHash_compare, ui_prepro_calcTxnHash_compare);
			// The above code does something strange: it calls io_exchange
			// directly from the command handler. You might wonder: why not
			// just prepare the APDU buffer and let sia_main call io_exchange?
			// The answer, surprisingly, is that we also need to call
			// UX_DISPLAY, and UX_DISPLAY affects io_exchange in subtle ways.
			// To understand why, we'll need to dive deep into the Nano S
			// firmware. I recommend that you don't skip this section, even
			// though it's lengthy, because it will save you a lot of
			// frustration when you go "off the beaten path" in your own app.
			//
			// Recall that the Nano S has two chips. Your app (and the Ledger
			// OS, BOLOS) runs on the Secure Element. The SE is completely
			// self-contained; it doesn't talk to the outside world at all. It
			// only talks to the other chip, the MCU. The MCU is what
			// processes button presses, renders things on screen, and
			// exchanges APDU packets with the computer. The communication
			// layer between the SE and the MCU is called SEPROXYHAL. There
			// are some nice diagrams in the "Hardware Architecture" section
			// of Ledger's docs that will help you visualize all this.
			//
			// The SEPROXYHAL protocol, like any communication protocol,
			// specifies exactly when each party is allowed to talk.
			// Communication happens in a loop: first the MCU sends an Event,
			// then the SE replies with zero or more Commands, and finally the
			// SE sends a Status to indicate that it has finished processing
			// the Event, completing one iteration:
			//
			//    Event -> Commands -> Status -> Event -> Commands -> ...
			//
			// For our purposes, an "Event" is a request APDU, and a "Command"
			// is a response APDU. (There are other types of Events and
			// Commands, such as button presses, but they aren't relevant
			// here.) As for the Status, there is a "General" Status and a
			// "Display" Status. A General Status tells the MCU to send the
			// response APDU, and a Display Status tells it to render an
			// element on the screen. Remember, it's "zero or more Commands,"
			// so it's legal to send just a Status without any Commands.
			//
			// You may have some picture of the problem now. Imagine we
			// prepare the APDU buffer, then call UX_DISPLAY, and then let
			// sia_main send the APDU with io_exchange. What happens at the
			// SEPROXYHAL layer? First, UX_DISPLAY will send a Display Status.
			// Then, io_exchange will send a Command and a General Status. But
			// no Event was processed between the two Statuses! This causes
			// SEPROXYHAL to freak out and crash, forcing you to reboot your
			// Nano S.
			//
			// So why does calling io_exchange before UX_DISPLAY fix the
			// problem? Won't we just end up sending two Statuses again? The
			// secret is that io_exchange_with_code uses the
			// IO_RETURN_AFTER_TX flag. Previously, the only thing we needed
			// to know about IO_RETURN_AFTER_TX is that it sends a response
			// APDU without waiting for the next request APDU. But it has one
			// other important property: it tells io_exchange not to send a
			// Status! So the only Status we send comes from UX_DISPLAY. This
			// preserves the ordering required by SEPROXYHAL.
			//
			// Lastly: what if we prepare the APDU buffer in the handler, but
			// with the IO_RETURN_AFTER_TX flag set? Will that work?
			// Unfortunately not. io_exchange won't send a status, but it
			// *will* send a Command containing the APDU, so we still end up
			// breaking the correct SEPROXYHAL ordering.
			//
			// Here's a list of rules that will help you debug similar issues:
			//
			// - Always preserve the order: Event -> Commands -> Status
			// - UX_DISPLAY sends a Status
			// - io_exchange sends a Command and a Status
			// - IO_RETURN_AFTER_TX makes io_exchange not send a Status
			// - IO_ASYNCH_REPLY (or tx=0) makes io_exchange not send a Command
			//
			// Okay, that second rule isn't 100% accurate. UX_DISPLAY doesn't
			// necessarily send a single Status: it sends a separate Status
			// for each element you render! The reason this works is that the
			// MCU replies to each Display Status with a Display Processed
			// Event. That means you can call UX_DISPLAY many times in a row
			// without disrupting SEPROXYHAL. Anyway, as far as we're
			// concerned, it's simpler to think of UX_DISPLAY as sending just
			// a single Status.
		}
		// Reset the initialization state.
		ctx->initialized = false;

	}
	
}

// It is not necessary to completely understand this handler to write your own
// Nano S app; much of it is Sia-specific and will not generalize to other
// apps. The important part is knowing how to structure handlers that involve
// multiple APDU exchanges. If you would like to dive deeper into how the
// handler buffers transaction data and parses elements, proceed to txn.c.
// Otherwise, this concludes the walkthrough. Feel free to fork this app and
// modify it to suit your own needs.
