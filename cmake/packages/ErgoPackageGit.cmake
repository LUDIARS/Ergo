# ---------------------------------------------------------------------------
# ErgoPackageGit.cmake — read-only git probes of a package checkout
#
# One responsibility: ask git about a source directory and hand the facts
# back (HEAD, pin ancestry, dirty, expect_tag ancestry, top level). Nothing
# here judges the facts — ErgoPackageRulesAcquire.cmake does — and nothing
# writes to the checkout. Without git, or outside a repository, every probe
# answers "" / FALSE so the rules report drift `unknown` (PKG-09).
#
#   ergo_pkg_git_probe(<prefix> <dir> <pin> <expect_tag> [ROOT_ONLY])
#     ROOT_ONLY: <dir> must itself be the repository root. A plain source
#     tree that merely sits inside another repository (a copy under a build
#     directory) then reads as "not a repository" instead of borrowing the
#     enclosing repository's HEAD.
#     <prefix>_TOPLEVEL        repository root ("" when not a repository)
#     <prefix>_HEAD            40-digit HEAD ("" when unreadable)
#     <prefix>_PIN_KNOWN       the pin commit exists in the checkout
#     <prefix>_PIN_IS_ANCESTOR pin is an ancestor of HEAD
#     <prefix>_HEAD_IS_ANCESTOR HEAD is an ancestor of the pin
#     <prefix>_DIRTY           tracked files differ from HEAD
#     <prefix>_TAG_FOUND       expect_tag exists in the checkout and can be
#                              judged (pin present, history not shallow)
#     <prefix>_PIN_DESCENDS    the pin descends from expect_tag
# ---------------------------------------------------------------------------
include_guard(GLOBAL)

# Internal: run git in <dir>; <out_rc> is the exit code (-1 without git).
function(_ergo_pkg_git out_rc out_text dir)
    if(NOT GIT_EXECUTABLE)
        find_package(Git QUIET)
    endif()
    if(NOT GIT_EXECUTABLE)
        set(${out_rc} -1 PARENT_SCOPE)
        set(${out_text} "" PARENT_SCOPE)
        return()
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${dir}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(${out_rc} "${_rc}" PARENT_SCOPE)
    set(${out_text} "${_out}" PARENT_SCOPE)
endfunction()

# Internal: TRUE when `git merge-base --is-ancestor <a> <b>` succeeds.
function(_ergo_pkg_git_is_ancestor out_var dir a b)
    _ergo_pkg_git(_rc _out "${dir}" merge-base --is-ancestor "${a}" "${b}")
    if(_rc EQUAL 0)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(ergo_pkg_git_probe prefix dir pin expect_tag)
    set(_top "")
    set(_head "")
    set(_pin_known FALSE)
    set(_pin_anc FALSE)
    set(_head_anc FALSE)
    set(_dirty FALSE)
    set(_tag_found FALSE)
    set(_descends FALSE)

    if(IS_DIRECTORY "${dir}")
        _ergo_pkg_git(_rc _out "${dir}" rev-parse --show-toplevel)
        if(_rc EQUAL 0 AND "ROOT_ONLY" IN_LIST ARGN)
            file(REAL_PATH "${dir}" _want)
            file(REAL_PATH "${_out}" _got)
            if(NOT _want STREQUAL _got)
                set(_rc 1)
            endif()
        endif()
        if(_rc EQUAL 0)
            set(_top "${_out}")
            _ergo_pkg_git(_rc _out "${dir}" rev-parse --verify -q HEAD)
            if(_rc EQUAL 0)
                set(_head "${_out}")
            endif()
        endif()
    endif()

    if(NOT _head STREQUAL "")
        # A shallow history (GIT_SHALLOW fetch) cannot prove ancestry: only
        # HEAD == pin is judged there, the rest stays unknown.
        _ergo_pkg_git(_rc _out "${dir}" rev-parse --is-shallow-repository)
        set(_shallow FALSE)
        if(_rc EQUAL 0 AND _out STREQUAL "true")
            set(_shallow TRUE)
        endif()
        if(NOT "${pin}" STREQUAL "" AND NOT _shallow)
            _ergo_pkg_git(_rc _out "${dir}" cat-file -e "${pin}^{commit}")
            if(_rc EQUAL 0)
                set(_pin_known TRUE)
                _ergo_pkg_git_is_ancestor(_pin_anc "${dir}" "${pin}" "${_head}")
                _ergo_pkg_git_is_ancestor(_head_anc "${dir}" "${_head}" "${pin}")
            endif()
        endif()
        _ergo_pkg_git(_rc _out "${dir}" status --porcelain --untracked-files=no)
        if(_rc EQUAL 0 AND NOT _out STREQUAL "")
            set(_dirty TRUE)
        endif()
        if(NOT "${expect_tag}" STREQUAL "" AND NOT _shallow)
            _ergo_pkg_git(_rc _out "${dir}" rev-parse --verify -q "refs/tags/${expect_tag}^{commit}")
            if(_rc EQUAL 0)
                set(_tag_found TRUE)
                if(_pin_known)
                    _ergo_pkg_git_is_ancestor(_descends "${dir}" "${_out}" "${pin}")
                else()
                    # Pin not in the checkout: the relation is not judged.
                    set(_tag_found FALSE)
                endif()
            endif()
        endif()
    endif()

    set(${prefix}_TOPLEVEL "${_top}" PARENT_SCOPE)
    set(${prefix}_HEAD "${_head}" PARENT_SCOPE)
    set(${prefix}_PIN_KNOWN ${_pin_known} PARENT_SCOPE)
    set(${prefix}_PIN_IS_ANCESTOR ${_pin_anc} PARENT_SCOPE)
    set(${prefix}_HEAD_IS_ANCESTOR ${_head_anc} PARENT_SCOPE)
    set(${prefix}_DIRTY ${_dirty} PARENT_SCOPE)
    set(${prefix}_TAG_FOUND ${_tag_found} PARENT_SCOPE)
    set(${prefix}_PIN_DESCENDS ${_descends} PARENT_SCOPE)
endfunction()
