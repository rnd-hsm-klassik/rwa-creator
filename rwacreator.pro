TEMPLATE = app
QT += widgets network multimedia concurrent serialport bluetooth
DEFINES += "PUREDATA"
DEFINES += "PD"
DEFINES += "USINGQT"

# Version information
VERSION = 0.8.7

# the string ${QMAKE_TARGET_BUNDLE_PREFIX}.${QMAKE_BUNDLE}.plist should not overlap with the
# qt generated settings file-name (com.fhnw.rwacreator.plist, see main.cpp)
QMAKE_TARGET_BUNDLE_PREFIX = com.fhnw.rwa.creator
QMAKE_BUNDLE = rwacreator

# Display name of the final .app bundle (kept separate from QMAKE_BUNDLE/TARGET,
# which stay as the internal "rwacreator" name to avoid threading a space
# through every build path and linked binary name).
APP_BUNDLE_NAME = "RWA Creator"

# =============================================================================
# Info.plist
# =============================================================================
ICON = images/rwa-creator.icns

# qmake assembles a bundle identifier from ${QMAKE_TARGET_BUNDLE_PREFIX}.${QMAKE_BUNDLE}
# since QMAKE_BUNDLE determines the binary name, it shouldn't be changed
# but to align the bundle identifier with the upstream version, we should use
# the same BUNDLE_IDENTIFIER as that version:
BUNDLE_IDENTIFIER = com.fhnw.rwa.creator

# GitCommitHash has no native qmake plist token, so template it in with
# QMAKE_SUBSTITUTES (Info.plist.in -> build dir). qmake then resolves the
# remaining @...@ / ${...} tokens when it installs QMAKE_INFO_PLIST.
GIT_COMMIT_HASH = $$system(git -C $$PWD rev-parse --short HEAD)
infoplist.input  = Info.plist.in
infoplist.output = $$OUT_PWD/Info.plist
QMAKE_SUBSTITUTES += infoplist

QMAKE_INFO_PLIST = $$OUT_PWD/Info.plist

# =============================================================================
# Compiler & language
# =============================================================================
QMAKE_CC=clang
QMAKE_CXX=clang++
CONFIG += c++17

# qDebug(), qInfo(), and qWarning() are debugging tools.
# They can be compiled away by defining QT_NO_DEBUG_OUTPUT, QT_NO_INFO_OUTPUT,
# or QT_NO_WARNING_OUTPUT during compilation.
# CONFIG(release):DEFINES += QT_NO_DEBUG_OUTPUT

QMAKE_RPATHDIR += @executable_path/../Frameworks

# Set minimum macOS version for Intel Macs running Big Sur/Monterey
QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0

# =============================================================================
# Include paths
# =============================================================================
INCLUDEPATH += /usr/local/include
INCLUDEPATH += $$PWD/vas_library/source
INCLUDEPATH += $$PWD/vas_library/examples/PureData
INCLUDEPATH += $$PWD/libpd/libpd_wrapper
INCLUDEPATH += $$PWD/libpd/pure-data/src
INCLUDEPATH += portaudio/include
INCLUDEPATH += $$PWD/speech_tools/include
INCLUDEPATH += $$PWD/ofq/libofqf
INCLUDEPATH += $$PWD/libogg/include/
INCLUDEPATH += $$PWD/vorbis/include/
INCLUDEPATH += $$PWD/vorbis/lib/
INCLUDEPATH += $$PWD/cpp-httplib
INCLUDEPATH += $$PWD/taglib
INCLUDEPATH += $$PWD/taglib/taglib
INCLUDEPATH += $$PWD/taglib/taglib/toolkit
INCLUDEPATH += $$PWD/taglib/taglib/ogg
INCLUDEPATH += $$PWD/taglib/3rdparty
INCLUDEPATH += $$PWD/taglib/build

# =============================================================================
# Libraries
# =============================================================================
LIBS += -framework AudioToolbox \
        -framework AudioUnit \
        -framework CoreAudio \
        -framework CoreServices \
        -framework Accelerate \
        -framework Carbon

