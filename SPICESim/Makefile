PARSER_DIR=./parser

all: sparser

sparser: $(PARSER_DIR)/main.cpp
	g++ -std=c++11 -O3 -o $(PARSER_DIR)/sparser $(PARSER_DIR)/main.cpp

clean:
	rm -f $(PARSER_DIR)/sparser
