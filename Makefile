#
#  Dyplo library RTEMS makefile (based on lib makefile template)
#

LIBNAME=libdyplo.a        # xxx- your library names goes here
LIB=${ARCH}/${LIBNAME}

# C and C++ source names, if any, go here -- minus the .c or .cc
C_PIECES=
C_FILES=$(C_PIECES:%=%.c)
C_O_FILES=$(C_PIECES:%=${ARCH}/%.o)

CC_PIECES=pthreadscheduler noopscheduler hardware filequeue fileio
CC_FILES=$(CC_PIECES:%=%.cpp)
CC_O_FILES=$(CC_PIECES:%=${ARCH}/%.o)

H_FILES=

# Assembly source names, if any, go here -- minus the .S
S_PIECES=
S_FILES=$(S_PIECES:%=%.S)
S_O_FILES=$(S_FILES:%.S=${ARCH}/%.o)

SRCS=$(C_FILES) $(CC_FILES) $(H_FILES) $(S_FILES)
OBJS=$(C_O_FILES) $(CC_O_FILES) $(S_O_FILES) $(H_FILES)

include $(RTEMS_MAKEFILE_PATH)/Makefile.inc

include $(RTEMS_CUSTOM)
include lib.cfg

#
# Add local stuff here using +=
#

DEFINES  +=
CPPFLAGS += -DBITSTREAM_DATA_PATH=\"/usr/share/bitstreams\" -Wall -Werror
CFLAGS   +=

#
# Add your list of files to delete here.  The config files
#  already know how to delete some stuff, so you may want
#  to just run 'make clean' first to see what gets missed.
#  'make clobber' already includes 'make clean'
#

CLEAN_ADDITIONS += 
CLOBBER_ADDITIONS +=

all:	$(SRCS) $(LIB)

# $(ARCH) creates the output path for object files
$(OBJS): $(ARCH)/.dirstamp
# Create the output path. Use a "stamp" file because the directory
# date/time stamp changes when a file is added or removed there.
$(ARCH)/.dirstamp:
	test -d $(ARCH) || mkdir -p $(ARCH)
	touch $@

$(LIB): ${OBJS}
	$(make-library)

# Install the library, appending _g or _p as appropriate.
# for include files, just use $(INSTALL_CHANGE)

# Don't install libraries for now
#install:  all
#	$(INSTALL_VARIANT) -m 644 ${LIB} ${PROJECT_RELEASE}/lib
