# ---------------------------------------------------------------------------
# Shader compilation — finds slangc and compiles all .slang files to .spv
# ---------------------------------------------------------------------------

find_program(SLANGC_EXECUTABLE slangc
    HINTS
        "$ENV{SLANG_HOME}/bin"
        "$ENV{ProgramFiles}/slang/bin"
        "/usr/local/bin"
    DOC "Path to the Slang shader compiler (slangc)"
)

if(NOT SLANGC_EXECUTABLE)
    message(WARNING "slangc not found — shader compilation disabled. "
                    "Install Slang (https://shader-slang.com) or set SLANG_HOME.")
    return()
endif()

message(STATUS "slangc found: ${SLANGC_EXECUTABLE}")

# ---------------------------------------------------------------------------
# Shader source directory (relative to CMAKE_SOURCE_DIR)
# ---------------------------------------------------------------------------
set(SHADER_SRC_DIR  "${CMAKE_SOURCE_DIR}/bin/assets/shaders")
set(SHADER_OUT_DIR  "${SHADER_SRC_DIR}")

file(GLOB_RECURSE SLANG_FILES "${SHADER_SRC_DIR}/*.slang")

if(NOT SLANG_FILES)
    message(STATUS "No .slang files found in ${SHADER_SRC_DIR}")
    return()
endif()

# ---------------------------------------------------------------------------
# Entry-point name extraction helper.
# Scans a .slang file for [shader("stage")] and the function name on the
# following line, e.g.:
#   [shader("vertex")]
#   VSOutput vertexMain(VSInput input)
# Returns a list of "stage:entryPoint" pairs.
# ---------------------------------------------------------------------------
function(get_slang_entry_points SLANG_FILE OUT_STAGES)
    file(READ "${SLANG_FILE}" CONTENT)

    # Match [shader("STAGE")]\nRETURN_TYPE ENTRY_NAME(
    # We capture the stage name and the entry-point function name.
    string(REGEX MATCHALL
        "\\[shader\\(\"([^\"]+)\"\\)\\][ \t]*\n[^\n]*[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\("
        MATCHES "${CONTENT}")

    set(STAGES "")
    foreach(M ${MATCHES})
        string(REGEX REPLACE
            "\\[shader\\(\"([^\"]+)\"\\)\\][ \t]*\n[^\n]*[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\(.*"
            "\\1:\\2" PAIR "${M}")
        list(APPEND STAGES "${PAIR}")
    endforeach()

    set(${OUT_STAGES} "${STAGES}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Compile each .slang file
# ---------------------------------------------------------------------------
set(ALL_SPV_FILES "")

foreach(SLANG_FILE ${SLANG_FILES})
    get_filename_component(SHADER_NAME "${SLANG_FILE}" NAME_WE)

    get_slang_entry_points("${SLANG_FILE}" ENTRY_POINTS)

    foreach(PAIR ${ENTRY_POINTS})
        string(REPLACE ":" ";" PARTS "${PAIR}")
        list(GET PARTS 0 STAGE)
        list(GET PARTS 1 ENTRY)

        # Map stage name to slangc -stage argument
        if(STAGE STREQUAL "vertex")
            set(STAGE_ARG "vertex")
            set(STAGE_SUFFIX "vert")
        elseif(STAGE STREQUAL "fragment")
            set(STAGE_ARG "fragment")
            set(STAGE_SUFFIX "frag")
        elseif(STAGE STREQUAL "compute")
            set(STAGE_ARG "compute")
            set(STAGE_SUFFIX "comp")
        else()
            message(WARNING "Unknown shader stage '${STAGE}' in ${SLANG_FILE}")
            continue()
        endif()

        set(SPV_OUT "${SHADER_OUT_DIR}/${SHADER_NAME}.${STAGE_SUFFIX}.spv")

        add_custom_command(
            OUTPUT "${SPV_OUT}"
            COMMAND "${SLANGC_EXECUTABLE}"
                "${SLANG_FILE}"
                -target spirv
                -entry "${ENTRY}"
                -stage "${STAGE_ARG}"
                -o "${SPV_OUT}"
            MAIN_DEPENDENCY "${SLANG_FILE}"
            COMMENT "Compiling ${SHADER_NAME}.slang → ${SHADER_NAME}.${STAGE_SUFFIX}.spv (${STAGE})"
            WORKING_DIRECTORY "${SHADER_SRC_DIR}"
            VERBATIM
        )

        list(APPEND ALL_SPV_FILES "${SPV_OUT}")
    endforeach()
endforeach()

# ---------------------------------------------------------------------------
# Custom target that builds all shaders
# ---------------------------------------------------------------------------
if(ALL_SPV_FILES)
    add_custom_target(shaders ALL
        DEPENDS ${ALL_SPV_FILES}
        COMMENT "All shaders up to date"
    )
    list(LENGTH SLANG_FILES SLANG_COUNT)
    message(STATUS "Shader compilation enabled — ${SLANG_COUNT} .slang file(s) found")
endif()
