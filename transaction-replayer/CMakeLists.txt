cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

if (NOT OPENSSL_FOUND)
  find_package(OpenSSL REQUIRED)
endif()


set(TRANSACTION_REPLAYER_SOURCE
  transaction-replayer.cpp
)

add_executable(transaction-replayer ${TRANSACTION_REPLAYER_SOURCE})
target_link_libraries(transaction-replayer transaction-replayer-lib ${JEMALLOC_LIBRARIES})

install(TARGETS transaction-replayer RUNTIME DESTINATION bin)
