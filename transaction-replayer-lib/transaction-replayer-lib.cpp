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
#include "block/transaction.h"
#include "block-auto.h"
#include "block-parse.h"
#include "crypto/openssl/rand.hpp"

#include "td/utils/check.h"

#define CHECK(v, ...) \
  if (!(v)) { \
    ::td::detail::process_check_error(#v, __FILE__, __LINE__); \
  }

std::pair<td::Ref<vm::Cell>, td::Ref<vm::Cell>> replay_ordinary_transaction(
  td::Ref<vm::Cell> acc_root, td::Ref<vm::Cell> msg_root, td::Ref<vm::Cell> cfg_root,
  long long unsigned int lt, int now, long long unsigned int prev_lt, const td::BitArray<256> &prev_hash,
  bool special
) {
  td::Ref<vm::Cell> shard_root;
  block::gen::t_ShardAccount.cell_pack_account_descr(shard_root, std::move(acc_root), prev_hash, prev_lt);

  ton::StdSmcAddress addr;
  auto cs = vm::load_cell_slice(msg_root);
  bool external;
  td::Ref<vm::CellSlice> src, dest;
  const auto tt = block::gen::t_CommonMsgInfo.get_tag(cs);
  switch (block::gen::t_CommonMsgInfo.get_tag(cs)) {
    case block::gen::CommonMsgInfo::ext_in_msg_info: {
      block::gen::CommonMsgInfo::Record_ext_in_msg_info info;
      if (!tlb::unpack(cs, info)) {
        LOG(DEBUG) << "cannot unpack inbound external message";
        return {};
      }
      dest = std::move(info.dest);
      external = true;
      break;
    }
    case block::gen::CommonMsgInfo::int_msg_info: {
      block::gen::CommonMsgInfo::Record_int_msg_info info;
      CHECK(tlb::unpack(cs, info));
      src = std::move(info.src);
      dest = std::move(info.dest);
      external = false;
      break;
    }
    default:
      CHECK(false, "cannot unpack message to be processed by an ordinary transaction");
  }
  ton::WorkchainId wc;
  block::tlb::t_MsgAddressInt.extract_std_address(dest, wc, addr);
  LOG(DEBUG) << "inbound message to our smart contract " << addr.to_hex();
  auto is_masterchain = wc == -1;

  auto mode = block::Config::needWorkchainInfo | block::Config::needCapabilities | block::Config::needSpecialSmc;
  auto config_ = block::Config::unpack_config(vm::load_cell_slice_ref(std::move(cfg_root)), mode).move_as_ok();
  
  block::ActionPhaseConfig action_phase_cfg_;
  auto storage_prices_ = config_->get_storage_prices().move_as_ok();
  block::StoragePhaseConfig storage_phase_cfg_(&storage_prices_);
  block::ComputePhaseConfig compute_phase_cfg_;
  td::BitArray<256> rand_seed_;
  {
    // generate rand seed
    prng::rand_gen().strong_rand_bytes(rand_seed_.data(), 32);
    LOG(DEBUG) << "block random seed set to " << rand_seed_.to_hex();
  }
  {
    // compute compute_phase_cfg / storage_phase_cfg
    auto cell = config_->get_config_param(is_masterchain ? 20 : 21);
    if (cell.is_null()) {
      CHECK(false, "cannot fetch current gas prices and limits from masterchain configuration");
    }
    if (!compute_phase_cfg_.parse_GasLimitsPrices(std::move(cell), storage_phase_cfg_.freeze_due_limit,
                                                  storage_phase_cfg_.delete_due_limit)) {
      CHECK(false, "cannot unpack current gas prices and limits from masterchain configuration");
    }
    compute_phase_cfg_.block_rand_seed = rand_seed_;
    //compute_phase_cfg_.libraries = std::make_unique<vm::Dictionary>(config_->get_libraries_root(), 256);
    compute_phase_cfg_.global_config = config_->get_root_cell();
  }
  {
    // compute action_phase_cfg
    block::gen::MsgForwardPrices::Record rec;
    auto cell = config_->get_config_param(24);
    if (cell.is_null() || !tlb::unpack_cell(std::move(cell), rec)) {
      CHECK(false, "cannot fetch masterchain message transfer prices from masterchain configuration");
    }
    action_phase_cfg_.fwd_mc =
        block::MsgPrices{rec.lump_price,           rec.bit_price,          rec.cell_price, rec.ihr_price_factor,
                         (unsigned)rec.first_frac, (unsigned)rec.next_frac};
    cell = config_->get_config_param(25);
    if (cell.is_null() || !tlb::unpack_cell(std::move(cell), rec)) {
      CHECK(false, "cannot fetch standard message transfer prices from masterchain configuration");
    }
    action_phase_cfg_.fwd_std =
        block::MsgPrices{rec.lump_price,           rec.bit_price,          rec.cell_price, rec.ihr_price_factor,
                         (unsigned)rec.first_frac, (unsigned)rec.next_frac};
    action_phase_cfg_.workchains = &config_->get_workchain_list();
    action_phase_cfg_.bounce_msg_body = (config_->has_capability(ton::capBounceMsgBody) ? 256 : 0);
  }

  block::Account acc(wc, addr.cbits());
  if (!acc.unpack(vm::load_cell_slice_ref(std::move(shard_root)), td::Ref<vm::CellSlice>(), now, special)) {
    acc.init_new(now);
  }
  //if (external) {
  //  // transactions processing external messages must have lt larger than all processed internal messages
  //  lt = std::max(lt, last_proc_int_msg_.first);
  //}
  std::unique_ptr<block::Transaction> trans =
      std::make_unique<block::Transaction>(acc, block::Transaction::tr_ord, lt, now, msg_root);
  bool ihr_delivered = false;  // FIXME
  if (!trans->unpack_input_msg(ihr_delivered, &action_phase_cfg_)) {
    if (external) {
      // inbound external message was not accepted
      LOG(DEBUG) << "inbound external message rejected by account " << addr.to_hex()
                 << " before smart-contract execution";
      return {};
    }
    CHECK(false, "cannot unpack input message for a new transaction");
    return {};
  }
  if (trans->bounce_enabled) {
    if (!trans->prepare_storage_phase(storage_phase_cfg_, true)) {
      CHECK(false, "cannot create storage phase of a new transaction for smart contract "s + addr.to_hex());
      return {};
    }
    if (!external && !trans->prepare_credit_phase()) {
      CHECK(false, "cannot create credit phase of a new transaction for smart contract "s + addr.to_hex());
      return {};
    }
  } else {
    if (!external && !trans->prepare_credit_phase()) {
      CHECK(false, "cannot create credit phase of a new transaction for smart contract "s + addr.to_hex());
      return {};
    }
    if (!trans->prepare_storage_phase(storage_phase_cfg_, true, true)) {
      CHECK(false, "cannot create storage phase of a new transaction for smart contract "s + addr.to_hex());
      return {};
    }
  }
  
  if (!trans->prepare_compute_phase(compute_phase_cfg_)) {
    CHECK(false, "cannot create compute phase of a new transaction for smart contract "s + addr.to_hex());
    return {};
  }
  if (!trans->compute_phase->accepted) {
    if (external) {
      // inbound external message was not accepted
      LOG(DEBUG) << "inbound external message rejected by transaction " << addr.to_hex();
      return {};
    } else if (trans->compute_phase->skip_reason == block::ComputePhase::sk_none) {
      CHECK(false, "new ordinary transaction for smart contract "s + addr.to_hex() +
                       " has not been accepted by the smart contract (?)");
      return {};
    }
  }
  if (trans->compute_phase->success && !trans->prepare_action_phase(action_phase_cfg_)) {
    CHECK(false, "cannot create action phase of a new transaction for smart contract "s + addr.to_hex());
    return {};
  }
  if (trans->bounce_enabled && !trans->compute_phase->success && !trans->prepare_bounce_phase(action_phase_cfg_)) {
    CHECK(false, "cannot create bounce phase of a new transaction for smart contract "s + addr.to_hex());
    return {};
  }
  if (!trans->serialize()) {
    CHECK(false, "cannot serialize new transaction for smart contract "s + addr.to_hex());
    return {};
  }
  auto trans_root = trans->commit(acc);
  if (trans_root.is_null()) {
    CHECK(false, "cannot commit new transaction for smart contract "s + addr.to_hex());
    return {};
  }
  return std::make_pair(trans_root, acc.total_state);
}

std::pair<td::Ref<vm::Cell>, td::Ref<vm::Cell>> replay_ticktock_transaction(
  td::Ref<vm::Cell> acc_root, td::Ref<vm::Cell> cfg_root,
  long long unsigned int lt, int now, long long unsigned int prev_lt, const td::BitArray<256> &prev_hash, 
  bool tick
) {
  block::gen::Account::Record_account acc_record;
  CHECK(tlb::unpack_cell(acc_root, acc_record), "cannot unpack account");
  td::Ref<vm::CellSlice> dest = acc_record.addr;

  td::Ref<vm::Cell> shard_root;
  block::gen::t_ShardAccount.cell_pack_account_descr(shard_root, std::move(acc_root), prev_hash, prev_lt);

  ton::StdSmcAddress addr;
  ton::WorkchainId wc;
  block::tlb::t_MsgAddressInt.extract_std_address(dest, wc, addr);
  LOG(DEBUG) << "tick tock smart contract " << addr.to_hex();
  auto is_masterchain = wc == -1;

  auto mode = block::Config::needWorkchainInfo | block::Config::needCapabilities | block::Config::needSpecialSmc;
  auto config_ = block::Config::unpack_config(vm::load_cell_slice_ref(std::move(cfg_root)), mode).move_as_ok();

  block::ActionPhaseConfig action_phase_cfg_;
  auto storage_prices_ = config_->get_storage_prices().move_as_ok();
  block::StoragePhaseConfig storage_phase_cfg_(&storage_prices_);
  block::ComputePhaseConfig compute_phase_cfg_;
  td::BitArray<256> rand_seed_;
  {
    // generate rand seed
    prng::rand_gen().strong_rand_bytes(rand_seed_.data(), 32);
    LOG(DEBUG) << "block random seed set to " << rand_seed_.to_hex();
  }
  {
    // compute compute_phase_cfg / storage_phase_cfg
    auto cell = config_->get_config_param(is_masterchain ? 20 : 21);
    if (cell.is_null()) {
      CHECK(false, "cannot fetch current gas prices and limits from masterchain configuration");
    }
    if (!compute_phase_cfg_.parse_GasLimitsPrices(std::move(cell), storage_phase_cfg_.freeze_due_limit,
                                                  storage_phase_cfg_.delete_due_limit)) {
      CHECK(false, "cannot unpack current gas prices and limits from masterchain configuration");
    }
    compute_phase_cfg_.block_rand_seed = rand_seed_;
    //compute_phase_cfg_.libraries = std::make_unique<vm::Dictionary>(config_->get_libraries_root(), 256);
    compute_phase_cfg_.global_config = config_->get_root_cell();
  }
  {
    // compute action_phase_cfg
    block::gen::MsgForwardPrices::Record rec;
    auto cell = config_->get_config_param(24);
    if (cell.is_null() || !tlb::unpack_cell(std::move(cell), rec)) {
      CHECK(false, "cannot fetch masterchain message transfer prices from masterchain configuration");
    }
    action_phase_cfg_.fwd_mc =
        block::MsgPrices{rec.lump_price,           rec.bit_price,          rec.cell_price, rec.ihr_price_factor,
                         (unsigned)rec.first_frac, (unsigned)rec.next_frac};
    cell = config_->get_config_param(25);
    if (cell.is_null() || !tlb::unpack_cell(std::move(cell), rec)) {
      CHECK(false, "cannot fetch standard message transfer prices from masterchain configuration");
    }
    action_phase_cfg_.fwd_std =
        block::MsgPrices{rec.lump_price,           rec.bit_price,          rec.cell_price, rec.ihr_price_factor,
                         (unsigned)rec.first_frac, (unsigned)rec.next_frac};
    action_phase_cfg_.workchains = &config_->get_workchain_list();
    action_phase_cfg_.bounce_msg_body = (config_->has_capability(ton::capBounceMsgBody) ? 256 : 0);
  }

  block::Account acc(wc, addr.cbits());
  acc.unpack(vm::load_cell_slice_ref(std::move(shard_root)), td::Ref<vm::CellSlice>(), now, true);
  //if (external) {
  //  // transactions processing external messages must have lt larger than all processed internal messages
  //  lt = std::max(lt, last_proc_int_msg_.first);
  //}

  std::unique_ptr<block::Transaction> trans = std::make_unique<block::Transaction>(
      acc, tick ? block::Transaction::tr_tick : block::Transaction::tr_tock, lt, now);
  if (!trans->prepare_storage_phase(storage_phase_cfg_, true)) {
    CHECK(false, "cannot create storage phase of a new transaction for smart contract "} + smc_addr.to_hex());
    return {};
  }
  if (!trans->prepare_compute_phase(compute_phase_cfg_)) {
    CHECK(false, "cannot create compute phase of a new transaction for smart contract "} + smc_addr.to_hex());
    return {};
  }
  if (!trans->compute_phase->accepted && trans->compute_phase->skip_reason == block::ComputePhase::sk_none) {
    CHECK(false, "new tick-tock transaction for smart contract "} + smc_addr.to_hex() + " has not been accepted by the smart contract (?)");
    return {};
  }
  if (trans->compute_phase->success && !trans->prepare_action_phase(action_phase_cfg_)) {
    CHECK(false, "cannot create action phase of a new transaction for smart contract "} + smc_addr.to_hex());
    return {};
  }
  if (!trans->serialize()) {
    CHECK(false, "cannot serialize new transaction for smart contract "s + addr.to_hex());
    return {};
  }
  auto trans_root = trans->commit(acc);
  if (trans_root.is_null()) {
    CHECK(false, "cannot commit new transaction for smart contract "s + addr.to_hex());
    return {};
  }
  return std::make_pair(trans_root, acc.total_state);
}
