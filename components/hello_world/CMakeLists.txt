ulmk_component_register(
    NAME         hello_world
    ENABLED      OFF
    REQUIRES     ping_pong
    SOURCES
        src/root_thread.c
        src/hello_world.c
    INCLUDE_DIRS include
    ROOT_THREAD
)
