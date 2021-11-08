#include <random>

#include "transaction-replayer-lib/transaction-replayer-lib.h"

#include "fift/utils.h"

#include "td/utils/tests.h"
#include "crypto/block/transaction.h"
#include "block-parse.h"
#include "td/utils/filesystem.h"

const int BLOCK_LT = 2000000000;
const int BLOCK_UT = 1576526553;
const int ACCOUNT_UT = 1572169011;

static td::Ref<vm::Cell> load_file_as_cell(const char *path) {
  auto data = td::read_file_str(td::CSlice(path)).move_as_ok();
  return vm::std_boc_deserialize(td::Slice{data.data(), data.length()}).move_as_ok();
}

td::UInt256 create_snd_address() {
  td::UInt256 acc_addr;
  acc_addr.set_zero();
  acc_addr.as_slice()[1] = 2;
  return acc_addr;
}

td::UInt256 create_fst_address() {
  td::UInt256 acc_addr;
  acc_addr.set_zero();
  acc_addr.as_slice()[1] = 1;
  return acc_addr;
}

void create_int_msg(vm::CellBuilder &cb, bool bounce, unsigned long long val, int lt, bool is_src) {
  block::gen::CommonMsgInfoRelaxed::Record_int_msg_info info;
  info.ihr_disabled = true;
  info.bounce = bounce;
  info.bounced = false;
  info.created_at = lt;
  info.created_lt = lt;
  const auto fst_addr = block::tlb::t_MsgAddressInt.pack_std_address(block::StdAddress(-1, td::ConstBitPtr(create_snd_address().raw)));
  const auto snd_addr = block::tlb::t_MsgAddressInt.pack_std_address(block::StdAddress(-1, td::ConstBitPtr(create_fst_address().raw)));
  if (is_src) {
    info.src = fst_addr;
    info.dest = snd_addr;
  } else {
    info.dest = fst_addr;
    info.src = snd_addr;
  }
  
  td::RefInt256 value = td::make_refint(val);
  td::Ref<vm::Cell> extra;
  vm::CellBuilder balance;
  CHECK(block::store_CurrencyCollection(balance, value, extra));
  info.value = balance.as_cellslice_ref();
  
  vm::CellBuilder balance_fee;
  CHECK(block::tlb::Grams{}.store_integer_ref(balance_fee, td::make_refint(0)));
  info.ihr_fee = balance_fee.as_cellslice_ref();
  info.fwd_fee = balance_fee.as_cellslice_ref();
  
  block::gen::MessageRelaxed::Record msg;
  CHECK(tlb::csr_pack(msg.info, info));
  td::Ref<vm::Cell> cell;
  vm::CellBuilder ccb;
  CHECK(block::gen::t_Maybe_Either_StateInit_Ref_StateInit.pack_nothing(ccb));
  CHECK(ccb.finalize_to(cell));
  msg.init = vm::load_cell_slice_ref(cell);
  msg.body = vm::load_cell_slice_ref(cell);
  CHECK(tlb::type_pack(cb, block::gen::t_MessageRelaxed_Any, msg));
}

td::Ref<vm::CellSlice> create_int_msg_cell_slice(bool bounce, unsigned long long val, int lt, bool is_src) {
  vm::CellBuilder cb;
  create_int_msg(cb, bounce, val, lt, is_src);
  return cb.as_cellslice_ref();
}

td::Ref<vm::Cell> create_int_msg_cell(bool bounce, unsigned long long val, int lt, bool is_src) {
  vm::CellBuilder cb;
  create_int_msg(cb, bounce, val, lt, is_src);
  td::Ref<vm::Cell> result;
  CHECK(cb.finalize_to(result));
  return result;
}

std::pair<td::Ref<vm::CellSlice>, td::Ref<vm::CellSlice>> create_two_int_msgs() {
  return std::make_pair(create_int_msg_cell_slice(false, 50000000, BLOCK_LT + 2, false), create_int_msg_cell_slice(true, 100000000, BLOCK_LT + 3, false));
}

td::Ref<vm::Cell> create_two_int_msgs_cell() {
  auto [msg1, msg2] = create_two_int_msgs();
  vm::CellBuilder data;
  std::array<unsigned char, 32> arr;
  std::fill(arr.begin(), arr.end(), 0x55);
  td::Ref<vm::Cell> result;
  td::Ref<vm::Cell> result2;
  td::Ref<vm::Cell> result3;
  
  CHECK(data.append_cellslice_bool(msg1));
  CHECK(data.finalize_to(result));
  
  CHECK(data.append_cellslice_bool(msg2));
  data.finalize_to(result2);
  
  data.store_bytes(arr.data(), arr.size());
  CHECK(data.store_ref_bool(result2));
  CHECK(data.store_ref_bool(result));
  
  CHECK(data.finalize_to(result));
  return result;
}

