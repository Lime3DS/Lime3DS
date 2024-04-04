# Gets a UTC timstamp and sets the provided variable to it
function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()
get_timestamp(BUILD_DATE)

list(APPEND CMAKE_MODULE_PATH "${SRC_DIR}/externals/cmake-modules")

if (EXISTS "${SRC_DIR}/.git/objects")
    # Find the package here with the known path so that the GetGit commands can find it as well
    find_package(Git QUIET PATHS "${GIT_EXECUTABLE}")

    # only use Git to check revision info when source is obtained via Git
    include(GetGitRevisionDescription)
    get_git_head_revision(GIT_REF_SPEC GIT_REV)
    git_describe(GIT_DESC --always --long --dirty)
    git_branch_name(GIT_BRANCH)
elseif (EXISTS "${SRC_DIR}/GIT-COMMIT" AND EXISTS "${SRC_DIR}/GIT-TAG")
    # unified source archive
    file(READ "${SRC_DIR}/GIT-COMMIT" GIT_REV_RAW LIMIT 64)
    string(STRIP "${GIT_REV_RAW}" GIT_REV)
    string(SUBSTRING "${GIT_REV_RAW}" 0 9 GIT_DESC)
    set(GIT_BRANCH "HEAD")
else()
    # self-packed archive?
    set(GIT_REV "UNKNOWN")
    set(GIT_DESC "UNKNOWN")
    set(GIT_BRANCH "UNKNOWN")
endif()
string(SUBSTRING "${GIT_REV}" 0 7 GIT_SHORT_REV)

# Set build version
set(REPO_NAME "")
set(BUILD_VERSION "0")
set(BUILD_FULLNAME "${GIT_SHORT_REV}")
if (DEFINED ENV{CI} AND DEFINED ENV{GITHUB_ACTIONS})
    if ($ENV{GITHUB_REF_TYPE} STREQUAL "tag")
        set(GIT_TAG $ENV{GITHUB_REF_NAME})
    endif()
elseif (EXISTS "${SRC_DIR}/GIT-COMMIT" AND EXISTS "${SRC_DIR}/GIT-TAG")
    file(READ "${SRC_DIR}/GIT-TAG" GIT_TAG)
    string(STRIP ${GIT_TAG} GIT_TAG)
endif()

if (DEFINED GIT_TAG AND NOT "${GIT_TAG}" STREQUAL "unknown")
    set(BUILD_VERSION "${GIT_TAG}")
    set(BUILD_FULLNAME "${BUILD_VERSION}")
endif()