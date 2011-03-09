#==================================================
# LIBCTINY - Matt Pietrek 1996
# Microsoft Systems Journal, October 1996
# FILE: LIBCTINY.MAK - Makefile for Microsoft version
#==================================================

!ifdef DEBUG
OBJ_DIR=objd
!else
OBJ_DIR=obj
!endif

PROJ = LIBCTINY

OBJS =  $(OBJ_DIR)\CONMAIN.OBJ \
#        $(OBJ_DIR)\CRT0TWIN.OBJ $(OBJ_DIR)\DLLCRT0.OBJ \
#        $(OBJ_DIR)\PRINTF.OBJ \
        $(OBJ_DIR)\INITTERM.OBJ $(OBJ_DIR)\ARGCARGV.OBJ \
#        $(OBJ_DIR)\SPRINTF.OBJ $(OBJ_DIR)\PUTS.OBJ $(OBJ_DIR)\STRUPLWR.OBJ \
        $(OBJ_DIR)\ALLOC.OBJ $(OBJ_DIR)\ALLOC2.OBJ $(OBJ_DIR)\ALLOCSUP.OBJ \
#        $(OBJ_DIR)\ISCTYPE.OBJ $(OBJ_DIR)\ATOL.OBJ $(OBJ_DIR)\STRICMP.OBJ \
        $(OBJ_DIR)\TOKENIZE.OBJ $(OBJ_DIR)\PARSETOK.OBJ $(OBJ_DIR)\STRCPY4.OBJ \
#        $(OBJ_DIR)\NEWDEL.OBJ

.SUFFIXES : OBJ

CC = CL
CC_OPTIONS = /c /W3 /DWIN32_LEAN_AND_MEAN

!ifdef DEBUG
CC_OPTIONS = $(CC_OPTIONS) /Zi
!else
CC_OPTIONS = $(CC_OPTIONS) /O1
!endif

$(PROJ).LIB: $(OBJS)
    LIB /OUT:$(OBJ_DIR)\$(PROJ).LIB $(OBJS)

.CPP{$(OBJ_DIR)\}.OBJ:
    $(CC) $(CC_OPTIONS) /Fo$@ $<
