set(LIB_SRC TcpConnection.cc daqb.cc)
add_library(comet_wsf_remote STATIC ${LIB_SRC})

target_link_libraries(comet_wsf_remote
  PUBLIC
  Threads::Threads
  msgpack
  )


target_include_directories(comet_wsf_remote
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
  )

set(LIB_PUBLIC_HEADERS daqb.hh TcpConnection.hh)
set_target_properties(comet_wsf_remote PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
set_target_properties(comet_wsf_remote PROPERTIES LINKER_LANGUAGE CXX)

install(TARGETS comet_wsf_remote
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )

