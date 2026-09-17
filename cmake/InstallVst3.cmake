# Copies the single-file VST3 into the Studio One scan folder after a build.
# Invoked as: cmake -DSRC=<built dll> -DDST=<install path> -P InstallVst3.cmake
#
# A DAW that currently has the plugin loaded keeps the DLL locked. In that
# case the copy fails; we warn instead of failing the whole build so the
# Standalone and the build tree stay usable. Close the DAW and rebuild (or run
# the copy again) to update the installed plugin.

get_filename_component(dstDir "${DST}" DIRECTORY)
file(MAKE_DIRECTORY "${dstDir}")

# A stale bundle *folder* with the same name would shadow the file.
if(IS_DIRECTORY "${DST}")
    file(REMOVE_RECURSE "${DST}")
endif()

file(COPY_FILE "${SRC}" "${DST}" RESULT copyResult ONLY_IF_DIFFERENT)

if(copyResult)
    message(WARNING
        "Could not update ${DST}\n"
        "  (${copyResult})\n"
        "  The DLL is probably loaded by Studio One. Close the DAW and rebuild.")
else()
    message(STATUS "Installed VST3 -> ${DST}")
endif()