LIBS += -ltermcap
LIBS += -lcurses
LIBS += -lncurses
LIBS += -lz
LIBS += -L$$PWD/taglib/build/taglib -ltag
LIBS += -L$$PWD/portaudio/lib/.libs/ -lportaudio.2
LIBS += -L$$PWD/libpd/libs/ -lpd

# =============================================================================
# Precompiled dependencies (portaudio, libpd, taglib)
# =============================================================================
extralib.target = extra
extralib.commands = echo "Precompiling portaudio, libpd and taglib..."; \
    $$PWD/makeportaudio.sh ; \
    $$PWD/makelibpd.sh ; \
    $$PWD/maketaglib.sh
extralib.depends =

QMAKE_EXTRA_TARGETS += extralib
PRE_TARGETDEPS = extra

# =============================================================================
# Sources & headers
# =============================================================================
include($$PWD/qmapcontrol/QMapControl/QMapControl.pri)

HEADERS += \
    libogg/include/ogg/config_types.h.in \
    libogg/include/ogg/ogg.h \
    libogg/include/ogg/os_types.h \
    lo/lo.h \
    rwaasset1.h \
    rwaheadtrackerconnect.h \
    rwahistory.h \
    rwainputdialog.h \
    rwalocation1.h \
    rwapdextra~.h \
    rwaruntime.h \
    rwasearchdialog.h \
    rwatoolbar.h \
    rwaview.h \
    rwascene.h \
    rwastate.h \
    rwabackend.h \
    rwamapview.h \
    rwasearchbox.h \
    rwasuggestplaces.h \
    rwaassetlist.h \
    rwautilities.h \
    rwaexport.h \
    rwaimport.h \
    rwaaudiooutput.h \
    pawrapper.h \
    rwaattribute.h \
    rwaentity.h \
    rwasimulator.h \
    rwacreator.h \
    rwastyles.h \
    rwagraphicsview.h \
    $$PWD/ofq/libofqf/qoscclient.h \
    $$PWD/ofq/libofqf/qoscserver.h \
    $$PWD/ofq/libofqf/qosctypes.h \
    rwalistview.h \
    rwastatelist.h \
    rwasceneview.h \
    rwaattributeview.h \
    rwastateattributeview.h \
    rwaassetattributeview.h \
    rwalogview.h \
    rwagameview.h \
    rwascenelist.h \
    rwasceneattributeview.h \
    rwaarea.h \
    bluetooth/characteristicinfo.h \
    bluetooth/device.h \
    bluetooth/deviceinfo.h \
    bluetooth/serviceinfo.h \
    rwastateview.h \
    clipper/clipper.hpp \
    vas_library/examples/PureData/m_pd.h \
    vas_library/examples/PureData/rwa_binauralsimple~.h \
    vas_library/source/vas_dynamicFirChannel.h \
    vas_library/source/vas_fir.h \
    vas_library/source/vas_fir_binaural.h \
    vas_library/source/vas_fir_list.h \
    vas_library/source/vas_fir_read.h \
    vas_library/source/vas_firobject.h \
    vas_library/source/vas_gps_util.h \
    vas_library/source/vas_mem.h \
    vas_library/source/vas_pdmaxobject.h \
    vas_library/source/vas_util.h \
    vorbis/include/vorbis/codec.h \
    vorbis/include/vorbis/vorbisenc.h \
    vorbis/include/vorbis/vorbisfile.h \
    vorbis/lib/backends.h \
    vorbis/lib/bitrate.h \
    vorbis/lib/books/coupled/res_books_51.h \
    vorbis/lib/books/coupled/res_books_stereo.h \
    vorbis/lib/books/floor/floor_books.h \
    vorbis/lib/books/uncoupled/res_books_uncoupled.h \
    vorbis/lib/codebook.h \
    vorbis/lib/codec_internal.h \
    vorbis/lib/envelope.h \
    vorbis/lib/highlevel.h \
    vorbis/lib/lookup.h \
    vorbis/lib/lookup_data.h \
    vorbis/lib/lpc.h \
    vorbis/lib/lsp.h \
    vorbis/lib/masking.h \
    vorbis/lib/mdct.h \
    vorbis/lib/misc.h \
    vorbis/lib/modes/floor_all.h \
    vorbis/lib/modes/psych_11.h \
    vorbis/lib/modes/psych_16.h \
    vorbis/lib/modes/psych_44.h \
    vorbis/lib/modes/psych_8.h \
    vorbis/lib/modes/residue_16.h \
    vorbis/lib/modes/residue_44.h \
    vorbis/lib/modes/residue_44p51.h \
    vorbis/lib/modes/residue_44u.h \
    vorbis/lib/modes/residue_8.h \
    vorbis/lib/modes/setup_11.h \
    vorbis/lib/modes/setup_16.h \
    vorbis/lib/modes/setup_22.h \
    vorbis/lib/modes/setup_32.h \
    vorbis/lib/modes/setup_44.h \
    vorbis/lib/modes/setup_44p51.h \
    vorbis/lib/modes/setup_44u.h \
    vorbis/lib/modes/setup_8.h \
    vorbis/lib/modes/setup_X.h \
    vorbis/lib/os.h \
    vorbis/lib/psy.h \
    vorbis/lib/registry.h \
    vorbis/lib/scales.h \
    vorbis/lib/smallft.h \
    vorbis/lib/window.h

