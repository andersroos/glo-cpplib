MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
BASE_DIR := $(realpath $(CURDIR)/$(MAKEFILE_DIR))
INCLUDE_DIR := $(BASE_DIR)/include
DOCKER_USER := ygram

#
# Settings
#

# Override

-include local.mk

#
# Build vars
#

CXXFLAGS = -fPIC -g -O2 -DDEFAULT_DB=\"$(DEFAULT_DB)\" -I$(INCLUDE_DIR) -std=c++14 -Wall

LIBS =

TEST_LIBS = -lboost_unit_test_framework

TEST_OBJS = \
	test/run_tests.o \
	test/group_format_lock_test.o \
	test/group_format_test.o


default: examples test


test: $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o ./run-tests $(TEST_OBJS) $(TEST_LIBS) $(LIBS)
	./run-tests

examples:
	make -j -C examples

clean:
	make -C examples clean
	\rm -f include/*.o run-tests

todo:
	@grep -irn todo | grep -v -E -e '(\.git|Makefile)' -e .idea | sort; echo ""


depend:
	makedepend -Y -Iinclude test/*.cpp

docker-build:
	docker build dist/test -t $(DOCKER_USER)/glo:cpplib-test

docker-push:
	docker push $(DOCKER_USER)/glo:cpplib-test

docker-pull:
	docker pull -a $(DOCKER_USER)/glo

docker-test:
	docker run -v $$(pwd):/src -i -t ygram/glo:cpplib-test /bin/sh -c 'cd /src && make clean && make -j  examples  test'

.PHONY: depend default test examples clean todo

# DO NOT DELETE

test/group_format_lock_test.o: include/glo/status.hpp include/glo/json.hpp
test/group_format_test.o: include/glo/status.hpp include/glo/json.hpp
