# NOTE: mpexpr tests do not use the tcltest framework.
# This file exists just so nmake test works.

# Run tests in separate interpreters since they check if mpexpr is
# statically linked or dynamically loaded.

set ip [interp create]
$ip eval {source mpexpr.test}
interp delete $ip

set ip [interp create]
$ip eval {source mpformat.test}
interp delete $ip

