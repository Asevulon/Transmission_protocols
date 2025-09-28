create_executable(udp_server "server")
create_executable(udp_client "client")
target_include_directories(udp_server PRIVATE "includes")
target_include_directories(udp_client PRIVATE "includes")
