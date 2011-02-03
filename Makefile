
NAME	=	jolicloud-displaymanager

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
RM	=	rm -f



CFLAGS	=	`pkg-config --cflags webkit-1.0` -Wall
LDFLAGS	=	`pkg-config --libs webkit-1.0` -lpam


all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ) $(LDFLAGS)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all



# install: $(NAME) install-theme
# 	install -D -m 755 $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
# 	test -e $(DESTDIR)/$(CFGDIR)/$(NAME)/ || \
