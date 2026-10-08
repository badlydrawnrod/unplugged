Feature: Scoped keyboard protocol enhancement
  The editor requests escape-code disambiguation without reading a support reply
  and restores the previous keyboard mode when its protocol session ends.

  Scenario: Startup requests disambiguation without a support probe
    When a keyboard protocol session starts
    Then only the escape-code disambiguation request is emitted
    And no support query or debug text is emitted

  Scenario: Normal shutdown restores the previous keyboard mode once
    Given an active keyboard protocol session
    When the session is explicitly restored and its scope ends
    Then the previous keyboard mode is restored once

  Scenario: An application failure restores the previous keyboard mode
    Given an active keyboard protocol session
    When an application failure unwinds its scope
    Then the previous keyboard mode is restored

  Scenario: A reset failure is reported without preventing cleanup
    Given an active keyboard protocol session with a broken output pipe
    When the session is explicitly restored
    Then the output failure is reported
    And ending the session scope does not throw
