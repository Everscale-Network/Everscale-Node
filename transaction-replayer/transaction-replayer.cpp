/* 
    This file is part of TON Blockchain source code.

    TON Blockchain is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    TON Blockchain is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TON Blockchain.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give permission 
    to link the code of portions of this program with the OpenSSL library. 
    You must obey the GNU General Public License in all respects for all 
    of the code used other than OpenSSL. If you modify file(s) with this 
    exception, you may extend this exception to your version of the file(s), 
    but you are not obligated to do so. If you do not wish to do so, delete this 
    exception statement from your version. If you delete this exception statement 
    from all source files in the program, then also delete it here.

    Copyright 2017-2020 Telegram Systems LLP
*/
#include "transaction-replayer-lib/transaction-replayer-lib.h"

#include "block/transaction.h"
#include "block-auto.h"
#include "block-parse.h"
#include "crypto/openssl/rand.hpp"

#include "crypto/vm/cp0.h"

#include "td/utils/filesystem.h"
#include "td/utils/port/signals.h"
#include "td/utils/port/rlimit.h"
#include "td/utils/check.h"

#define CHECK(v, ...) \
  if (!(v)) { \
    ::td::detail::process_check_error(#v, __FILE__, __LINE__); \
  }

static td::Ref<vm::Cell> load_file_as_cell(const char *path) {
  auto data = td::read_file_str(td::CSlice(path)).move_as_ok();
  return vm::std_boc_deserialize(td::Slice{data.data(), data.length()}).move_as_ok();
}

static bool write_cell_to_file(td::Ref<vm::Cell> root, const char *path) {
  auto data = vm::std_boc_serialize(std::move(root)).move_as_ok();
  LOG_STATUS(td::write_file(td::CSlice(path), std::move(data)));
  return true;
}

int replay_transaction(const char *acc, const char *cfg, const char *tx, const char *acc_new, const char *tx_new) {
  auto acc_root = load_file_as_cell(acc);
  auto cfg_root = load_file_as_cell(cfg);
  auto tr_root = load_file_as_cell(tx);

  block::gen::Transaction::Record good_trans;
  CHECK(tlb::unpack_cell(std::move(tr_root), good_trans), "cannot unpack ethalon transaction");
  auto msg_root = good_trans.r1.in_msg->prefetch_ref();
  std::pair<td::Ref<vm::Cell>, td::Ref<vm::Cell>> result;
  if (msg_root.is_null()) {
    result = replay_ticktock_transaction(std::move(acc_root), std::move(cfg_root), good_trans.lt, good_trans.now, good_trans.prev_trans_lt, good_trans.prev_trans_hash, true);
  } else {
    result = replay_ordinary_transaction(std::move(acc_root), std::move(msg_root), std::move(cfg_root), good_trans.lt, good_trans.now, good_trans.prev_trans_lt, good_trans.prev_trans_hash, false);
  }
  
  if (result.first.get()) {
    CHECK(write_cell_to_file(result.first, tx_new));
  }
  if (result.second.get()) {
    CHECK(write_cell_to_file(result.second, acc_new));
  }
  return 0;
}

int main(int argc, char *argv[]) {
  SET_VERBOSITY_LEVEL(verbosity_INFO);

  td::set_default_failure_signal_handler().ensure();

  SCOPE_EXIT {
    td::log_interface = td::default_log_interface;
  };

  LOG_STATUS(td::change_maximize_rlimit(td::RlimitType::nofile, 65536));

  CHECK(vm::init_op_cp0(false))

  if (argc != 6) {
    std::cout << "Run format\ntransaction-replayer acc_path transaction_path config_path result_acc_path result_transaction_path" << std::endl;
    return -2;
  }
  
  auto acc = argv[1];
  auto tx = argv[2];
  auto cfg = argv[3];
  auto acc_new = argv[4];
  auto tx_new = argv[5];
  return replay_transaction(acc, cfg, tx, acc_new, tx_new);
}
