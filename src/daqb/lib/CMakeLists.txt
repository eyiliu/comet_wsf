set(LIB_SRC comet_wsf_regctrl.cc comet_wsf_datarecv.cc TcpServer.cc TcpConnection.cc)
add_library(comet_wsf_daqb STATIC ${LIB_SRC})
target_link_libraries(comet_wsf_daqb
  PUBLIC
  Threads::Threads
  msgpack
  )

target_include_directories(comet_wsf_daqb
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
  )

set(LIB_PUBLIC_HEADERS comet_wsf_regctrl.hh comet_wsf_datarecv.hh comet_wsf_regdefine.hh comet_wsf_system.hh TcpServer.hh TcpConnection.hh)
set_target_properties(comet_wsf_daqb PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
set_target_properties(comet_wsf_daqb PROPERTIES LINKER_LANGUAGE CXX)

install(TARGETS comet_wsf_daqb
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