SOURCES += main.cpp \
    libogg/src/bitwise.c \
    libogg/src/framing.c \
    oggread~.c \
    pd-extra/pd/externals/freeverb~/freeverb~.c \
    pd-extra/pd/externals/pdogg/oggwrite~.c \
    rwaasset1.cpp \
    rwaheadtrackerconnect.cpp \
    rwahistory.cpp \
    rwainputdialog.cpp \
    rwalocation1.cpp \
    rwaruntime.cpp \
    rwasearchdialog.cpp \
    rwatoolbar.cpp \
    rwaview.cpp \
    rwascene.cpp \
    rwastate.cpp \
    rwabackend.cpp \
    rwamapview.cpp \
    rwasearchbox.cpp \
    rwasuggestplaces.cpp \
    rwaassetlist.cpp \
    rwautilities.cpp \
    rwaexport.cpp \
    rwaimport.cpp \
    rwaaudiooutput.cpp \
    pawrapper.cpp \
    rwaattribute.cpp \
    rwaentity.cpp \
    rwasimulator.cpp \
    rwacreator.cpp \
    rwagraphicsview.cpp \
    $$PWD/ofq/libofqf/qoscclient.cpp \
    $$PWD/ofq/libofqf/qoscserver.cpp \
    $$PWD/ofq/libofqf/qosctypes.cpp \
    rwalistview.cpp \
    rwastatelist.cpp \
    rwasceneview.cpp \
    rwaattributeview.cpp \
    rwastateattributeview.cpp \
    rwaassetattributeview.cpp \
    rwalogview.cpp \
    rwagameview.cpp \
    rwascenelist.cpp \
    rwasceneattributeview.cpp \
    rwaarea.cpp \
    bluetooth/characteristicinfo.cpp \
    bluetooth/device.cpp \
    bluetooth/deviceinfo.cpp \
    bluetooth/serviceinfo.cpp \
    rwastateview.cpp \
    clipper/clipper.cpp \
    vas_library/examples/PureData/rwa_binauralsimple~.c \
    vas_library/source/vas_dynamicFirChannel.c \
    vas_library/source/vas_fir.c \
    vas_library/source/vas_fir_binaural.c \
    vas_library/source/vas_fir_list.c \
    vas_library/source/vas_fir_read.c \
    vas_library/source/vas_firobject.c \
    vas_library/source/vas_gps_util.c \
    vas_library/source/vas_mem.c \
    vas_library/source/vas_pdmaxobject.c \
    vas_library/source/vas_util.c \
    vorbis/lib/analysis.c \
    vorbis/lib/bitrate.c \
    vorbis/lib/block.c \
    vorbis/lib/codebook.c \
    vorbis/lib/envelope.c \
    vorbis/lib/floor0.c \
    vorbis/lib/floor1.c \
    vorbis/lib/info.c \
    vorbis/lib/lookup.c \
    vorbis/lib/lpc.c \
    vorbis/lib/lsp.c \
    vorbis/lib/mapping0.c \
    vorbis/lib/mdct.c \
    vorbis/lib/misc.c \
    vorbis/lib/psy.c \
    vorbis/lib/registry.c \
    vorbis/lib/res0.c \
    vorbis/lib/sharedbook.c \
    vorbis/lib/smallft.c \
    vorbis/lib/synthesis.c \
    vorbis/lib/vorbisenc.c \
    vorbis/lib/vorbisfile.c \
    vorbis/lib/window.c

