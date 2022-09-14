/*
	Copyright (C) 2004-2005 Cory Nelson

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgment in the product documentation would be
		appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
	
	CVS Info :
		$Author: jaysha $
		$Date: 2010/05/25 07:43:12 $
		$Revision: 1.6 $
*/

#include <sqlite3.h>
#include "sqlite3x.hpp"

namespace sqlite3x {

	sqlite3_transaction::sqlite3txn_behavior_t const sqlite3_transaction::sqlite3txn_deferred  = 0;
	sqlite3_transaction::sqlite3txn_behavior_t const sqlite3_transaction::sqlite3txn_immediate = 1;
	sqlite3_transaction::sqlite3txn_behavior_t const sqlite3_transaction::sqlite3txn_exclusive = 2;


sqlite3_transaction::sqlite3_transaction(sqlite3_connection &con, bool start, sqlite3txn_behavior_t txn_behavior) : con(con),intrans(false),txnbehavior(txn_behavior) {
	if(start) begin();
}

sqlite3_transaction::~sqlite3_transaction() {
	if(intrans) {
		try {
			rollback();
		}
		catch(...) {
			return;
		}
	}
}

void sqlite3_transaction::begin() {
	if(txnbehavior == sqlite3txn_deferred)
		begin_deferred();
	else if(txnbehavior == sqlite3txn_immediate)
		begin_immediate();
	else if(txnbehavior == sqlite3txn_exclusive)
		begin_exclusive();
	else
		begin_deferred();


}

void sqlite3_transaction::begin_deferred() {
	con.executenonquery("begin;");
	intrans=true;
}

void sqlite3_transaction::begin_immediate() {
	con.executenonquery("begin immediate;");
	intrans=true;
}

void sqlite3_transaction::begin_exclusive() {
	con.executenonquery("begin exclusive;");
	intrans=true;
}

void sqlite3_transaction::commit() {
	con.executenonquery("commit;");
	intrans=false;
}

void sqlite3_transaction::rollback() {
	con.executenonquery("rollback;");
	intrans=false;
}

}
