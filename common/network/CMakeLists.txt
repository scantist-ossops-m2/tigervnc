include_directories(${CMAKE_SOURCE_DIR}/common)

add_library(network STATIC
  Socket.cxx
  Socket.h
  TcpSocket.cxx
  TcpSocket.h
  )

if(NOT WIN32)
  target_sources(network PRIVATE UnixSocket.cxx UnixSocket.h)
endif()

if(UNIX)
  libtool_create_control_file(network)
endif()
