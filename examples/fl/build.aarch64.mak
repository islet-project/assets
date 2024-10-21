#    
#    File: build.mak
#
#    Usage: make -f build.mak CC=g++ EXE=device INC_PATH=/path/to/include/ LIB_PATH=/path/to/lib/ TENSOR_FLOW=ON

SRC_DIR=.
OBJ_DIR=.
EXE_DIR=.
LOCAL_LIB=/usr/lib/aarch64-linux-gnu/

S=.
O= $(OBJ_DIR)
US=.
I=.
INCLUDE=-I$(INC_PATH) -I$(I)

CFLAGS=$(INCLUDE) --static -O2 -g -Wall -Wno-unused-variable -D X64 -Wno-deprecated-declarations
CFLAGS1=$(INCLUDE) -O1 -g -Wall -Wno-unused-variable -D X64 -Wno-deprecated-declarations
CC=$(CC)
LINK=$(CC)
PROTO=protoc
AR=ar

LDFLAGS= -Wl,--no-as-needed -L$(LOCAL_LIB) -lpthread -ltensorflowlite_flex -ltensorflowlite

dobj=	$(O)/$(EXE).o $(O)/socket.o $(O)/word_model.o

all:	$(EXE).exe
clean:
	@echo "removing object files"
	rm $(O)/*.o
	@echo "removing executable file"
	rm $(EXE_DIR)/$(EXE).exe

$(EXE).exe: $(dobj) 
	@echo "linking executable files"
	$(LINK) -o $(EXE_DIR)/$(EXE).exe $(dobj) $(LDFLAGS)

$(O)/$(EXE).o: $(S)/$(EXE).cc
	@echo "compiling certifier.cc"
	$(CC) $(CFLAGS) -c -o $(O)/$(EXE).o $(S)/$(EXE).cc

$(O)/socket.o: $(S)/socket.cc
	@echo "compiling socket.cc"
	$(CC) $(CFLAGS) -c -o $(O)/socket.o $(S)/socket.cc

$(O)/word_model.o: $(S)/word_model.cc
	@echo "compiling word_model.cc"
	$(CC) $(CFLAGS) -c -o $(O)/word_model.o $(S)/word_model.cc

