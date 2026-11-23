#
# Makefile
# Minishell "make" source
# Regenerate msh recompiling only modified sources.
#
# Copyright (c) 1993,2002,2019, Francisco Rosales <frosal@fi.upm.es>
# Todos los derechos reservados.
#
# Publicado bajo Licencia de Proyecto Educativo Práctico
# <http://laurel.datsi.fi.upm.es/~ssoo/LICENCIA/LPEP>
#
# Queda prohibida la difusión total o parcial por cualquier
# medio del material entregado al alumno para la realización 
# de este proyecto o de cualquier material derivado de este, 
# incluyendo la solución particular que desarrolle el alumno.
#
# DO NOT MODIFY THIS FILE
#

CC	= gcc
CFLAGS	= -Wall -g
YACC	= bison --yacc
YFLAGS	= -d
LEX	= flex
LFLAGS	=

SRCDIR	= src
BASEDIR	= $(SRCDIR)/base
OBJDIR	= build
BINDIR	= bin
TARGET	= $(BINDIR)/msh
OBJS	= $(addprefix $(OBJDIR)/,parser.o scanner.o main.o)

all: $(TARGET)

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Ficheros generados por bison y flex (en build/)
$(OBJDIR)/parser.c: $(BASEDIR)/parser.y | $(OBJDIR)
	$(YACC) $(YFLAGS) -b $(OBJDIR)/y $<
	mv -f $(OBJDIR)/y.tab.c $@

$(OBJDIR)/y.tab.h: $(OBJDIR)/parser.c ;

$(OBJDIR)/scanner.c: $(BASEDIR)/scanner.l | $(OBJDIR)
	$(LEX) $(LFLAGS) -t $< > $@

$(OBJDIR)/%.o: $(OBJDIR)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -I$(OBJDIR) -c -o $@ $<

$(OBJDIR)/scanner.o: $(OBJDIR)/y.tab.h

$(OBJDIR)/main.o: $(SRCDIR)/main.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR) $(BINDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f *.bak *~ core

cleanall: clean
	rm -f :* freefds* nofiles* killmyself* sigdfl*

depend:
	makedepend $(SRCDIR)/main.c $(BASEDIR)/parser.y $(BASEDIR)/scanner.l
