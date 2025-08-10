NAME = ircserv

CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iincludes

SRCDIR = srcs
OBJDIR = objs
INCDIR = includes

SOURCES = main.cpp \
			$(SRCDIR)/Server.cpp \
			$(SRCDIR)/Client.cpp \
			$(SRCDIR)/Channel.cpp \
      	$(SRCDIR)/Command.cpp \
			$(SRCDIR)/CommandHandlers.cpp \
			$(SRCDIR)/IRCProtocol.cpp \
			$(SRCDIR)/utils.cpp

OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJECTS)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME) $(TEST_NAME)

re: fclean all

TEST_NAME = irc_tests
TEST_SOURCES = $(SRCDIR)/Server.cpp \
		  $(SRCDIR)/Client.cpp \
		  $(SRCDIR)/Channel.cpp \
		  $(SRCDIR)/Command.cpp \
		  $(SRCDIR)/CommandHandlers.cpp \
		  $(SRCDIR)/IRCProtocol.cpp \
		  $(SRCDIR)/utils.cpp \
		  tests/test_suite.cpp

TEST_OBJECTS = $(TEST_SOURCES:%.cpp=$(OBJDIR)/%.o)

test: $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TEST_NAME) $(TEST_OBJECTS)
	./$(TEST_NAME)

.PHONY: all clean fclean re test
