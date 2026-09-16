add_library(retro_achievements STATIC
    client.cpp
    client.h
    client_observer.h
    models.h
)

target_link_libraries(retro_achievements
    PUBLIC rcheevos
    PRIVATE citra_common httplib
)
