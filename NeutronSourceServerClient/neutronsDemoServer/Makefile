#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure

DIRS += src
src_DEPEND_DIRS = configure

DIRS += srcIoc
srcIoc_DEPEND_DIRS = src

DIRS += iocBoot

include $(TOP)/configure/RULES_TOP


