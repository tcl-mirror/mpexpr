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
ÄMondoBuildÄ = {MAKEFILE}
Includes     = 	-i "{TclSourceDir}:generic:" -i ""
SymÄPPC      = 
ObjDirÄPPC   =

PPCCOptions  = {Includes} {SymÄPPC} è
		-typecheck relaxed -d MAC_TCL è
		-shared_lib_export on -export_list syms.exp

ObjectsÄPPC  = è
		"{ObjDirÄPPC}mpexpr.c.x" è
		"{ObjDirÄPPC}mpformat.c.x" è
		"{ObjDirÄPPC}mpiface.c.x" è
		"{ObjDirÄPPC}mpparse.c.x" è
		"{ObjDirÄPPC}qfunc.c.x" è
		"{ObjDirÄPPC}qio.c.x" è
		"{ObjDirÄPPC}qmath.c.x" è
		"{ObjDirÄPPC}qmod.c.x" è
		"{ObjDirÄPPC}qtrans.c.x" è
		"{ObjDirÄPPC}zfunc.c.x" è
		"{ObjDirÄPPC}zio.c.x" è
		"{ObjDirÄPPC}zmath.c.x" è
		"{ObjDirÄPPC}zmod.c.x" è
		"{ObjDirÄPPC}zmul.c.x"
		


{PROJECT}.shlb üü {ÄMondoBuildÄ} {ObjectsÄPPC}
	PPCLink è
		-o {Targ} {SymÄPPC} è
		{ObjectsÄPPC} è
		-t 'shlb' è
		-c '????' è
		-xm s è
		-export Mpexpr_Init è
		"{SharedLibraries}InterfaceLib" è
		"{SharedLibraries}StdCLib" è
		"{SharedLibraries}MathLib" è
		"{TclLib}" è
		"{PPCLibraries}StdCRuntime.o" è
		"{PPCLibraries}PPCCRuntime.o" è
		"{PPCLibraries}PPCToolLibs.o"


"{ObjDirÄPPC}mpexpr.c.x" ü {ÄMondoBuildÄ} mpexpr.c
	{PPCC} mpexpr.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}mpformat.c.x" ü {ÄMondoBuildÄ} mpformat.c
	{PPCC} mpformat.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}mpiface.c.x" ü {ÄMondoBuildÄ} mpiface.c
	{PPCC} mpiface.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}mpparse.c.x" ü {ÄMondoBuildÄ} mpparse.c
	{PPCC} mpparse.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}qfunc.c.x" ü {ÄMondoBuildÄ} qfunc.c
	{PPCC} qfunc.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}qio.c.x" ü {ÄMondoBuildÄ} qio.c
	{PPCC} qio.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}qmath.c.x" ü {ÄMondoBuildÄ} qmath.c
	{PPCC} qmath.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}qmod.c.x" ü {ÄMondoBuildÄ} qmod.c
	{PPCC} qmod.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}qtrans.c.x" ü {ÄMondoBuildÄ} qtrans.c
	{PPCC} qtrans.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}zfunc.c.x" ü {ÄMondoBuildÄ} zfunc.c
	{PPCC} zfunc.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}zio.c.x" ü {ÄMondoBuildÄ} zio.c
	{PPCC} zio.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}zmath.c.x" ü {ÄMondoBuildÄ} zmath.c
	{PPCC} zmath.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}zmod.c.x" ü {ÄMondoBuildÄ} zmod.c
	{PPCC} zmod.c -o {Targ} {PPCCOptions}

"{ObjDirÄPPC}zmul.c.x" ü {ÄMondoBuildÄ} zmul.c
	{PPCC} zmul.c -o {Targ} {PPCCOptions}


mpexpr üü {PROJECT}.shlb

