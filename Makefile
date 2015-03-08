.PHONY: all test clean cleanall
	
MAIN_DIR=.
UNITTEST_DIR:=$(MAIN_DIR)/test/unittest
PERFTEST_DIR:=$(MAIN_DIR)/test/performance

SVN_EXE?=svn

all:
	$(MAKE) -C $(UNITTEST_DIR) $@
	$(MAKE) -C $(PERFTEST_DIR) $@

test: all
	$(MAKE) -C $(UNITTEST_DIR) $@
	$(MAKE) -C $(PERFTEST_DIR) $@

clean:
	$(MAKE) -C $(UNITTEST_DIR) $@
	$(MAKE) -C $(PERFTEST_DIR) $@

cleanall: clean
	$(MAKE) -C $(UNITTEST_DIR) $@
	$(MAKE) -C $(PERFTEST_DIR) $@
	$(RM) -r $(MAIN_DIR)/dist
	$(RM) -r $(MAIN_DIR)/test/build


check-release-prerequisite:
	@if [ -z "$(RELVERSION)" ]; then \
		echo "No RELVERSION found, wrong tag name?"; \
		exit 1; \
	fi
