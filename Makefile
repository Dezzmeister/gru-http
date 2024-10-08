SRC_DIR := src
INC_DIR := src
TEST_SRC_DIR := test/src
TEST_INC_DIR := test/include
TEST_BINARY := test_bin

CC := gcc
CFLAGS := -Wall -Werror -std=gnu17 -pthread
LDFLAGS := -lpthread

HEADERS = \
		${INC_DIR}/ip.h \
		${INC_DIR}/net.h \
		${INC_DIR}/http.h \
		${INC_DIR}/status.h \
		${INC_DIR}/error.h \
		${INC_DIR}/files.h \
		${INC_DIR}/params.h

OBJS = \
		${SRC_DIR}/main.o  \
		${SRC_DIR}/ip.o  \
		${SRC_DIR}/http.o  \
		${SRC_DIR}/status.o  \
		${SRC_DIR}/net.c \
		${SRC_DIR}/error.c \
		${SRC_DIR}/files.c

OBJS_NO_MAIN = $(filter-out ${SRC_DIR}/main.o, ${OBJS})

TEST_HEADERS = \
		${TEST_INC_DIR}/utils.h \
		${TEST_INC_DIR}/setup.h

TEST_OBJS = \
		${TEST_SRC_DIR}/main.o

.PHONY: clean

debug: CFLAGS += -g -Og -fsanitize=unreachable -fsanitize=undefined
debug: LDFLAGS += -lg
release: CFLAGS += -O3 -march=native
test: CFLAGS += -DTEST -fsanitize=unreachable -fsanitize=undefined
memtest: CFLAGS += -g -Og -DTEST -fsanitize=unreachable -fsanitize=undefined
memtest: LDFLAGS += -lg
drdtest: CFLAGS += -g -Og -DTEST -fsanitize=unreachable -fsanitize=undefined
drdtest: LDFLAGS += -lg
massiftest: CFLAGS += -g -Og -DTEST -fsanitize=unreachable -fsanitize=undefined
massiftest: LDFLAGS += -lg
invtest: CFLAGS += -DTEST -fsanitize=unreachable -fsanitize=undefined -DINVERT_EXPECT

debug: ${OBJS}
	${CC} ${LDFLAGS} -o $@ $^ ${CFLAGS}

release: ${OBJS}
	${CC} ${LDFLAGS} -o $@ $^ ${CFLAGS}

test: ${OBJS_NO_MAIN} ${TEST_OBJS}
	${CC} -o ${TEST_BINARY} $^ ${CFLAGS} && ./${TEST_BINARY} ${PATTERN} ; rm -f ./${TEST_BINARY}

memtest: ${OBJS}
	${CC} ${LDFLAGS} -o ${TEST_BINARY} $^ ${CFLAGS} && valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./${TEST_BINARY} ${ARGS} ; rm -f ./${TEST_BINARY}

drdtest: ${OBJS}
	${CC} ${LDFLAGS} -o ${TEST_BINARY} $^ ${CFLAGS} && valgrind --tool=drd --exclusive-threshold=1000 ./${TEST_BINARY} ${ARGS} ; rm -f ./${TEST_BINARY}

massiftest: ${OBJS}
	${CC} ${LDFLAGS} -o ${TEST_BINARY} $^ ${CFLAGS} && valgrind --tool=massif ./${TEST_BINARY} ${ARGS} ; rm -f ./${TEST_BINARY}

invtest: ${OBJS_NO_MAIN} ${TEST_OBJS}
	${CC} -o ${TEST_BINARY} $^ ${CFLAGS} && ./${TEST_BINARY} ${PATTERN} ; rm -f ./${TEST_BINARY}

%.o: %.cpp ${HEADERS} ${TEST_HEADERS}
	${CC} -c -o $@ $< ${CFLAGS}

clean:
	find . -name '*.o' -delete
	rm -f debug
	rm -f release
