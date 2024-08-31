file(GLOB_RECURSE ALL_SOURCE_FILES "./*")
set(modules_src "")

 foreach(file ${ALL_SOURCE_FILES})
     get_filename_component(extension ${file} EXT)
     if(NOT extension STREQUAL "")
        list(APPEND modules_src ${file})
    endif()
 endforeach()

 file(GLOB_RECURSE ALL_INCLUDE_PATHS "./*")
 set(headers_dirs "")

 foreach(path ${ALL_INCLUDE_PATHS})
     get_filename_component(parentDir ${path} DIRECTORY)

     if(IS_DIRECTORY ${parentDir})
         list(APPEND headers_dirs ${parentDir})
     endif()
 endforeach()

idf_component_register(
                    SRCS
                    "${modules_src}"
                    EMBED_FILES modules/web_file_browser/http_server/header.html modules/web_file_browser/http_server/favicon_b.ico
                    modules/web_file_browser/http_server/style.css
                    INCLUDE_DIRS
                    "${headers_dirs}")

# # Append all directories to INCLUDE_DIRS

# file(GLOB_RECURSE ALL_INCLUDE_PATHS "./*")
# set(INCLUDE_DIRS "")

# foreach(path ${ALL_INCLUDE_PATHS})
#     get_filename_component(parentDir ${path} DIRECTORY)

#     if(IS_DIRECTORY ${parentDir})
#         list(APPEND INCLUDE_DIRS ${parentDir})
#     endif()
# endforeach()

# list(REMOVE_DUPLICATES INCLUDE_DIRS)

# # Append all source files to SRCS

# file(GLOB_RECURSE ALL_SOURCE_FILES "./*")
# set(SRCS "")

# foreach(file ${ALL_SOURCE_FILES})
#     get_filename_component(extension ${file} EXT)
#     if(NOT extension STREQUAL "")
#         list(APPEND SRCS ${file})
#     endif()
# endforeach()

# # Register the component

# idf_component_register(
#                     SRCS
#                     ${SRCS}
#                     INCLUDE_DIRS
#                     ${INCLUDE_DIRS})
