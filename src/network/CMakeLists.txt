add_library(network STATIC
    announce_multiplayer_session.cpp
    announce_multiplayer_session.h
    artic_base/artic_base_client.cpp
    artic_base/artic_base_client.h
    artic_base/artic_base_common.h
    network.cpp
    network.h
    network_settings.cpp
    network_settings.h
    packet.cpp
    packet.h
    precompiled_headers.h
    room.cpp
    room.h
    room_member.cpp
    room_member.h
    socket_manager.cpp
    socket_manager.h
    verify_user.cpp
    verify_user.h
)

create_target_directory_groups(network)

if (ENABLE_WEB_SERVICE)
    target_link_libraries(network PRIVATE web_service)
endif()

target_link_libraries(network PRIVATE lime_common enet Boost::serialization httplib)

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(network PRIVATE precompiled_headers.h)
endif()
