include_directories(${CMAKE_SOURCE_DIR}/common)
include_directories(${JPEG_INCLUDE_DIR})
include_directories(${PIXMAN_INCLUDE_DIRS})

add_library(rfb STATIC
  Blacklist.cxx
  Blacklist.h
  CConnection.cxx
  CConnection.h
  CMsgHandler.cxx
  CMsgHandler.h
  CMsgReader.cxx
  CMsgReader.h
  CMsgWriter.cxx
  CMsgWriter.h
  CSecurity.h
  CSecurityNone.h
  CSecurityPlain.cxx
  CSecurityPlain.h
  CSecurityRSAAES.h
  CSecurityStack.cxx
  CSecurityStack.h
  CSecurityTLS.h
  CSecurityVeNCrypt.cxx
  CSecurityVeNCrypt.h
  CSecurityVncAuth.cxx
  CSecurityVncAuth.h
  ClientParams.cxx
  ClientParams.h
  ComparingUpdateTracker.cxx
  ComparingUpdateTracker.h
  Configuration.cxx
  Configuration.h
  Congestion.cxx
  Congestion.h
  CopyRectDecoder.cxx
  CopyRectDecoder.h
  Cursor.cxx
  Cursor.h
  DecodeManager.cxx
  DecodeManager.h
  Decoder.cxx
  Decoder.h
  EncodeManager.cxx
  EncodeManager.h
  Encoder.cxx
  Encoder.h
  Exception.h
  H264Decoder.h
  H264DecoderContext.h
  H264LibavDecoderContext.h
  H264WinDecoderContext.h
  HextileDecoder.cxx
  HextileDecoder.h
  HextileEncoder.cxx
  HextileEncoder.h
  Hostname.h
  InputHandler.h
  JpegCompressor.cxx
  JpegCompressor.h
  JpegDecompressor.cxx
  JpegDecompressor.h
  KeyRemapper.cxx
  KeyRemapper.h
  LogWriter.cxx
  LogWriter.h
  Logger.cxx
  Logger.h
  Logger_file.cxx
  Logger_file.h
  Logger_stdio.cxx
  Logger_stdio.h
  Logger_syslog.h
  Palette.h
  Password.cxx
  Password.h
  Pixel.h
  PixelBuffer.cxx
  PixelBuffer.h
  PixelFormat.cxx
  PixelFormat.h
  RREDecoder.cxx
  RREDecoder.h
  RREEncoder.cxx
  RREEncoder.h
  RawDecoder.cxx
  RawDecoder.h
  RawEncoder.cxx
  RawEncoder.h
  Rect.h
  Region.cxx
  Region.h
  SConnection.cxx
  SConnection.h
  SDesktop.h
  SMsgHandler.cxx
  SMsgHandler.h
  SMsgReader.cxx
  SMsgReader.h
  SMsgWriter.cxx
  SMsgWriter.h
  SSecurity.h
  SSecurityNone.h
  SSecurityPlain.cxx
  SSecurityPlain.h
  SSecurityRSAAES.h
  SSecurityStack.cxx
  SSecurityStack.h
  SSecurityTLS.h
  SSecurityVeNCrypt.cxx
  SSecurityVeNCrypt.h
  SSecurityVncAuth.cxx
  SSecurityVncAuth.h
  ScaleFilters.cxx
  ScaleFilters.h
  ScreenSet.h
  Security.cxx
  Security.h
  SecurityClient.cxx
  SecurityClient.h
  SecurityServer.cxx
  SecurityServer.h
  ServerCore.cxx
  ServerCore.h
  ServerParams.cxx
  ServerParams.h
  TightConstants.h
  TightDecoder.cxx
  TightDecoder.h
  TightEncoder.cxx
  TightEncoder.h
  TightJPEGEncoder.cxx
  TightJPEGEncoder.h
  Timer.cxx
  Timer.h
  UnixPasswordValidator.h
  UpdateTracker.cxx
  UpdateTracker.h
  UserMsgBox.h
  UserPasswdGetter.h
  VNCSConnectionST.cxx
  VNCSConnectionST.h
  VNCServer.h
  VNCServerST.cxx
  VNCServerST.h
  WinPasswdValidator.h
  XF86keysym.h
  ZRLEDecoder.cxx
  ZRLEDecoder.h
  ZRLEEncoder.cxx
  ZRLEEncoder.h
  clipboardTypes.h
  d3des.c
  d3des.h
  encodings.cxx
  encodings.h
  fenceTypes.h
  hextileConstants.h
  hextileDecode.h
  hextileEncode.h
  hextileEncodeBetter.h
  keysymdef.h
  ledStates.h
  msgTypes.h
  pam.h
  qemuTypes.h
  rreDecode.h
  rreEncode.h
  screenTypes.h
  tightDecode.h
  util.cxx
  util.h
  zrleDecode.h
  )

target_link_libraries(rfb os rdr)
target_link_libraries(rfb ${JPEG_LIBRARIES} ${PIXMAN_LIBRARIES})
target_link_directories(rfb PUBLIC ${PIXMAN_LIBRARY_DIRS})

if(ENABLE_H264 AND NOT H264_LIBS STREQUAL "NONE")
  target_sources(rfb PRIVATE H264Decoder.cxx H264DecoderContext.cxx)
  if(H264_LIBS STREQUAL "LIBAV")
    target_sources(rfb PRIVATE H264LibavDecoderContext.cxx)
  elseif(H264_LIBS STREQUAL "WIN")
    target_sources(rfb PRIVATE H264WinDecoderContext.cxx)
  endif()
  include_directories(${H264_INCLUDE_DIRS})
  target_link_libraries(rfb ${H264_LIBRARIES})
  target_link_directories(rfb PUBLIC ${H264_LIBRARY_DIRS})
endif()

if(UNIX)
  target_sources(rfb PRIVATE Logger_syslog.cxx)
endif()

if(WIN32)
  include_directories(${CMAKE_SOURCE_DIR}/win)
  target_sources(rfb PRIVATE WinPasswdValidator.cxx)
endif(WIN32)

if(UNIX AND NOT APPLE)
  target_sources(rfb PRIVATE UnixPasswordValidator.cxx pam.c)
  target_link_libraries(rfb ${PAM_LIBS})
endif()

if(GNUTLS_FOUND)
  target_sources(rfb PRIVATE CSecurityTLS.cxx SSecurityTLS.cxx)
  include_directories(${GNUTLS_INCLUDE_DIR})
  target_link_libraries(rfb ${GNUTLS_LIBRARIES})
endif()

if (NETTLE_FOUND)
  target_sources(rfb PRIVATE CSecurityRSAAES.cxx SSecurityRSAAES.cxx)
  include_directories(${NETTLE_INCLUDE_DIRS} ${GMP_INCLUDE_DIRS})
  target_link_libraries(rfb ${HOGWEED_LIBRARIES}
                        ${NETTLE_LIBRARIES} ${GMP_LIBRARIES})
  target_link_directories(rfb PUBLIC ${HOGWEED_LIBRARY_DIRS}
                          ${NETTLE_LIBRARY_DIRS} ${GMP_LIBRARY_DIRS})
endif()

if(UNIX)
  libtool_create_control_file(rfb)
endif()
