mod_gsgi.la: mod_gsgi.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_gsgi.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_gsgi.la
