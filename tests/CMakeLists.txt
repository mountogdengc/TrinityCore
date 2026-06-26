# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tests)

CollectAndAddSourceFiles(
  tests
  ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotCombatPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotCohortPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotDeathPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotMovementPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotPetPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotRangedAttackPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots/BotRotationPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/server/scripts/EasternKingdoms/tirisfal_recruitment.cpp)

target_link_libraries(tests
  PRIVATE
    trinity-core-interface
    game
    Catch2::Catch2)

target_include_directories(tests
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/Bots)

catch_discover_tests(tests)

set_target_properties(tests
    PROPERTIES
      FOLDER
        "tests")
