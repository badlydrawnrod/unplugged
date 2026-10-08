Feature: Checked terminal input
  Key acquisition distinguishes waiting for input from an ended input stream.

  Scenario: Finite input ends after its last decoded key
    Given input contains text "x" followed by an Up-arrow sequence
    And the stream ends after those bytes
    When I acquire successive keys
    Then I receive text "x", Up, and EOF in order

  Scenario: EOF during a sequence remains EOF
    Given input contains an incomplete Escape sequence
    When the stream ends
    Then key acquisition reports EOF

  Scenario: Acquisition failures report an error
    Given the input descriptor is invalid
    When I acquire a key
    Then acquisition reports a system error

  Scenario: A terminal hangup ends input
    Given a terminal's input session has disconnected
    When I acquire a key
    Then acquisition reports EOF
