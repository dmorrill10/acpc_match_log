#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <string>

#include <test_helper.hpp>

#include <lib/acpc_match_log.hpp>
#include <lib/encapsulated_match_state.hpp>

using namespace AcpcMatchLog;
using namespace Acpc;

#include <string>
#include <lib/acpc.hpp>

GameDef new3PlayerLimitKuhnGameDef() {
  const std::string& filePath = std::string(__FILE__);
  const auto pos = filePath.find_last_of("/\\");
  std::string gameDefPath = filePath.substr(0,pos) + "/../vendor/project_acpc_server/kuhn.limit.3p.game";

  return GameDef(gameDefPath);
}
GameDef myGameDef = new3PlayerLimitKuhnGameDef();

SCENARIO("Parsing a log state line into a match state") {
  GIVEN("A log state line") {
    THEN("A match state is returned") {
      const std::string logStateLine = "STATE:2999:crff:Ks|As|Qs:-1|2|-1:Bluffer|HITSZ_CS|hyperborean3pk.RMPUE";
      const Acpc::EncapsulatedMatchState patient(logStateLine, myGameDef);
      REQUIRE(patient.handNum() == (2999 + 1));
      REQUIRE(patient.isFinished());
      REQUIRE(patient.isObserver());
      REQUIRE(!patient.handRevealed(0));
      REQUIRE(!patient.handRevealed(1));
      REQUIRE(!patient.handRevealed(2));
    }
    THEN("The list of player names is returned") {
      const std::string logStateLine = "STATE:2999:crff:Ks|As|Qs:-1|2|-1:Bluffer|HITSZ_CS|hyperborean3pk.RMPUE";
      const std::vector<std::string> patient = players(logStateLine, myGameDef);
      REQUIRE(patient[0] == "Bluffer");
      REQUIRE(patient[1] == "HITSZ_CS");
      REQUIRE(patient[2] == "hyperborean3pk.RMPUE");
    }
  }
}

