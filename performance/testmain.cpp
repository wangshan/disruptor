#include <iostream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

//TODO: define a new xml listener, based on XmlUnitTestResultPrinter, to print results of repeating runs.
//what the format should look like?

int main(int argc, char** argv) {
  std::cout << "Running main() from gmock_main.cc\n";
  // Since Google Mock depends on Google Test, InitGoogleMock() is
  // also responsible for initializing Google Test.  Therefore there's
  // no need for calling testing::InitGoogleTest() separately.
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
