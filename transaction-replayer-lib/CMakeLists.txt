cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)


set(TRANSACTION_REPLAYER_LIB_SOURCE
  transaction-replayer-lib.cpp
)

add_library(transaction-replayer-lib STATIC ${TRANSACTION_REPLAYER_LIB_SOURCE})
target_link_libraries(transaction-replayer-lib fift-lib)
