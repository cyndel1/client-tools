#
# STLport makefile for VC++ 7.1 (.NET)
#

CXX=cl.exe
CC=cl.exe
RC=rc

LIB_BASENAME=stlport_vc71
COMP=VC71

# EXTRA_COMMON_FLAGS=/D "_MBCS"
EXTRA_COMMON_FLAGS=/FI "vc_warning_disable.h" /D "_MBCS"
EXTRA_DEBUG_FLAGS=
EXTRA_NDEBUG_FLAGS=

all: platform all_static all_dynamic 

!INCLUDE vc_common.mak

#
#



