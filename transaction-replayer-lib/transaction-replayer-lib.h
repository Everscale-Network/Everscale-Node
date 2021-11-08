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
#include "block-auto.h"

std::pair<td::Ref<vm::Cell>, td::Ref<vm::Cell>> replay_ordinary_transaction(td::Ref<vm::Cell> acc_root, td::Ref<vm::Cell> msg_root, td::Ref<vm::Cell> cfg_root, 
                                                                            long long unsigned int lt, int now, long long unsigned int prev_lt, const td::BitArray<256> &prev_hash,
                                                                            bool special);

std::pair<td::Ref<vm::Cell>, td::Ref<vm::Cell>> replay_ticktock_transaction(td::Ref<vm::Cell> acc_root, td::Ref<vm::Cell> cfg_root,
                                                                            long long unsigned int lt, int now, long long unsigned int prev_lt, const td::BitArray<256> &prev_hash,
                                                                            bool tick);
