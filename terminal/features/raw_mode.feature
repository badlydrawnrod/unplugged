Feature: Scoped terminal settings
  Raw-mode sessions restore each terminal's saved settings when their scope ends.

  Scenario: Leaving raw mode restores the previous terminal settings
    Given a terminal has its own flags and read timeout settings
    When I enter raw mode and then leave its scope
    Then all previous terminal settings are restored
    And the borrowed terminal descriptor remains open

  Scenario: An exception restores the terminal before error handling
    Given a terminal's settings are saved by a raw-mode session
    When editor work throws an exception
    Then the terminal settings are restored before the exception is handled

  Scenario: Independent terminal sessions restore their own settings
    Given two terminals have different settings
    When both enter raw mode
    And the first session restores its terminal
    Then the first terminal has its original settings
    And the second terminal remains in raw mode
    When the second session restores its terminal
    Then the second terminal has its own original settings

  Scenario: Raw mode rejects nonterminal input
    Given an input descriptor is not a terminal
    When I request a raw-mode session
    Then acquisition fails with a system error

  Scenario: Restoration failure is reported without throwing during scope exit
    Given a terminal is in raw mode
    And its terminal session is disconnected
    When I explicitly request restoration
    Then restoration reports a system error
    And leaving the raw-mode scope does not throw
