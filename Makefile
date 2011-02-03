
NAME	=	jolicloud-displaymanager
VERSION	=	1.0

PREFIX	=	/usr
CFGDIR	=	/etc
DESTDIR	=

SRC	=	main.c			\
		displaymanager.c	\
		datetime.c		\
		config.c		\
		locker.c		\
		log.c			\
		pam.c			\
		session.c		\
		xserver.c		\
		ui.c

OBJ	=	$(SRC:.c=.o)

CC	=	gcc
RM	=	@rm -f

DEFINES	=	-DPACKAGE=\"$(NAME)\"						\
		-DVERSION=\"$(VERSION)\"					\
		-DPKGDATADIR=\"$(PREFIX)/share/jolicloud-displaymanager\"	\
		-DSYSCONFDIR=\"$(CFGDIR)/jolicloud-displaymanager\"

CFLAGS	=	`pkg-config --cflags webkit-1.0` -Wall $(DEFINES)
LDFLAGS	=	`pkg-config --libs webkit-1.0` -lpam


all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ) $(LDFLAGS)

clean:
	$(RM) $(NAME) $(OBJ)

re: clean all

install: $(NAME) install-theme
	install -D -m 755 $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	test -e $(DESTDIR)/$(CFGDIR)/jolicloud-displaymanager/01Standard || \
		install -D -m 644 config/01Standard $(DESTDIR)/$(CFGDIR)/jolicloud-displaymanager/01Standard

dist:
	@rm -rf $(NAME)-$(VERSION)
	@mkdir $(NAME)-$(VERSION)
	@cp -r *.[ch] Makefile config themes $(NAME)-$(VERSION)
	@tar cvzf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)
	@rm -rf $(NAME)-$(VERSION)

install-theme:
	@mkdir -p -m 755 $(DESTDIR)$(PREFIX)/share/jolicloud-displaymanager/themes/default
	install -D -m 644 themes/default/* $(DESTDIR)$(PREFIX)/share/jolicloud-displaymanager/themes/default
