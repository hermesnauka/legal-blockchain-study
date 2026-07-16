cmake_minimum_required(VERSION 3.20)
project(legalchain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)

# --- Drogon (HTTP + WebSocket framework) — expected to be installed via apt
# (libdrogon-dev), see README.md "Prerequisites".
find_package(Drogon REQUIRED)
find_package(OpenSSL REQUIRED)

# --- liboqs (NIST PQC reference library: ML-KEM / ML-DSA) -------------------
# Not packaged in apt; vendored at configure time. Only the C library and
# headers are needed — no docs/tests/apps in the vendored build.
set(OQS_BUILD_ONLY_LIB ON CACHE BOOL "" FORCE)
set(OQS_DIST_BUILD OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  liboqs
  GIT_REPOSITORY https://github.com/open-quantum-safe/liboqs.git
  GIT_TAG 0.16.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(liboqs)

# --- Ledger library (everything except main.cpp) ----------------------------
add_library(legalchain_lib STATIC
  src/core/Block.cpp
  src/core/Blockchain.cpp
  src/core/NodeWallet.cpp
  src/core/TokenomicsService.cpp
  src/core/Transaction.cpp
  src/crypto/HashUtil.cpp
  src/crypto/PqcSignatureService.cpp
  src/crypto/PqcKeyExchangeService.cpp
  src/crypto/SecureChannel.cpp
  src/consensus/ConsensusEngine.cpp
  src/consensus/ProofOfWorkStrategy.cpp
  src/consensus/ProofOfStakeStrategy.cpp
  src/consensus/BftStrategy.cpp
  src/contracts/ContractEngine.cpp
  src/contracts/MedicalConsentContract.cpp
  src/contracts/AgriSupplyChainContract.cpp
  src/nft/NftService.cpp
  src/p2p/P2pService.cpp
  src/p2p/P2pClientService.cpp
  src/p2p/EventBroadcaster.cpp
  src/api/ChainController.cpp
  src/api/TransactionController.cpp
  src/api/NodeController.cpp
  src/api/WalletController.cpp
  src/api/NftController.cpp
  src/api/ContractController.cpp
  src/api/ConsensusController.cpp
  src/api/I18nController.cpp
  src/api/ApiExceptionHandler.cpp
  src/config/Config.cpp
)
target_include_directories(legalchain_lib PUBLIC src)
target_link_libraries(legalchain_lib PUBLIC Drogon::Drogon OpenSSL::Crypto oqs)

# --- Executable ---------------------------------------------------------
add_executable(legalchain_node src/main.cpp)
target_link_libraries(legalchain_node PRIVATE legalchain_lib)

# Copy runtime resources (i18n dictionaries, default config) next to the binary.
add_custom_command(TARGET legalchain_node POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/i18n $<TARGET_FILE_DIR:legalchain_node>/i18n
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/config $<TARGET_FILE_DIR:legalchain_node>/config
)

# --- Tests ----------------------------------------------------------------
option(LEGALCHAIN_BUILD_TESTS "Build the GoogleTest suite" ON)
if(LEGALCHAIN_BUILD_TESTS)
  enable_testing()
  # Vendored via FetchContent (not apt) so the test suite doesn't need
  # libgtest-dev installed system-wide.
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(googletest)
  add_executable(legalchain_tests
    tests/BlockchainTest.cpp
    tests/PqcCryptoTest.cpp
    tests/ConsensusStrategyTest.cpp
    tests/ContractsTest.cpp
    tests/ApiContractIntegrationTest.cpp
    tests/TwoNodeSyncIntegrationTest.cpp
  )
  target_link_libraries(legalchain_tests PRIVATE legalchain_lib gtest gtest_main)
  add_custom_command(TARGET legalchain_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/i18n $<TARGET_FILE_DIR:legalchain_tests>/i18n
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/config $<TARGET_FILE_DIR:legalchain_tests>/config
  )
  include(GoogleTest)
  gtest_discover_tests(legalchain_tests)
endif()
