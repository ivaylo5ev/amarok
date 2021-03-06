include_directories( .
  ${AMAROK_TEST_TREE}
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_BINARY_DIR}/tests
  ${KDE4_INCLUDE_DIR}
)

#------------------------ Test ASXPlaylist -----------------------------

set( testasxplaylist_SRCS asx/TestASXPlaylist.cpp )
kde4_add_unit_test( testasxplaylist ${testasxplaylist_SRCS} )
target_link_libraries( testasxplaylist ${KDE4_THREADWEAVER_LIBRARIES} ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} amarokcore amaroklib )

#------------------------ Test M3UPlaylist -----------------------------

set( testm3uplaylist_SRCS m3u/TestM3UPlaylist.cpp )
kde4_add_unit_test( testm3uplaylist ${testm3uplaylist_SRCS} )
target_link_libraries( testm3uplaylist ${KDE4_THREADWEAVER_LIBRARIES} ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} amarokcore amaroklib )

#------------------------ Test PLSPlaylist -----------------------------

set( testplsplaylist_SRCS pls/TestPLSPlaylist.cpp )
kde4_add_unit_test( testplsplaylist ${testplsplaylist_SRCS} )
target_link_libraries( testplsplaylist ${KDE4_THREADWEAVER_LIBRARIES} ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY} amarokcore amaroklib )

#------------------------ Test XSPFPLAYLIST -----------------------------

set( testxspfplaylist_SRCS xspf/TestXSPFPlaylist.cpp )
kde4_add_unit_test( testxspfplaylist ${testxspfplaylist_SRCS} )
target_link_libraries( testxspfplaylist ${KDE4_THREADWEAVER_LIBRARIES} ${KDE4_KDECORE_LIBS} ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY} amarokcore amaroklib )

#------------------------ Test PlaylistFileSupport -----------------------------

set( testplaylistfilesupport_SRCS
     TestPlaylistFileSupport.cpp
     ${AMAROK_SOURCE_TREE}/core-impl/playlists/types/file/PlaylistFileSupport.cpp
     )
kde4_add_unit_test( testplaylistfilesupport ${testplaylistfilesupport_SRCS} )
target_link_libraries( testplaylistfilesupport ${KDE4_KDECORE_LIBS} ${KDE4_KIO_LIBS} ${QT_QTTEST_LIBRARY} ${QT_QTCORE_LIBRARY} amarokcore amaroklib )

