#
# 3 proxy common Makefile
#

all:	$(BUILDDIR)proxy$(EXESUFFICS)


sockmap$(OBJSUFFICS): sockmap.c proxy.h structures.h
	$(CC) $(CFLAGS) sockmap.c

common$(OBJSUFFICS): common.c proxy.h structures.h
	$(CC) $(CFLAGS) common.c

base64$(OBJSUFFICS): base64.c
	$(CC) $(CFLAGS) base64.c

ftp$(OBJSUFFICS): ftp.c proxy.h structures.h
	$(CC) $(CFLAGS) ftp.c

#$(COMPATLIBS): 
#	$(CC) $(CFLAGS) strncasecmp.c

sockgetchar$(OBJSUFFICS): sockgetchar.c proxy.h structures.h
	$(CC) $(CFLAGS) sockgetchar.c

proxy$(OBJSUFFICS): proxy.c proxy.h structures.h proxymain.c
	$(CC) $(CFLAGS) $(DEFINEOPTION)NOPORTMAP $(DEFINEOPTION)ANONYMOUS proxy.c

$(BUILDDIR)proxy$(EXESUFFICS): sockmap$(OBJSUFFICS) proxy$(OBJSUFFICS) sockgetchar$(OBJSUFFICS) common$(OBJSUFFICS) log$(OBJSUFFICS) base64$(OBJSUFFICS) ftp$(OBJSUFFICS) $(COMPATLIBS)
	$(LN) $(LNOUT)$(BUILDDIR)proxy$(EXESUFFICS) $(LDFLAGS) sockmap$(OBJSUFFICS) proxy$(OBJSUFFICS) sockgetchar$(OBJSUFFICS) log$(OBJSUFFICS) common$(OBJSUFFICS) base64$(OBJSUFFICS) ftp$(OBJSUFFICS) $(COMPATLIBS) $(LIBS)

log$(OBJSUFFICS): log.c proxy.h structures.h
	$(CC) $(COUT)log$(OBJSUFFICS) $(CFLAGS) log.c
