all:
	rman -f html    mpexpr.man  >mpexpr.html
	nroff -man      mpexpr.man  >mpexpr.txt
	groff -Tps -man mpexpr.man  >mpexpr.ps