# =============================================================================
# Post-link fixups
# =============================================================================
# Fix library install names to use @executable_path/../Frameworks
QMAKE_POST_LINK += install_name_tool -change /usr/local/lib/libportaudio.2.dylib @executable_path/../Frameworks/libportaudio.2.dylib $$OUT_PWD/rwacreator.app/Contents/MacOS/rwacreator $$escape_expand(\\n\\t)
QMAKE_POST_LINK += install_name_tool -change libs/libpd.dylib @executable_path/../Frameworks/libpd.dylib $$OUT_PWD/rwacreator.app/Contents/MacOS/rwacreator $$escape_expand(\\n\\t)

# =============================================================================
# Bundle assembly (copy resources, deploy Qt frameworks, rename bundle)
# =============================================================================
copydata.depends = all
copydata.commands = test -d $$OUT_PWD/rwacreator.app/Contents/Resources/puredata || mkdir -p $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& test -d $$OUT_PWD/rwacreator.app/Contents/Resources/images || mkdir -p $$OUT_PWD/rwacreator.app/Contents/Resources/images \
&& test -d $$OUT_PWD/rwacreator.app/Contents/Frameworks || mkdir -p $$OUT_PWD/rwacreator.app/Contents/Frameworks \
&& $(COPY_FILE) $$PWD/libpd/libs/libpd.dylib $$OUT_PWD/rwacreator.app/Contents/Frameworks \
&& $(COPY_FILE) $$PWD/portaudio/lib/.libs/libportaudio.2.dylib $$OUT_PWD/rwacreator.app/Contents/Frameworks \
&& $(COPY_DIR) $$PWD/images/* $$OUT_PWD/rwacreator.app/Contents/Resources/images \
&& $(COPY_FILE) $$PWD/puredata/fabian_dir256.txt $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaloopplayermono.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaloopplayerstereo.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaloopplayerstereoogg.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaloopplayermonoogg.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaplayer5_1channelbinaural_fabian.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaplayer7channelbinaural_fabian.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaplayermonobinaural_fabian.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaplayermonobinauralogg_fabian.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaplayerstereobinauralogg_fabian.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaplayermonobrir1.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/rwaplayerstereobinaural_fabian.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata \
&& $(COPY_FILE) $$PWD/puredata/stereoout.pd $$OUT_PWD/rwacreator.app/Contents/Resources/puredata

first.depends = copydata
export(first.depends)
export(copydata.depends)
export(copydata.commands)

QMAKE_EXTRA_TARGETS += first copydata

# Deploy Qt frameworks (release only) and rename the bundle to its display
# name (debug and release), after copydata has populated Frameworks/Resources.
CONFIG(release, debug|release) {
    deployqt.target = deployqt
    deployqt.depends = copydata
    deployqt.commands = $$[QT_INSTALL_BINS]/macdeployqt \"$$OUT_PWD/rwacreator.app\"
    export(deployqt.target)
    export(deployqt.depends)
    export(deployqt.commands)
    QMAKE_EXTRA_TARGETS += deployqt
    renamebundle.depends = deployqt
} else {
    renamebundle.depends = copydata
}
export(renamebundle.depends)
renamebundle.target = renamebundle
renamebundle.commands = rm -rf \"$$OUT_PWD/$${APP_BUNDLE_NAME}.app\" \
    && mv \"$$OUT_PWD/rwacreator.app\" \"$$OUT_PWD/$${APP_BUNDLE_NAME}.app\"
export(renamebundle.target)
export(renamebundle.commands)
first.depends += renamebundle
export(first.depends)
QMAKE_EXTRA_TARGETS += renamebundle

# =============================================================================
# INSTALLS ("make install" target — not invoked by build_release.sh /
# build_debug.sh today; kept as-is pending investigation)
# =============================================================================
target.path = $$PWD/build/
INSTALLS += target
