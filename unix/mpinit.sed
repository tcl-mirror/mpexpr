/if.*Tcl[X]*_Init.*interp.*==.*TCL_ERROR/{
n
n
n
i\
\
\ \ \ \ if (Mpexpr_Init (interp) == TCL_ERROR) {\
\ \ \ \     return TCL_ERROR;\
\ \ \ \ }\
\ \ \ \ Tcl_StaticPackage (interp, "Mpexpr", Mpexpr_Init, (Tcl_PackageInitProc *) NULL);

}
