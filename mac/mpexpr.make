# Mpexpr - make ppc shared lib for mac

# define PROJECT, project_VERSION, and DLL_VERSION for use by install.tcl

MPEXPR_VERSION = 1.0
DLL_VERSION = 10
PROJECT = mpexpr{DLL_VERSION}

DriveName    =  dogbert
TclLibDir    = {DriveName}:System Folder:Extensions:Tool Command Language
TclLib       = {TclLibDir}:Tcl8.0.shlb
TclSourceDir = {DriveName}:Tcl/Tk Folder:tcl8.0


MAKEFILE     = mpexpr.make
�MondoBuild� = {MAKEFILE}
Includes     = 	-i "{TclSourceDir}:generic:" -i ""
Sym�PPC      = 
ObjDir�PPC   =

PPCCOptions  = {Includes} {Sym�PPC} �
		-typecheck relaxed -d MAC_TCL �
		-shared_lib_export on -export_list syms.exp

Objects�PPC  = �
		"{ObjDir�PPC}mpexpr.c.x" �
		"{ObjDir�PPC}mpformat.c.x" �
		"{ObjDir�PPC}mpiface.c.x" �
		"{ObjDir�PPC}mpparse.c.x" �
		"{ObjDir�PPC}qfunc.c.x" �
		"{ObjDir�PPC}qio.c.x" �
		"{ObjDir�PPC}qmath.c.x" �
		"{ObjDir�PPC}qmod.c.x" �
		"{ObjDir�PPC}qtrans.c.x" �
		"{ObjDir�PPC}zfunc.c.x" �
		"{ObjDir�PPC}zio.c.x" �
		"{ObjDir�PPC}zmath.c.x" �
		"{ObjDir�PPC}zmod.c.x" �
		"{ObjDir�PPC}zmul.c.x"
		


{PROJECT}.shlb �� {�MondoBuild�} {Objects�PPC}
	PPCLink �
		-o {Targ} {Sym�PPC} �
		{Objects�PPC} �
		-t 'shlb' �
		-c '????' �
		-xm s �
		-export Mpexpr_Init �
		"{SharedLibraries}InterfaceLib" �
		"{SharedLibraries}StdCLib" �
		"{SharedLibraries}MathLib" �
		"{TclLib}" �
		"{PPCLibraries}StdCRuntime.o" �
		"{PPCLibraries}PPCCRuntime.o" �
		"{PPCLibraries}PPCToolLibs.o"


"{ObjDir�PPC}mpexpr.c.x" � {�MondoBuild�} mpexpr.c
	{PPCC} mpexpr.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}mpformat.c.x" � {�MondoBuild�} mpformat.c
	{PPCC} mpformat.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}mpiface.c.x" � {�MondoBuild�} mpiface.c
	{PPCC} mpiface.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}mpparse.c.x" � {�MondoBuild�} mpparse.c
	{PPCC} mpparse.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}qfunc.c.x" � {�MondoBuild�} qfunc.c
	{PPCC} qfunc.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}qio.c.x" � {�MondoBuild�} qio.c
	{PPCC} qio.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}qmath.c.x" � {�MondoBuild�} qmath.c
	{PPCC} qmath.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}qmod.c.x" � {�MondoBuild�} qmod.c
	{PPCC} qmod.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}qtrans.c.x" � {�MondoBuild�} qtrans.c
	{PPCC} qtrans.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}zfunc.c.x" � {�MondoBuild�} zfunc.c
	{PPCC} zfunc.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}zio.c.x" � {�MondoBuild�} zio.c
	{PPCC} zio.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}zmath.c.x" � {�MondoBuild�} zmath.c
	{PPCC} zmath.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}zmod.c.x" � {�MondoBuild�} zmod.c
	{PPCC} zmod.c -o {Targ} {PPCCOptions}

"{ObjDir�PPC}zmul.c.x" � {�MondoBuild�} zmul.c
	{PPCC} zmul.c -o {Targ} {PPCCOptions}


mpexpr �� {PROJECT}.shlb

