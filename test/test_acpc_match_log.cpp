#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <test_helper.hpp>

#include <lib/acpc_match_log.hpp>


SCENARIO("This project") {
  GIVEN("Different sized inputs"){
  WHEN("Input is 1") {
    THEN("Result is 1") {
      REQUIRE( Factorial(1) == 1 );
    }
  }
  REQUIRE( Factorial(2) == 2 );
  REQUIRE( Factorial(3) == 6 );
  REQUIRE( Factorial(10) == 3628800 );
}
}