static bool compute_state(block::Account &acc) {
  using namespace block;
  
  td::Ref<vm::Cell> new_total_state;
  
  if (acc.status == Account::acc_nonexist || acc.status == Account::acc_deleted) {
    CHECK(acc.balance.is_zero());
    vm::CellBuilder cb;
    CHECK(cb.store_long_bool(0, 1)  // account_none$0
          && cb.finalize_to(new_total_state));
    acc.total_state = new_total_state;
    return true;
  }
  vm::CellBuilder cb;
  CHECK(cb.store_long_bool(acc.last_trans_lt_, 64)  // account_storage$_ last_trans_lt:uint64
        && acc.balance.store(cb));          // balance:CurrencyCollection
  unsigned si_pos = 0;
  if (acc.status == Account::acc_uninit) {
    CHECK(cb.store_long_bool(0, 2));  // account_uninit$00 = AccountState
  } else if (acc.status == Account::acc_frozen) {
    vm::CellBuilder cb2;
    CHECK(acc.split_depth_ ? cb2.store_long_bool(acc.split_depth_ + 32, 6)  // _ ... = StateInit
                                : cb2.store_long_bool(0, 1));                        // ... split_depth:(Maybe (## 5))
    CHECK(cb2.store_long_bool(0, 1));  // special:(Maybe TickTock)
    CHECK(cb2.store_maybe_ref(acc.code) && cb2.store_maybe_ref(acc.data) && cb2.store_maybe_ref(acc.library));
    // code:(Maybe ^Cell) data:(Maybe ^Cell) library:(HashmapE 256 SimpleLib)
    auto frozen_state = cb2.finalize();
    td::BitArray<256> frozen_hash;
    frozen_hash = frozen_state->get_hash().bits();
    if (frozen_hash == acc.addr_orig) {
      // if frozen_hash equals account's "original" address (before rewriting), do not need storing hash
      CHECK(cb.store_long_bool(0, 2));  // account_uninit$00 = AccountState
    } else {
      CHECK(cb.store_long_bool(1, 2)              // account_frozen$01
            && cb.store_bits_bool(frozen_hash));  // state_hash:bits256
    }
  } else {
    CHECK(acc.status == Account::acc_active);
    si_pos = cb.size_ext() + 1;
    CHECK(acc.split_depth_ ? cb.store_long_bool(acc.split_depth_ + 96, 7)      // account_active$1 _:StateInit
                               : cb.store_long_bool(2, 2));                            // ... split_depth:(Maybe (## 5))
    CHECK(cb.store_long_bool(0, 1));  // special:(Maybe TickTock)
    CHECK(cb.store_maybe_ref(acc.code) && cb.store_maybe_ref(acc.data) && cb.store_maybe_ref(acc.library));
    // code:(Maybe ^Cell) data:(Maybe ^Cell) library:(HashmapE 256 SimpleLib)
  }
  auto storage = cb.finalize();
  Ref<vm::CellSlice> new_inner_state;
  if (si_pos) {
    auto cs_ref = load_cell_slice_ref(storage);
    CHECK(cs_ref.unique_write().skip_ext(si_pos));
    new_inner_state = std::move(cs_ref);
  } else {
    new_inner_state.clear();
  }
  vm::CellStorageStat stats;
  CHECK(stats.compute_used_storage(Ref<vm::Cell>(storage)));
  bool res = cb.store_long_bool(1, 1)                       // account$1
        && cb.append_cellslice_bool(acc.my_addr)   // addr:MsgAddressInt
        && block::store_UInt7(cb, stats.cells)         // storage_used$_ cells:(VarUInteger 7)
        && block::store_UInt7(cb, stats.bits)          //   bits:(VarUInteger 7)
        && block::store_UInt7(cb, stats.public_cells)  //   public_cells:(VarUInteger 7)
        && cb.store_long_bool(acc.last_paid, 32);         // last_paid:uint32
  CHECK(res);
  if (acc.due_payment.not_null() && td::sgn(acc.due_payment) != 0) {
    CHECK(cb.store_long_bool(1, 1) && block::tlb::t_Grams.store_integer_ref(cb, acc.due_payment));
    // due_payment:(Maybe Grams)
  } else {
    CHECK(cb.store_long_bool(0, 1));
  }
  CHECK(cb.append_data_cell_bool(std::move(storage)));
  new_total_state = cb.finalize();
  CHECK(block::gen::t_Account.validate_ref(new_total_state));
  CHECK(block::tlb::t_Account.validate_ref(new_total_state));
  
  acc.total_state = new_total_state;
  return true;
}

