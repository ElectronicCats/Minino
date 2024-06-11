# Append all directories to INCLUDE_DIRS

file(GLOB_RECURSE ALL_INCLUDE_PATHS "./*")
set(INCLUDE_DIRS "")

foreach(path ${ALL_INCLUDE_PATHS})
    get_filename_component(parentDir ${path} DIRECTORY)

    if(IS_DIRECTORY ${parentDir})
        list(APPEND INCLUDE_DIRS ${parentDir})
    endif()
endforeach()

list(REMOVE_DUPLICATES INCLUDE_DIRS)

# Append all source files to SRCS

file(GLOB_RECURSE ALL_SOURCE_FILES "./*")
set(SRCS "")

foreach(file ${ALL_SOURCE_FILES})
    get_filename_component(extension ${file} EXT)
    if(NOT extension STREQUAL "")
        list(APPEND SRCS ${file})
    endif()
endforeach()

# Register the component

idf_component_register(
                    SRCS
                    ${SRCS}
                    INCLUDE_DIRS
                    ${INCLUDE_DIRS})
