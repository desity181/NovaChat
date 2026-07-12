include(CTest)
include(Catch)

add_executable(NovaChat_Tests
    logger/test_logger.cpp
    config/test_appconfig.cpp
    storage/test_database.cpp
    storage/test_user_repository.cpp
    storage/test_contact_repository.cpp
    storage/test_message_repository.cpp
    storage/test_conversation_repository.cpp
    network/test_codec.cpp
)
target_include_directories(NovaChat_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(NovaChat_Tests PRIVATE
    NovaChat_Core
    NovaChat_Proto
    Catch2::Catch2WithMain
)

# Resolve Qt bin dir from the imported target so the path is not hardcoded
get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION_DEBUG)
if(NOT _qt_core_loc)
    get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION)
endif()
get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)

catch_discover_tests(NovaChat_Tests
    DISCOVERY_MODE PRE_TEST
    DL_PATHS "${_qt_bin_dir}"
)