block::Account create_account(const std::string &code, unsigned long long value) {
  block::Account acc(-1, td::ConstBitPtr(create_fst_address().raw));
    
  acc.code = fift::compile_asm(code, "", true).move_as_ok();
  acc.data = create_two_int_msgs_cell();
  acc.balance = block::CurrencyCollection(value);
  acc.status = block::Account::acc_active;
  acc.orig_status = block::Account::acc_active;
  acc.last_trans_lt_ = BLOCK_LT + 2;
  acc.last_paid = ACCOUNT_UT;
  
  acc.addr_orig = acc.addr;
  acc.addr_rewrite = acc.addr.bits();
  CHECK(acc.compute_my_addr(true));
  
  CHECK(compute_state(acc));
  
  return acc;
}

void run_tx(const std::string &code, long long unsigned int acc_balance, long long unsigned int msg_balance, int out_msgs, long long unsigned int result_acc_balance) {
  block::Account acc = create_account(code, acc_balance);
  td::Ref<vm::Cell> msg = create_int_msg_cell(false, msg_balance, BLOCK_LT - 2, true);
  auto cfg_root = load_file_as_cell(ADDITIONAL_ARGS()[0].c_str());
   
  const auto [trans_new, acc_new] = replay_ordinary_transaction(acc.total_state, msg, cfg_root, BLOCK_LT, BLOCK_UT, BLOCK_LT + 1, td::BitArray<256>(), false);
  block::gen::Transaction::Record result_trans;
  CHECK(tlb::unpack_cell(std::move(trans_new), result_trans));
  
  auto fees = block::CurrencyCollection{};
  fees.unpack(result_trans.total_fees);
  ASSERT_EQ(result_trans.outmsg_cnt, out_msgs);
  
  td::Ref<vm::Cell> shard_root;
  CHECK(block::gen::t_ShardAccount.cell_pack_account_descr(shard_root, std::move(acc_new), td::BitArray<256>(), BLOCK_LT + 1));
  block::Account acc2(acc.workchain, acc.addr.cbits());
  if (acc2.unpack(vm::load_cell_slice_ref(std::move(shard_root)), td::Ref<vm::CellSlice>(), BLOCK_UT, false)) {
    ASSERT_EQ(acc2.get_balance().to_str(), std::to_string(result_acc_balance) + "ng");
  }
}

TEST(TRANSACTION, fix_128_flag) {
  // message with 128 flag is processed last
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "128 PUSHINT\n"
      "SENDRAWMSG\n"
      "1 PUSHINT\n"
      "SENDRAWMSG\n";
    
    run_tx(code, 310000000, 1230000000, 2, 0);
  }
    
  // if money is not enought, transaction fail
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "128 PUSHINT\n"
      "SENDRAWMSG\n"
      "1 PUSHINT\n"
      "SENDRAWMSG\n";
    
    run_tx(code, 310000000, 44404882 + 68259633 - 1, 0, 119999999);
  }
  
  // if money is not enought, with mode 2 message will be skipped
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "130 PUSHINT\n"
      "SENDRAWMSG\n"
      "1 PUSHINT\n"
      "SENDRAWMSG\n";
    
    run_tx(code, 310000000, 44404882 + 68259633 - 1, 1, 9999999);
  }
  
  // two messages with 128 flag is disabled
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "128 PUSHINT\n"
      "SENDRAWMSG\n"
      "128 PUSHINT\n"
      "SENDRAWMSG\n";
    
    run_tx(code, 310000000, 1230000000, 0, 1236111632);
  }
  
  // two messages with 128 flag is disabled, but flag 2 ignores error
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "128 PUSHINT\n"
      "SENDRAWMSG\n"
      "130 PUSHINT\n"
      "SENDRAWMSG\n";
    
    run_tx(code, 310000000, 1230000000, 1, 0);
  }
  
  // message with 128 flag is processed after rawreserve
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "128 PUSHINT\n"
      "SENDRAWMSG\n"
      "1000 PUSHINT\n"
      "0 PUSHINT\n"
      "RAWRESERVE\n";
    
    run_tx(code, 310000000, 1230000000, 1, 1000);
  }
  
  // message with 128+32 flag is processed last
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "160 PUSHINT\n"
      "SENDRAWMSG\n"
      "0 PUSHINT\n"
      "SENDRAWMSG\n";
    
    run_tx(code, 310000000, 1230000000, 2, 0);
  }
  
  // message with 32 flag is not valid without 128 flag
  {
    const std::string code = 
      "\n"
      "ACCEPT\n"
      "PUSHROOT\n"
      "CTOS\n"
      "LDREF\n"
      "PLDREF\n"
      "32 PUSHINT\n"
      "SENDRAWMSG\n"
      "0 PUSHINT\n"
      "SENDRAWMSG\n";
    
    run_tx(code, 310000000, 1230000000, 0, 1237947412);
  }
}
