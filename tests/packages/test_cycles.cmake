# PKG-12: cycle detection over the declared graph (package requires and
# feature requires).
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

# Acyclic: chain, diamond, isolated nodes.
ergo_pkg_rule_cycles(f NODES pictor tela lone EDGES "tela>pictor")
pkg_assert_empty("PKG-12 chain" "${f}")
ergo_pkg_rule_cycles(f NODES a b c d EDGES "a>b" "a>c" "b>d" "c>d")
pkg_assert_empty("PKG-12 diamond" "${f}")
ergo_pkg_rule_cycles(f NODES a b EDGES)
pkg_assert_empty("PKG-12 no edges" "${f}")

# Self loop.
ergo_pkg_rule_cycles(f NODES a EDGES "a>a")
pkg_assert_finding("PKG-12 self loop" "${f}" PKG-12 error a)

# Two-node cycle.
ergo_pkg_rule_cycles(f NODES pictor tela EDGES "pictor>tela" "tela>pictor")
pkg_assert_finding("PKG-12 two nodes" "${f}" PKG-12 error)
if(NOT f MATCHES "pictor -> tela -> pictor|tela -> pictor -> tela")
    pkg_fail("PKG-12 path not reported: ${f}")
endif()

# Three-node cycle with a tail hanging off it and an unrelated acyclic part:
# the reported loop must be the real cycle, not the tail.
ergo_pkg_rule_cycles(f NODES x y z tail free1 free2
    EDGES "x>y" "y>z" "z>x" "z>tail" "free1>free2")
pkg_assert_finding("PKG-12 three nodes" "${f}" PKG-12 error)
if(NOT f MATCHES "x -> y -> z -> x")
    pkg_fail("PKG-12 three-node path: ${f}")
endif()
if(f MATCHES "free1|free2")
    pkg_fail("PKG-12 acyclic nodes reported as involved: ${f}")
endif()

# Feature-level nodes ("<package>/<feature>") cycle through a package.
ergo_pkg_rule_cycles(f NODES pictor
    EDGES "pictor/web>pictor/webgpu" "pictor/webgpu>pictor/web")
pkg_assert_finding("PKG-12 feature cycle" "${f}" PKG-12 error)

# Deterministic: edge order does not change the finding.
ergo_pkg_rule_cycles(f1 NODES EDGES "x>y" "y>z" "z>x")
ergo_pkg_rule_cycles(f2 NODES EDGES "z>x" "y>z" "x>y")
pkg_assert_eq("PKG-12 deterministic" "${f1}" "${f2}")
pkg_pass("PKG-12")
